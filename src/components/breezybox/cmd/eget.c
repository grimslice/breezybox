/*
 * eget.c - Download ELF binaries from GitHub releases
 * 
 * Usage: eget <user/repo>
 * 
 * Downloads all .elf files from the latest release to /root/bin/
 * The .elf extension is removed from the installed binary name.
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "breezy_vfs.h"

#define MAX_RESPONSE_SIZE   (64 * 1024)  // 64KB for API response
#define MAX_URL_LEN         512
#define MAX_DOWNLOAD_SIZE   (512 * 1024) // 512KB max binary size
#define BIN_DIR             "/root/bin"

#if defined(__riscv)
  #define BREEZY_ARCH "rv32"
#elif defined(__XTENSA__)
  #define BREEZY_ARCH "xtensa"
#else
  #error "eget: unknown breezy target arch"
#endif
#define ARCH_SUFFIX "." BREEZY_ARCH ".elf"

// Buffer for HTTP response
static char *s_response_buf = NULL;
static int s_response_len = 0;
static int s_response_max = 0;

// HTTP event handler
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (s_response_buf && (s_response_len + evt->data_len < s_response_max)) {
            memcpy(s_response_buf + s_response_len, evt->data, evt->data_len);
            s_response_len += evt->data_len;
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

// Fetch URL and return response in buffer (caller must free)
static char *fetch_json(const char *url, int *out_len)
{
    s_response_buf = heap_caps_malloc(MAX_RESPONSE_SIZE, MALLOC_CAP_SPIRAM);
    if (!s_response_buf) {
        printf("eget: out of memory\n");
        return NULL;
    }
    s_response_len = 0;
    s_response_max = MAX_RESPONSE_SIZE;
    
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000,
        .buffer_size = 4096,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(s_response_buf);
        s_response_buf = NULL;
        return NULL;
    }
    
    // Set headers for GitHub API
    esp_http_client_set_header(client, "Accept", "application/vnd.github.v3+json");
    esp_http_client_set_header(client, "User-Agent", "ESP32-BreezyBox");
    
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    
    if (err != ESP_OK || status != 200) {
        printf("eget: HTTP error %d (status %d)\n", err, status);
        free(s_response_buf);
        s_response_buf = NULL;
        return NULL;
    }
    
    // Null-terminate for JSON parsing
    s_response_buf[s_response_len] = '\0';
    
    char *result = s_response_buf;
    s_response_buf = NULL;
    *out_len = s_response_len;
    return result;
}

// Context struct to pass to the event handler
typedef struct {
    FILE *file;
    size_t total_written;
    char *location;          // if set, captures the redirect Location header
    size_t location_size;
} download_ctx_t;

static esp_err_t download_event_handler(esp_http_client_event_t *evt)
{
    download_ctx_t *ctx = (download_ctx_t *)evt->user_data;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_HEADER:
        if (ctx->location && strcasecmp(evt->header_key, "Location") == 0) {
            strlcpy(ctx->location, evt->header_value, ctx->location_size);
        }
        break;
    case HTTP_EVENT_ON_DATA:
        if (ctx->file && evt->data_len > 0) {
            size_t written = fwrite(evt->data, 1, evt->data_len, ctx->file);
            ctx->total_written += written;
            // Optional: Print progress dots
            // if (ctx->total_written % 10240 == 0) printf("."); 
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

// One shared client per phase keeps the TLS connection (and its certificate
// validation) alive across requests instead of handshaking twice per file.
static esp_http_client_handle_t asset_client_init(const char *url, download_ctx_t *ctx,
                                                  bool follow_redirects)
{
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = download_event_handler,
        .user_data = ctx,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 60000,
        .disable_auto_redirect = !follow_redirects,
        .max_redirection_count = follow_redirects ? 5 : 0,
        .buffer_size = 4096,
        .buffer_size_tx = 2048,  // holds long signed asset URLs
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client) {
        esp_http_client_set_header(client, "Accept", "application/octet-stream");
        esp_http_client_set_header(client, "User-Agent", "ESP32-BreezyBox");
    }
    return client;
}

static int download_file(esp_http_client_handle_t client, const char *url,
                         const char *dest_path)
{
    printf("  Downloading to %s...\n", dest_path);

    FILE *f = fopen(dest_path, "wb");
    if (!f) {
        printf("eget: cannot create file\n");
        return -1;
    }

    download_ctx_t ctx = { .file = f, .total_written = 0 };
    esp_http_client_set_user_data(client, &ctx);
    esp_http_client_set_url(client, url);

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    fclose(f);

    if (err != ESP_OK || status != 200) {
        printf("eget: download failed (err=%d, status=%d)\n", err, status);
        unlink(dest_path); // Delete partial file
        return -1;
    }

    printf("  Success (%u bytes)\n", (unsigned)ctx.total_written);
    return 0;
}

// cJSON allocation hooks: the GitHub API response parses into ~100KB of small
// nodes, which would exhaust internal SRAM and starve wifi/TLS. Keep the tree
// in PSRAM instead. (free() handles heap_caps memory, so restoring the default
// hooks later is safe.)
static void *psram_malloc(size_t sz)
{
    return heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
}

// True if some network interface is up with an IPv4 address
static bool is_connected(void)
{
    esp_netif_t *netif = esp_netif_get_default_netif();
    esp_netif_ip_info_t ip;
    return netif != NULL &&
           esp_netif_is_netif_up(netif) &&
           esp_netif_get_ip_info(netif, &ip) == ESP_OK &&
           ip.ip.addr != 0;
}

// Copy name with the arch suffix removed
static void strip_arch_suffix(const char *name, char *out, size_t out_size)
{
    strncpy(out, name, out_size - 1);
    out[out_size - 1] = '\0';

    size_t len = strlen(out);
    size_t sfx = strlen(ARCH_SUFFIX);
    if (len > sfx) {
        out[len - sfx] = '\0';
    }
}

int cmd_eget(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: eget <user/repo>\n");
        printf("  Downloads .elf files from latest GitHub release to %s/\n", BIN_DIR);
        return 1;
    }
    
    const char *repo = argv[1];

    // Validate repo format
    if (strchr(repo, '/') == NULL || repo[0] == '/' || repo[strlen(repo)-1] == '/') {
        printf("eget: invalid repo format, use 'user/repo'\n");
        return 1;
    }

    if (!is_connected()) {
        printf("eget: no network connection (try 'wifi' first)\n");
        return 1;
    }

    // Create bin directory if it doesn't exist
    mkdir(BIN_DIR, 0755);
    
    // Build API URL
    char url[MAX_URL_LEN];
    snprintf(url, sizeof(url), "https://api.github.com/repos/%s/releases/latest", repo);
    
    printf("Fetching %s...\n", repo);
    
    // Fetch release info
    int len;
    char *json = fetch_json(url, &len);
    if (!json) {
        printf("eget: failed to fetch release info\n");
        return 1;
    }
    
    // Parse JSON with the tree in PSRAM, extract what we need, and free the
    // tree before downloading: holding it in internal SRAM starved the HTTP
    // client and wifi of memory during downloads.
    cJSON_Hooks psram_hooks = { .malloc_fn = psram_malloc, .free_fn = free };
    cJSON_InitHooks(&psram_hooks);
    cJSON *root = cJSON_Parse(json);
    free(json);

    if (!root) {
        cJSON_InitHooks(NULL);
        printf("eget: failed to parse response\n");
        return 1;
    }

    // Check for error message (e.g., rate limit, not found)
    cJSON *message = cJSON_GetObjectItem(root, "message");
    if (message && cJSON_IsString(message)) {
        printf("eget: %s\n", message->valuestring);
        cJSON_Delete(root);
        cJSON_InitHooks(NULL);
        return 1;
    }

    // Get tag name
    cJSON *tag = cJSON_GetObjectItem(root, "tag_name");
    if (tag && cJSON_IsString(tag)) {
        printf("Latest release: %s\n", tag->valuestring);
    }

    // Get assets array
    cJSON *assets = cJSON_GetObjectItem(root, "assets");
    if (!assets || !cJSON_IsArray(assets)) {
        printf("eget: no assets in release\n");
        cJSON_Delete(root);
        cJSON_InitHooks(NULL);
        return 1;
    }

    // Collect matching assets into a PSRAM list
    typedef struct {
        char name[64];
        char url[1536];  // signed asset URLs run long
    } asset_t;

    int count = cJSON_GetArraySize(assets);
    asset_t *list = heap_caps_calloc(count > 0 ? count : 1, sizeof(asset_t),
                                     MALLOC_CAP_SPIRAM);
    int found = 0;

    for (int i = 0; list && i < count; i++) {
        cJSON *asset = cJSON_GetArrayItem(assets, i);
        cJSON *name = cJSON_GetObjectItem(asset, "name");
        cJSON *download_url = cJSON_GetObjectItem(asset, "browser_download_url");

        if (!name || !cJSON_IsString(name) || !download_url || !cJSON_IsString(download_url)) {
            continue;
        }

        const char *asset_name = name->valuestring;
        size_t name_len = strlen(asset_name);
        size_t sfx_len = strlen(ARCH_SUFFIX);

        // Only this platform's binaries: <name>.<arch>.elf
        if (name_len <= sfx_len ||
            strcasecmp(asset_name + name_len - sfx_len, ARCH_SUFFIX) != 0) {
            continue;
        }
        if (strlen(download_url->valuestring) >= sizeof(list[found].url)) {
            continue;
        }

        strip_arch_suffix(asset_name, list[found].name, sizeof(list[found].name));
        strcpy(list[found].url, download_url->valuestring);
        printf("Found: %s\n", asset_name);
        found++;
    }

    cJSON_Delete(root);
    cJSON_InitHooks(NULL);

    if (!list) {
        printf("eget: out of memory\n");
        return 1;
    }

    // Resolve github.com redirects to the signed asset URLs on one connection.
    // Capture Location ourselves: set_redirection would repoint the client at
    // the asset host and break connection reuse, and the client's redirect
    // counter never resets across requests.
    download_ctx_t redir_ctx = { 0 };  // no file: discards the redirect bodies
    esp_http_client_handle_t client =
        found ? asset_client_init(list[0].url, &redir_ctx, false) : NULL;
    for (int i = 0; client && i < found; i++) {
        redir_ctx.location = list[i].url;
        redir_ctx.location_size = sizeof(list[i].url);
        esp_http_client_set_url(client, list[i].url);
        esp_http_client_reset_redirect_counter(client);
        esp_http_client_perform(client);
        // on any failure list[i].url is untouched; downloads fall back to it
    }
    if (client) {
        esp_http_client_cleanup(client);
    }

    // Download the files, again sharing one connection (all assets live on the
    // same host; set_url reconnects automatically if one doesn't)
    int downloaded = 0;
    client = found ? asset_client_init(list[0].url, NULL, true) : NULL;
    for (int i = 0; client && i < found; i++) {
        char dest_path[128];
        snprintf(dest_path, sizeof(dest_path), "%s/%s", BIN_DIR, list[i].name);
        if (download_file(client, list[i].url, dest_path) == 0) {
            downloaded++;
        }
    }
    if (client) {
        esp_http_client_cleanup(client);
    }
    free(list);

    if (downloaded == 0) {
        printf("eget: no %s binaries found in release\n", ARCH_SUFFIX);
        return 1;
    }
    
    printf("Done. Installed %d binary(s) to %s/\n", downloaded, BIN_DIR);
    return 0;
}
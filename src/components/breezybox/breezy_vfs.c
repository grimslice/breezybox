#include "breezy_vfs.h"
#include "esp_littlefs.h"
#include "esp_vfs.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static char s_cwd[BREEZYBOX_MAX_PATH + 1] = BREEZYBOX_MOUNT_POINT;

void breezybox_get_cwd(char *buf, size_t size)
{
    if (size > 0) {
        strncpy(buf, s_cwd, size - 1);
        buf[size - 1] = '\0';
    }
}

const char *breezybox_cwd(void)
{
    return s_cwd;
}

int breezybox_set_cwd(const char *path)
{
    char new_path[BREEZYBOX_MAX_PATH + 1];

    if (strcmp(path, "..") == 0) {
        if (strcmp(s_cwd, "/") == 0) return 0;
        strncpy(new_path, s_cwd, sizeof(new_path));
        char *last_slash = strrchr(new_path, '/');
        if (last_slash == new_path) {
            new_path[1] = '\0';
        } else if (last_slash) {
            *last_slash = '\0';
        }
    } else if (path[0] == '/') {
        strncpy(new_path, path, sizeof(new_path) - 1);
        new_path[sizeof(new_path) - 1] = '\0';
    } else {
        size_t cwd_len = strlen(s_cwd);
        size_t path_len = strlen(path);
        if (cwd_len + 1 + path_len >= sizeof(new_path)) return -1;

        strcpy(new_path, s_cwd);
        if (cwd_len > 1) {
            new_path[cwd_len] = '/';
            strcpy(new_path + cwd_len + 1, path);
        } else {
            strcpy(new_path + 1, path);
        }
    }

    struct stat st;
    if (strcmp(new_path, "/") == 0 ||
        (stat(new_path, &st) == 0 && S_ISDIR(st.st_mode))) {
        strcpy(s_cwd, new_path);
        return 0;
    }
    return -1;
}

char *breezybox_resolve_path(const char *path, char *buf, size_t size)
{
    if (path[0] == '/') {
        strncpy(buf, path, size - 1);
        buf[size - 1] = '\0';
    } else {
        size_t cwd_len = strlen(s_cwd);
        size_t path_len = strlen(path);
        if (cwd_len + 1 + path_len >= size) return NULL;
        
        if (cwd_len == 1 && s_cwd[0] == '/') {
            snprintf(buf, size, "/%s", path);
        } else {
            snprintf(buf, size, "%s/%s", s_cwd, path);
        }
    }
    return buf;
}

// ---- /dev/null ---------------------------------------------------------
// The shell's redirections (`>/dev/null`, fd-close) fopen this path, and it
// doesn't exist in the ESP VFS by default: writes are swallowed, reads EOF.

static ssize_t devnull_write(int fd, const void *data, size_t size)
{
    (void)fd; (void)data;
    return size;
}

static ssize_t devnull_read(int fd, void *data, size_t size)
{
    (void)fd; (void)data; (void)size;
    return 0;   // EOF
}

static int devnull_open(const char *path, int flags, int mode)
{
    (void)path; (void)flags; (void)mode;
    return 0;
}

static int devnull_close(int fd) { (void)fd; return 0; }

static int devnull_fstat(int fd, struct stat *st)
{
    (void)fd;
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFCHR;
    return 0;
}

static esp_err_t devnull_register(void)
{
    esp_vfs_t vfs = {
        .flags = ESP_VFS_FLAG_DEFAULT,
        .write = devnull_write,
        .read  = devnull_read,
        .open  = devnull_open,
        .close = devnull_close,
        .fstat = devnull_fstat,
    };
    return esp_vfs_register("/dev/null", &vfs, NULL);
}

esp_err_t breezybox_vfs_init(void)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = BREEZYBOX_MOUNT_POINT,
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            printf("Failed to mount or format LittleFS\n");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            printf("No 'storage' partition found\n");
        }
        return ret;
    }

    size_t total = 0, used = 0;
    esp_littlefs_info("storage", &total, &used);
    printf("LittleFS: %d KB total, %d KB used\n", (int)(total / 1024), (int)(used / 1024));

    strcpy(s_cwd, BREEZYBOX_MOUNT_POINT);

    ret = devnull_register();
    if (ret != ESP_OK) {
        printf("Failed to register /dev/null: %s (raise CONFIG_VFS_MAX_COUNT?)\n",
               esp_err_to_name(ret));
    } else {
        FILE *t = fopen("/dev/null", "w");
        printf("/dev/null self-test: %s\n", t ? "ok" : "FAILED");
        if (t) fclose(t);
    }
    return ESP_OK;
}
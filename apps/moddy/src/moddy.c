/*
 * moddy - Amiga MOD player with spectrum + tracker visualization, on
 * breezy_tui. Runs on Mac (SDL2 audio via ../host_sdl.c) and as a
 * BreezyBox ELF app on ESP32-S3/P4 (snd_* from the firmware).
 *
 *   moddy <file.mod | directory>
 *
 *   Space       pause / resume
 *   Up / Down   previous / next .mod in the same directory
 *   Left/Right  volume down / up
 *   q / Esc / Ctrl-C   quit
 *
 * The current song loops until the user switches or quits. All keys come
 * through breezy_tui's stdin decoder only: on the device the built-in
 * keyboard is also visible via bt_keyboard_*, but polling both sources
 * made toggle keys (pause) fire twice, so the bt_ path is not used.
 *
 * Single loop, audio first: keep the PCM ring topped up (capturing a state
 * snapshot per chunk), then poll keys and redraw at ~30 fps. The displayed
 * snapshot is the one matching the audio the speaker is playing *now*
 * (frames_written - frames_queued), so visuals track the audible beat, not
 * the decoder, which runs ~93 ms ahead.
 *
 * Portability rule (same as breezybox modplay): only the exported symbol
 * surface below (snd_*, vTaskDelay) plus libc, dirent and breezy_tui.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define POCKETMOD_IMPLEMENTATION
#include "pocketmod.h"

#include "mp_mod.h"
#include "mp_view.h"
#include "mp_vis.h"
#include "tui_core.h"
#include "tui_input.h"
#include "tui_term.h"

/* ---- Firmware API (elf_loader symbols on device, host_sdl.c on Mac) ---- */

int  snd_init(void);
int  snd_stream_open(int rate, int channels);
int  snd_stream_space(void);
int  snd_stream_write(const int16_t *frames, int nframes);
void snd_stream_close(void);
void snd_set_volume(int pct);   /* 0..100 percent */

typedef uint32_t TickType_t;
void vTaskDelay(TickType_t ticks);

#define SAMPLE_RATE  44100
#define CHUNK_FRAMES 1024
#define DRAW_EVERY_FRAMES (SAMPLE_RATE / 30) /* ~30 fps, audio-clocked */

#define VOL_STEP 10
#define VOL_MAX  100

#define MAX_TRACKS 128
#define PATH_MAX_LEN 256

static pocketmod_context g_ctx; /* too big for the app stack */
static float   g_fbuf[CHUNK_FRAMES * 2];
static int16_t g_ibuf[CHUNK_FRAMES * 2];
static mp_mod g_mod;
static mp_snap_ring g_ring;
static mp_vis g_vis;

/* --- playlist: all *.mod files in one directory, sorted --- */

static char g_dir[PATH_MAX_LEN];
static char g_names[MAX_TRACKS][64];
static int g_ntracks;

static int lc(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int name_cmp(const char *a, const char *b)
{
    while (*a && lc(*a) == lc(*b)) { a++; b++; }
    return lc(*a) - lc(*b);
}

static bool is_mod_name(const char *n)
{
    size_t len = strlen(n);
    return len > 4 && name_cmp(n + len - 4, ".mod") == 0;
}

static void scan_dir(const char *dir)
{
    g_ntracks = 0;
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && g_ntracks < MAX_TRACKS) {
        if (!is_mod_name(e->d_name) ||
            strlen(e->d_name) >= sizeof(g_names[0]))
            continue;
        strcpy(g_names[g_ntracks++], e->d_name);
    }
    closedir(d);
    /* Insertion sort: tiny n, and no qsort dependency on the device. */
    for (int i = 1; i < g_ntracks; i++) {
        char tmp[64];
        strcpy(tmp, g_names[i]);
        int j = i;
        while (j > 0 && name_cmp(g_names[j - 1], tmp) > 0) {
            strcpy(g_names[j], g_names[j - 1]);
            j--;
        }
        strcpy(g_names[j], tmp);
    }
}

static void *load_file(const char *path, int *size_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *buf = (size > 0) ? malloc(size) : NULL;
    if (buf && fread(buf, 1, size, f) != (size_t)size) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    *size_out = (int)size;
    return buf;
}

/* Load playlist entry `idx`; replaces *data. False if unplayable. */
static bool load_track(int idx, void **data)
{
    char path[PATH_MAX_LEN + 80];
    snprintf(path, sizeof(path), "%s/%s", g_dir, g_names[idx]);
    int size = 0;
    void *buf = load_file(path, &size);
    if (!buf) return false;
    if (!pocketmod_init(&g_ctx, buf, size, SAMPLE_RATE) ||
        !mp_mod_parse(&g_mod, buf, size)) {
        free(buf);
        return false;
    }
    free(*data);
    *data = buf;
    memset(&g_ring, 0, sizeof(g_ring));
    g_vis.last_pattern = g_vis.last_line = -1;
    return true;
}

/* The firmware mixer owns master volume; moddy just hands it a normalized
 * full-scale int16 signal and pushes g_vol on change (kept for the UI). */

static int g_vol = 100;

static inline int16_t sample_out(float x)
{
    if (x > 1.0f) x = 1.0f;
    else if (x < -1.0f) x = -1.0f;
    return (int16_t)(x * 32767.0f);
}

static void capture_snap(uint32_t pos)
{
    mp_snap s = {0};
    s.pos = pos;
    s.pattern = g_ctx.pattern;
    s.line = g_ctx.line;
    int nch = g_ctx.num_channels < MP_VIS_CHANNELS ? g_ctx.num_channels
                                                   : MP_VIS_CHANNELS;
    for (int c = 0; c < nch; c++) {
        s.period[c] = g_ctx.channels[c].period;
        s.volume[c] = g_ctx.channels[c].real_volume;
        s.sample[c] = g_ctx.channels[c].sample;
    }
    mp_snap_push(&g_ring, &s);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: moddy <file.mod | directory>\n");
        return 1;
    }

    /* Resolve target: directory -> first .mod inside; file -> that file,
     * with its siblings as the hidden playlist. */
    struct stat st;
    int cur = 0;
    if (stat(argv[1], &st) == 0 && S_ISDIR(st.st_mode)) {
        snprintf(g_dir, sizeof(g_dir), "%s", argv[1]);
        scan_dir(g_dir);
    } else {
        const char *slash = strrchr(argv[1], '/');
        const char *base = slash ? slash + 1 : argv[1];
        if (slash)
            snprintf(g_dir, sizeof(g_dir), "%.*s",
                     (int)(slash - argv[1]), argv[1]);
        else
            snprintf(g_dir, sizeof(g_dir), ".");
        scan_dir(g_dir);
        for (int i = 0; i < g_ntracks; i++)
            if (name_cmp(g_names[i], base) == 0) { cur = i; break; }
    }
    if (g_ntracks == 0) {
        printf("moddy: no .mod files found at %s\n", argv[1]);
        return 1;
    }

    void *data = NULL;
    if (!load_track(cur, &data)) {
        printf("moddy: cannot play %s/%s\n", g_dir, g_names[cur]);
        return 1;
    }

    if (snd_init() != 0) {
        printf("moddy: sound init failed\n");
        free(data);
        return 1;
    }
    if (snd_stream_open(SAMPLE_RATE, 2) != 0) {
        printf("moddy: stream busy or unsupported\n");
        free(data);
        return 1;
    }
    snd_set_volume(g_vol);
    if (tui_init() != 0) {
        snd_stream_close();
        printf("moddy: tui init failed (not a tty?)\n");
        free(data);
        return 1;
    }

    tui_surface *scr = tui_screen();
    mp_vis_init(&g_vis, mp_view_spectrum_bins(scr->w));

    int ring_frames = snd_stream_space(); /* empty ring = full space */
    uint32_t written = 0;   /* total frames handed to the stream */
    uint32_t next_draw = 0; /* play position of the next redraw */
    uint32_t tick = 0;
    bool paused = false, quit = false;

    while (!quit) {
        /* --- audio first (the song loops; no natural end) --- */
        if (!paused) {
            int space = snd_stream_space();
            if (space > CHUNK_FRAMES) space = CHUNK_FRAMES;
            if (space >= 256) {
                capture_snap(written);
                int bytes = pocketmod_render(
                    &g_ctx, g_fbuf, space * (int)POCKETMOD_SAMPLE_SIZE);
                int frames = bytes / (int)POCKETMOD_SAMPLE_SIZE;
                for (int i = 0; i < frames * 2; i++)
                    g_ibuf[i] = sample_out(g_fbuf[i]);
                written += (uint32_t)snd_stream_write(g_ibuf, frames);
            }
        }

        uint32_t queued = (uint32_t)(ring_frames - snd_stream_space());
        uint32_t play_pos = written > queued ? written - queued : 0;

        /* --- input (stdin only; see header comment) --- */
        tui_key k = tui_read_key(0);
        int switch_to = -1;
        if (k.kind == TUI_KEY_CHAR &&
            (k.ch == 'q' || (k.ctrl && k.ch == 'c')))
            quit = true;
        else if (k.kind == TUI_KEY_ESC)
            quit = true;
        else if (k.kind == TUI_KEY_CHAR && k.ch == ' ')
            paused = !paused;
        else if (k.kind == TUI_KEY_LEFT && g_vol > 0) {
            g_vol -= VOL_STEP;
            snd_set_volume(g_vol);
        }
        else if (k.kind == TUI_KEY_RIGHT && g_vol < VOL_MAX) {
            g_vol += VOL_STEP;
            snd_set_volume(g_vol);
        }
        else if (k.kind == TUI_KEY_UP)
            switch_to = (cur - 1 + g_ntracks) % g_ntracks;
        else if (k.kind == TUI_KEY_DOWN)
            switch_to = (cur + 1) % g_ntracks;

        if (switch_to >= 0 && switch_to != cur) {
            /* Skip unplayable files, at most one full lap. */
            int step = switch_to == (cur + 1) % g_ntracks ? 1 : -1;
            for (int tries = 0; tries < g_ntracks; tries++) {
                if (load_track(switch_to, &data)) {
                    cur = switch_to;
                    paused = false;
                    break;
                }
                switch_to = (switch_to + step + g_ntracks) % g_ntracks;
                if (switch_to == cur) break;
            }
        }

        /* --- draw, paced by the audio clock (ticks while paused) --- */
        tick++;
        if (play_pos >= next_draw || (paused && tick % 3 == 0)) {
            next_draw = play_pos + DRAW_EVERY_FRAMES;
            const mp_snap *snap = mp_snap_at(&g_ring, play_pos);
            mp_vis_step(&g_vis, &g_mod, paused ? NULL : snap);
            tui_frame_begin(scr);
            mp_view_status status = {g_names[cur], cur + 1, g_ntracks,
                                     g_vol, paused};
            mp_view_draw(scr, tui_surface_rect(scr), &g_mod, &status,
                         &g_vis, snap);
            tui_present();
        }

        vTaskDelay(1); /* 10 ms tick */
    }

    tui_shutdown();
    snd_stream_close();
    free(data);
    printf("moddy: bye\n");
    return 0;
}

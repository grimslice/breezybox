/*
 * modplay - CLI Amiga MOD player (BreezyBox ELF app + Mac host build).
 *
 *   modplay <file.mod>
 *
 * Plays the song once and exits at the loop point, or on Space / Esc /
 * Ctrl-C. Decoding is pocketmod (vendored single header); output goes to
 * the firmware's snd_stream_* PCM API, mixed alongside the synth voices.
 *
 * Portability rule: this file talks only to the exported symbol surface
 * below (snd_*, bt_keyboard_*, vTaskDelay) plus libc -- no ESP-IDF headers.
 * On device those symbols come from the firmware's elf_loader table; on the
 * Mac the same symbols are provided by host_sdl.c over SDL2 (see
 * buildhost.sh), so this file compiles unchanged for both.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define POCKETMOD_IMPLEMENTATION
#include "pocketmod.h"

/* ---- Firmware API (elf_loader symbols on device, host_sdl.c on Mac) ---- */

int  snd_init(void);
int  snd_stream_open(int rate, int channels);
int  snd_stream_space(void);
int  snd_stream_write(const int16_t *frames, int nframes);
void snd_stream_close(void);

int bt_keyboard_is_pressed(uint8_t keycode);
int bt_keyboard_connected(void);

typedef uint32_t TickType_t;
void vTaskDelay(TickType_t ticks);

/* USB-HID usage codes */
#define KEY_SPACE 0x2C
#define KEY_ESC   0x29

#define SAMPLE_RATE  44100
#define CHUNK_FRAMES 1024   /* per render call; well under the fw ring */

static pocketmod_context g_ctx;   /* too big for the app stack */
static float   g_fbuf[CHUNK_FRAMES * 2];
static int16_t g_ibuf[CHUNK_FRAMES * 2];

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

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: modplay <file.mod>\n");
        return 1;
    }
    const char *path = argv[1];

    int size = 0;
    void *data = load_file(path, &size);
    if (!data) {
        printf("modplay: cannot read %s\n", path);
        return 1;
    }
    if (!pocketmod_init(&g_ctx, data, size, SAMPLE_RATE)) {
        printf("modplay: %s is not a valid MOD file\n", path);
        free(data);
        return 1;
    }

    /* Song title: bytes 0-19, not always NUL-terminated. */
    char title[21] = {0};
    memcpy(title, data, 20);
    for (int i = 0; i < 20; i++) {
        if (title[i] != 0 && (title[i] < 32 || title[i] > 126)) title[i] = '?';
    }
    printf("modplay: \"%s\" - %d channels, %d patterns, %d bytes\n",
           title, g_ctx.num_channels, g_ctx.num_patterns, size);
    printf("Space/Esc/Ctrl-C to stop\n");

    if (snd_init() != 0) {
        printf("modplay: sound init failed\n");
        free(data);
        return 1;
    }
    if (snd_stream_open(SAMPLE_RATE, 2) != 0) {
        printf("modplay: stream busy or unsupported\n");
        free(data);
        return 1;
    }

    /* Non-blocking stdin: Space/Ctrl-C from the console; raw key polling
     * (bt_keyboard) catches Space/Esc on the built-in keyboard. */
    int old_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, old_flags | O_NONBLOCK);
    bool have_keys = bt_keyboard_connected();
    int ring_frames = snd_stream_space();   /* empty ring = full space */

    int running = 1;
    bool ended = false;
    while (running) {
        if (pocketmod_loop_count(&g_ctx) > 0) { ended = true; break; }
        if (have_keys && (bt_keyboard_is_pressed(KEY_SPACE) ||
                          bt_keyboard_is_pressed(KEY_ESC))) break;
        char ch;
        while (read(STDIN_FILENO, &ch, 1) == 1) {
            if (ch == ' ' || ch == 3 || ch == 27) { running = 0; break; }
        }
        if (!running) break;

        int space = snd_stream_space();
        if (space > CHUNK_FRAMES) space = CHUNK_FRAMES;
        if (space >= 256) {
            /* pocketmod may render less than asked (tick boundaries). */
            int bytes = pocketmod_render(&g_ctx, g_fbuf,
                                         space * (int)POCKETMOD_SAMPLE_SIZE);
            int frames = bytes / (int)POCKETMOD_SAMPLE_SIZE;
            for (int i = 0; i < frames * 2; i++) {
                float s = g_fbuf[i] * 32767.0f;
                if (s > 32767.0f) s = 32767.0f;
                else if (s < -32768.0f) s = -32768.0f;
                g_ibuf[i] = (int16_t)s;
            }
            snd_stream_write(g_ibuf, frames);
        }
        vTaskDelay(1);
    }

    /* Natural end: let the buffered tail play out before closing. */
    while (ended && snd_stream_space() < ring_frames) {
        vTaskDelay(1);
    }

    fcntl(STDIN_FILENO, F_SETFL, old_flags);
    snd_stream_close();
    free(data);
    printf("modplay: bye\n");
    return 0;
}

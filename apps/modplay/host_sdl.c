/*
 * host_sdl.c - Mac/desktop implementation of the BreezyBox symbols that
 * modplay.c uses, over SDL2 audio + termios. Lets the same app source run
 * on the host for fast iteration (see buildhost.sh).
 *
 * Mapping:
 *   snd_stream_open/space/write/close -> SDL_OpenAudioDevice (queue mode,
 *       no callback) + SDL_QueueAudio/SDL_GetQueuedAudioSize. "space" is
 *       emulated as (RING_FRAMES - queued), mirroring the firmware ring.
 *   vTaskDelay        -> SDL_Delay (1 tick ~ 10 ms, like the device).
 *   bt_keyboard_*     -> stubs (no window = no SDL key events); modplay
 *       falls back to stdin, which snd_init puts in raw mode (restored
 *       at exit) so Space works without Enter.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include <SDL.h>   /* sdl2-config already points inside include/SDL2 */

#define RING_FRAMES 4096   /* match the firmware's stream ring */

static SDL_AudioDeviceID g_dev = 0;
static int g_channels = 2;
static struct termios g_saved_termios;
static int g_termios_saved = 0;

static void restore_termios(void)
{
    if (g_termios_saved) tcsetattr(STDIN_FILENO, TCSANOW, &g_saved_termios);
}

int snd_init(void)
{
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return -1;
    }
    if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &g_saved_termios) == 0) {
        struct termios raw = g_saved_termios;
        raw.c_lflag &= ~(ICANON | ECHO);   /* keys arrive without Enter */
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        g_termios_saved = 1;
        atexit(restore_termios);
    }
    return 0;
}

int snd_stream_open(int rate, int channels)
{
    if (g_dev || (channels != 1 && channels != 2)) return -1;
    SDL_AudioSpec want = {0}, have;
    want.freq = rate;
    want.format = AUDIO_S16SYS;
    want.channels = (Uint8)channels;
    want.samples = 1024;
    g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!g_dev) {
        fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return -1;
    }
    g_channels = channels;
    SDL_PauseAudioDevice(g_dev, 0);
    return 0;
}

int snd_stream_space(void)
{
    if (!g_dev) return 0;
    int queued = (int)(SDL_GetQueuedAudioSize(g_dev) / (sizeof(int16_t) * g_channels));
    return queued >= RING_FRAMES ? 0 : RING_FRAMES - queued;
}

int snd_stream_write(const int16_t *frames, int nframes)
{
    if (!g_dev || !frames || nframes < 0) return -1;
    int space = snd_stream_space();
    if (nframes > space) nframes = space;
    SDL_QueueAudio(g_dev, frames, (Uint32)(nframes * sizeof(int16_t) * g_channels));
    return nframes;
}

void snd_stream_close(void)
{
    if (!g_dev) return;
    SDL_CloseAudioDevice(g_dev);
    g_dev = 0;
}

/* No SDL window, so no SDL key events: report "no keyboard" and let the
 * app use raw stdin instead. */
int bt_keyboard_is_pressed(uint8_t keycode) { (void)keycode; return 0; }
int bt_keyboard_connected(void)             { return 0; }

void vTaskDelay(uint32_t ticks)
{
    SDL_Delay(ticks * 10);   /* device tick is 10 ms */
}

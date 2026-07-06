/*
 * soundkeys - Play the keyboard as a music keyboard (BreezyBox ELF app).
 *
 * Piano map on the bottom row (like a tracker / DAW):
 *
 *   S D   G H J        <- black keys (C# D#   F# G# A#)
 *  Z X C V B N M ,     <- white keys (C D E F G A B C+1)
 *
 * 1-8 select the waveform, Up/Down arrows shift the octave, Left/Right
 * adjust volume, Esc exits.
 *
 * Input: the bt_keyboard_* polling API exported by the firmware -- a real
 * BLE keyboard on the S3 build, the built-in key matrix (shimmed under the
 * same symbols) on the Tanmatsu. Sound: the snd_* mixer API (snd_core.c in
 * the example firmwares). Self-contained: no libm (semitone lookup table
 * instead of powf), no headers beyond libc.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>

/* ---- Firmware API (resolved from the elf_loader symbol table) ---- */

/* snd_core.h */
int  snd_init(void);
int  snd_note_on(int voice, float freq_hz, int wave, int vol);
void snd_note_off(int voice);
void snd_all_off(void);
void snd_set_volume(int pct);

enum {
    SND_WAVE_TRIANGLE = 0, SND_WAVE_TILTED_SAW, SND_WAVE_SAW, SND_WAVE_SQUARE,
    SND_WAVE_PULSE, SND_WAVE_ORGAN, SND_WAVE_NOISE, SND_WAVE_PHASER,
    SND_WAVE_EPIANO,
};

/* bt_keyboard.h (polling shim on Tanmatsu, real BLE on S3) */
int bt_keyboard_is_pressed(uint8_t keycode);
int bt_keyboard_connected(void);

/* freertos */
typedef uint32_t TickType_t;
void vTaskDelay(TickType_t ticks);

/* USB-HID usage codes (bt_keyboard.h BT_KEY_*) */
#define KEY_S      0x16
#define KEY_D      0x07
#define KEY_G      0x0A
#define KEY_H      0x0B
#define KEY_J      0x0D
#define KEY_Z      0x1D
#define KEY_X      0x1B
#define KEY_C      0x06
#define KEY_V      0x19
#define KEY_B      0x05
#define KEY_N      0x11
#define KEY_M      0x10
#define KEY_COMMA  0x36
#define KEY_1      0x1E   /* 1..8 are consecutive */
#define KEY_ESC    0x29
#define KEY_RIGHT  0x4F
#define KEY_LEFT   0x50
#define KEY_DOWN   0x51
#define KEY_UP     0x52

#define NUM_KEYS 13

typedef struct {
    uint8_t keycode;     /* HID usage code */
    int semitone;        /* offset from C in the current octave */
} piano_key_t;

static const piano_key_t k_keys[NUM_KEYS] = {
    { KEY_Z, 0 },  { KEY_S, 1 },
    { KEY_X, 2 },  { KEY_D, 3 },
    { KEY_C, 4 },
    { KEY_V, 5 },  { KEY_G, 6 },
    { KEY_B, 7 },  { KEY_H, 8 },
    { KEY_N, 9 },  { KEY_J, 10 },
    { KEY_M, 11 },
    { KEY_COMMA, 12 },
};

/* Keys 1-8 -> synth waveform. Noise is skipped (pointless melodically);
 * slot 7 gets the Rhodes-ish e-piano instead. */
static const int k_wave_map[8] = {
    SND_WAVE_TRIANGLE, SND_WAVE_TILTED_SAW, SND_WAVE_SAW, SND_WAVE_SQUARE,
    SND_WAVE_PULSE, SND_WAVE_ORGAN, SND_WAVE_EPIANO, SND_WAVE_PHASER,
};

static const char *k_wave_names[8] = {
    "triangle", "tilted saw", "saw", "square",
    "pulse", "organ", "e-piano", "phaser",
};

/* C4..C5 in Hz; other octaves by powers of two. No libm needed. */
static const float k_c4_freq[13] = {
    261.63f, 277.18f, 293.66f, 311.13f, 329.63f, 349.23f, 369.99f,
    392.00f, 415.30f, 440.00f, 466.16f, 493.88f, 523.25f,
};

static float note_freq(int octave, int semitone)
{
    float f = k_c4_freq[semitone];
    for (int o = octave; o < 4; o++) f *= 0.5f;
    for (int o = octave; o > 4; o--) f *= 2.0f;
    return f;
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    if (snd_init() != 0) {
        printf("Sound init failed\n");
        return 1;
    }
    if (!bt_keyboard_connected()) {
        printf("No keyboard for raw key state (BT: try btconnect)\n");
        return 1;
    }

    int wave = 0;                   /* index into k_wave_map/k_wave_names */
    int octave = 4;                 /* Z = C4 (middle C) */
    int volume = 100;               /* master volume percent */
    int voice_of[NUM_KEYS];
    bool held[NUM_KEYS] = { false };
    bool wave_held[8] = { false };
    bool oct_up_held = false, oct_down_held = false;
    bool vol_up_held = false, vol_down_held = false;
    for (int i = 0; i < NUM_KEYS; i++) voice_of[i] = -1;
    snd_set_volume(volume);

    printf("soundkeys: Z-row = piano (S D G H J = black keys), , = high C\n");
    printf("1-8 = waveform, Up/Down = octave, Left/Right = volume, Esc = exit\n");
    printf("[%s] octave %d\n", k_wave_names[wave], octave);

    /* Poll stdin non-blocking: drain chars the console buffered while we
     * poll raw key state; Ctrl-C works as an alternate exit from the USB
     * console. Restored on exit. */
    int old_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, old_flags | O_NONBLOCK);

    int running = 1;
    while (running) {
        if (bt_keyboard_is_pressed(KEY_ESC)) break;
        char ch;
        while (read(STDIN_FILENO, &ch, 1) == 1) {
            if (ch == 3) { running = 0; break; }
        }
        if (!running) break;

        /* Piano keys: edge-triggered note on/off from held-key state. */
        for (int i = 0; i < NUM_KEYS; i++) {
            bool down = bt_keyboard_is_pressed(k_keys[i].keycode);
            if (down && !held[i]) {
                voice_of[i] = snd_note_on(-1, note_freq(octave, k_keys[i].semitone),
                                          k_wave_map[wave], 220);
            } else if (!down && held[i] && voice_of[i] >= 0) {
                snd_note_off(voice_of[i]);
                voice_of[i] = -1;
            }
            held[i] = down;
        }

        /* Waveform select. */
        for (int w = 0; w < 8; w++) {
            bool down = bt_keyboard_is_pressed(KEY_1 + w);
            if (down && !wave_held[w] && w != wave) {
                wave = w;
                printf("[%s] octave %d\n", k_wave_names[wave], octave);
            }
            wave_held[w] = down;
        }

        /* Octave shift. */
        bool up = bt_keyboard_is_pressed(KEY_UP);
        bool dn = bt_keyboard_is_pressed(KEY_DOWN);
        if (up && !oct_up_held && octave < 7) {
            octave++;
            printf("[%s] octave %d\n", k_wave_names[wave], octave);
        }
        if (dn && !oct_down_held && octave > 1) {
            octave--;
            printf("[%s] octave %d\n", k_wave_names[wave], octave);
        }
        oct_up_held = up;
        oct_down_held = dn;

        /* Volume: Left = softer, Right = louder (capped at the 100% default). */
        bool vup = bt_keyboard_is_pressed(KEY_RIGHT);
        bool vdn = bt_keyboard_is_pressed(KEY_LEFT);
        if (vup && !vol_up_held && volume < 100) {
            volume += 10;
            snd_set_volume(volume);
            printf("volume %d%%\n", volume);
        }
        if (vdn && !vol_down_held && volume > 10) {
            volume -= 10;
            snd_set_volume(volume);
            printf("volume %d%%\n", volume);
        }
        vol_up_held = vup;
        vol_down_held = vdn;

        /* Sleep at least one tick so we never busy-loop and starve IDLE0. */
        vTaskDelay(1);
    }

    fcntl(STDIN_FILENO, F_SETFL, old_flags);
    snd_all_off();
    printf("soundkeys: bye\n");
    return 0;
}

/*
 * synth - Subtractive synth demo (BreezyBox ELF app). MVP prototype.
 *
 * See SynthDemoAppMVPScope.md. Signal path:
 *
 *   [osc + detuned twin] + [sub square -1 oct] -> resonant LPF -> amp
 *          ^ glide slew           ^ cutoff += env * amount        ^ env
 *
 * Monophonic. The app renders int16 mono at 44.1 kHz into the firmware's
 * snd_stream_* PCM ring (like modplay), so the engine's limiter and power
 * handling apply. Oscillators are Q32 phase accumulators; the filter is an
 * integer Chamberlin SVF run at 2x per sample for stability; floats appear
 * only in setup paths (preset load, note pitch), never per sample.
 *
 * Demo UI: a built-in chill beat pattern loops forever; keys 1-6 (BLE/matrix
 * keyboard or console) switch presets, Tab rotates beat patterns, Left/Right
 * adjust master volume, q/Ctrl-C exits.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>

/* ---- Firmware API (resolved from the elf_loader symbol table) ---- */

int  snd_init(void);
int  snd_stream_open(int rate, int channels);
int  snd_stream_space(void);
int  snd_stream_write(const int16_t *frames, int nframes);
void snd_stream_close(void);
void snd_set_volume(int pct);

int bt_keyboard_is_pressed(uint8_t keycode);
int bt_keyboard_connected(void);

typedef uint32_t TickType_t;
void vTaskDelay(TickType_t ticks);

#define KEY_1     0x1E   /* 1..6 consecutive */
#define KEY_ESC   0x29
#define KEY_TAB   0x2B
#define KEY_RIGHT 0x4F
#define KEY_LEFT  0x50

/* ---- Constants ---- */

#define RATE          44100
#define CHUNK         256
#define NUM_PRESETS   6

/* ---- Presets: the 10 MVP controls ---- */

enum { WAVE_SAW = 0, WAVE_SQUARE, WAVE_SINE };

typedef struct {
    const char *name;
    uint8_t wave;
    uint8_t detune;     /* 0..255 -> 0..~3% twin osc offset */
    uint8_t sub;        /* 0..255 sub square level */
    uint8_t cutoff;     /* 0..255 exponential, ~40 Hz..~8 kHz */
    uint8_t res;        /* 0..255 */
    int16_t env_amt;    /* -255..255, env -> cutoff */
    uint16_t attack_ms;
    uint16_t decay_ms;  /* doubles as release */
    uint8_t sustain;    /* 1 = hold at peak while gate on */
    uint16_t glide_ms;
    uint8_t note_shift; /* demo: semitones added to the loop (register framing) */
    uint8_t def_pat;    /* demo: pattern that suits this preset */
    uint16_t gain;      /* output make-up gain, Q8 (320 = prior level) */
} preset_t;

/* note_shift frames each preset in a register the Tanmatsu's tiny speaker can
 * actually reproduce; 808/Reese live an octave lower on real speakers. Those
 * two also get make-up gain: their energy sits in the fundamental, which
 * reads quieter than the harmonic-rich presets at equal peak. */
static const preset_t k_presets[NUM_PRESETS] = {
    /* name           wave        det  sub  cut  res  envA  atk  dec  sus glide shift pat gain */
    { "808 Sub Bass", WAVE_SINE,    0,   0, 255,   0,    0,   2, 350,  0,   0, 12,  1, 400 },
    { "Funk Bass",    WAVE_SQUARE,  0, 180,  60,  60,  140,   2, 180,  0,   0, 12,  1, 240 },
    { "Acid 303",     WAVE_SAW,     0,   0,  70, 200,  170,   2, 220,  0,  60, 12,  2, 160 },
    { "Reese Bass",   WAVE_SAW,   200,   0,  90,  40,    0,   5, 250,  1,   0, 12,  1, 360 },
    { "Stadium Lead", WAVE_SAW,   120,  60, 230,  30,    0,  10, 250,  1,   0, 24,  1, 160 },
    { "80s Brass",    WAVE_SAW,    60, 120, 120,  50,  120,  60, 300,  1,   0, 12,  1, 160 },
};

/* ---- Synth state ---- */

typedef struct {
    /* oscillators */
    uint32_t ph1, ph2, phs;    /* main, detuned twin, sub */
    uint32_t inc;              /* current (glide-slewed) phase increment */
    uint32_t inc_target;
    uint32_t glide_g;          /* Q16 slew coefficient per sample, 65536 = instant */
    uint32_t det_q16;          /* twin offset: inc2 = inc + inc*det>>16 */
    /* envelope, Q15 */
    int32_t  env;
    int32_t  atk_step, dec_step;
    uint8_t  env_stage;        /* 0 idle, 1 attack, 2 sustain/decay */
    uint8_t  gate;
    /* filter */
    int32_t  flp, fbp;         /* SVF state */
    int32_t  qcoef;            /* Q16 damping (1/Q) */
} synth_t;

static synth_t g_s;
static preset_t g_p;                /* active preset (copied, tweakable later) */
static int32_t g_ftab[256];         /* cutoff control -> SVF f coefficient, Q16 */
static int16_t g_buf[CHUNK];

/* f = 2*sin(pi*fc/(2*RATE)), Q16, for the 2x-iterated SVF. Small angles
 * (max ~0.29 rad at 8 kHz), so sin x ~= x - x^3/6 is plenty. Table built
 * once with floats: fc = 40 * 2^(i/32), capped at 8 kHz. */
static void build_ftab(void)
{
    const float r = 1.0218971487f;   /* 2^(1/32) */
    float fc = 40.0f;
    for (int i = 0; i < 256; i++) {
        float f = fc > 8000.0f ? 8000.0f : fc;
        float a = 3.14159265f * f / (2.0f * RATE);
        float s = a - a * a * a * (1.0f / 6.0f);
        g_ftab[i] = (int32_t)(2.0f * s * 65536.0f);
        fc *= r;
    }
}

static void load_preset(int idx)
{
    g_p = k_presets[idx];
    synth_t *s = &g_s;

    s->det_q16 = (uint32_t)g_p.detune * 8;          /* max ~3.1% */
    s->atk_step = (int32_t)(32767.0f / (g_p.attack_ms * 44.1f) + 1.0f);
    s->dec_step = (int32_t)(32767.0f / (g_p.decay_ms * 44.1f) + 1.0f);
    s->glide_g = g_p.glide_ms == 0
                 ? 65536u
                 : (uint32_t)(65536.0f / (g_p.glide_ms * 44.1f) + 1.0f);
    /* damping 1/Q: ~1.9 (dead) down to ~0.1 (screaming) */
    s->qcoef = 124518 - (int32_t)g_p.res * 462;
    printf("[%d] %s\n", idx + 1, g_p.name);
}

/* MIDI note -> phase increment. Setup-only floats, no libm: walk semitone
 * ratios from A4 = 440 Hz (MIDI 69). */
static uint32_t note_inc(int midi)
{
    float f = 440.0f;
    for (int n = midi; n < 69; n++) f *= 0.943874313f;   /* 2^(-1/12) */
    for (int n = midi; n > 69; n--) f *= 1.059463094f;
    return (uint32_t)(f * (4294967296.0f / RATE));
}

static void note_on(int midi)
{
    synth_t *s = &g_s;
    s->inc_target = note_inc(midi + g_p.note_shift);
    if (s->env_stage == 0 || s->glide_g == 65536u) s->inc = s->inc_target;
    s->gate = 1;
    s->env_stage = 1;
    /* no phase reset: mono synths keep oscillators free-running */
}

static void note_off(void)
{
    g_s.gate = 0;
}

/* Parabolic pseudo-sine on a Q15 phase position, ~+/-8192 out. */
static inline int32_t osc_sine(uint32_t phase)
{
    int32_t t = (int32_t)((phase >> 17) & 0x7fff);
    t = (t < 16384) ? t * 2 - 16384 : 49150 - t * 2;   /* triangle +/-16384 */
    int32_t a = t < 0 ? -t : t;
    return (t * (32768 - a)) >> 15;
}

static inline int32_t osc_wave(uint32_t phase, int wave)
{
    switch (wave) {
        case WAVE_SAW:    return ((int32_t)((phase >> 17) & 0x7fff) - 16384) >> 1;
        case WAVE_SQUARE: return (phase & 0x80000000u) ? -8192 : 8192;
        default:          return osc_sine(phase);
    }
}

static void render(int16_t *out, int n)
{
    synth_t *s = &g_s;

    for (int i = 0; i < n; i++) {
        /* envelope */
        if (s->env_stage == 1) {
            s->env += s->atk_step;
            if (s->env >= 32767) { s->env = 32767; s->env_stage = 2; }
        } else if (s->env_stage == 2) {
            if (!(g_p.sustain && s->gate)) {
                s->env -= s->dec_step;
                if (s->env <= 0) { s->env = 0; s->env_stage = 0; }
            }
        }
        if (s->env_stage == 0) { out[i] = 0; continue; }

        /* glide */
        int32_t d = (int32_t)(s->inc_target - s->inc);
        s->inc += (uint32_t)((int32_t)(((int64_t)d * s->glide_g) >> 16));

        /* oscillators */
        int32_t mix = osc_wave(s->ph1, g_p.wave);
        if (g_p.detune) mix += osc_wave(s->ph2, g_p.wave);
        if (g_p.sub) {
            int32_t sq = (s->phs & 0x80000000u) ? -8192 : 8192;
            mix += (sq * g_p.sub) >> 8;
        }
        s->ph1 += s->inc;
        s->ph2 += s->inc + (uint32_t)(((uint64_t)s->inc * s->det_q16) >> 16);
        s->phs += s->inc >> 1;

        /* filter: cutoff control + env*amount, then 2x-iterated SVF */
        int32_t c = g_p.cutoff + ((s->env * g_p.env_amt) >> 15);
        if (c < 0) c = 0;
        if (c > 255) c = 255;
        int32_t f = g_ftab[c];
        for (int k = 0; k < 2; k++) {
            int32_t hp = mix - s->flp - (int32_t)(((int64_t)s->fbp * s->qcoef) >> 16);
            s->fbp += (int32_t)(((int64_t)f * hp) >> 16);
            s->flp += (int32_t)(((int64_t)f * s->fbp) >> 16);
            if (s->fbp > 262144) s->fbp = 262144;          /* resonance clamp */
            else if (s->fbp < -262144) s->fbp = -262144;
        }

        /* amp: env, then per-preset make-up gain to a comfortable level */
        int32_t y = (s->flp * s->env) >> 15;
        y = (y * g_p.gain) >> 8;
        if (y > 32767) y = 32767;
        else if (y < -32767) y = -32767;
        out[i] = (int16_t)y;
    }
}

/* ---- Demo sequencer: a few chill single-instrument patterns ----
 *
 * 64 steps of 16ths (4 bars), all loosely in A minor (Am F G Am roots).
 * Cell values: a MIDI note starts it, REST cuts the previous note, TIE holds
 * it through the step (gapless -- and with glide > 0 a note followed by ties
 * into a new note slides, 303-style). Notes sit around A1 (33); each preset
 * adds its note_shift on top.
 */

#define REST  (-1)
#define TIE   (-2)
#define STEPS 64
#define NUM_PATTERNS 3

typedef struct {
    const char *name;
    uint16_t bpm;
    int8_t cells[STEPS];
} pattern_t;

#define _ REST
#define T TIE

static const pattern_t k_patterns[NUM_PATTERNS] = {
    { "Slow Groove", 84, {   /* laid-back dub bass, lots of air */
        33, T, T, _,  _, _,33, _,  _, _,40, _, 33, T, T, _,
        33, T, T, _,  _, _,33, _,  _, _,36, _, 33, T, T, _,
        29, T, T, _,  _, _,29, _,  _, _,36, _, 29, T, T, _,
        31, T, T, _,  _, _,31, _, 33, T, T, T,  T, T, _, _, } },
    { "Funk Sixteens", 96, { /* syncopated pops, short notes, rests */
        33, _, _,33,  _, _,45, _,  _,33, _, _, 45, _,43, _,
        33, _, _,33,  _, _,45, _,  _,33, _,36, 33, _, _, _,
        29, _, _,29,  _, _,41, _,  _,29, _, _, 41, _,40, _,
        31, _, _,31,  _, _,43, _,  _,31, _,38, 36, _,33, _, } },
    { "Acid Eighths", 110, { /* even eighths, ties make the 303 slide */
        33, _,33, _, 45, T,33, _, 36, _,33, _, 45, _,48, T,
        33, _,33, _, 45, T,33, _, 36, _,33, _, 31, T,33, _,
        29, _,29, _, 41, T,29, _, 33, _,29, _, 41, _,44, T,
        31, _,31, _, 43, T,31, _, 34, _,31, _, 43, T,45, _, } },
};

#undef _
#undef T

static int g_pat = 0;          /* active pattern */
static int g_step = 0;         /* 0..STEPS-1 */
static int g_step_smp = 0;     /* samples into the current step */
static bool g_gated = false;   /* note_off already sent for this step */

static void select_pattern(int idx)
{
    g_pat = idx;
    g_step = 0;
    g_step_smp = 0;
    g_gated = false;
    note_off();
    printf("  beat: %s (%d bpm)\n", k_patterns[idx].name, k_patterns[idx].bpm);
}

/* Render one chunk, running the sequencer sample-accurately against the
 * render clock. A step's note gates off at 3/4 of the step unless the next
 * cell is a TIE (then it holds through -- legato/slide). */
static void run_sequencer(int16_t *buf, int nframes)
{
    const pattern_t *p = &k_patterns[g_pat];
    const int step_len = RATE * 60 / (p->bpm * 4);
    int filled = 0;

    while (filled < nframes) {
        if (g_step_smp == 0) {
            int8_t cell = p->cells[g_step];
            if (cell >= 0) note_on(cell);
            else if (cell == REST) note_off();
            g_gated = false;
        }
        int8_t next = p->cells[(g_step + 1) % STEPS];
        int gate_len = (next == TIE) ? step_len : step_len * 3 / 4;

        int upto = (!g_gated && g_step_smp < gate_len) ? gate_len : step_len;
        int run = upto - g_step_smp;
        if (run > nframes - filled) run = nframes - filled;
        render(buf + filled, run);
        filled += run;
        g_step_smp += run;

        if (!g_gated && g_step_smp >= gate_len) {
            if (next != TIE && p->cells[g_step] != TIE) note_off();
            g_gated = true;
        }
        if (g_step_smp >= step_len) {
            g_step_smp = 0;
            g_step = (g_step + 1) % STEPS;
        }
    }
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    if (snd_init() != 0) {
        printf("Sound init failed\n");
        return 1;
    }
    build_ftab();
    snd_set_volume(80);

    if (snd_stream_open(RATE, 1) != 0) {
        printf("snd_stream_open failed (stream busy?)\n");
        return 1;
    }

    int have_kbd = bt_keyboard_connected();
    printf("synth: 1-%d = preset, Tab = beat, Left/Right = volume, q = exit\n",
           NUM_PRESETS);
    load_preset(0);
    select_pattern(k_presets[0].def_pat);

    int old_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, old_flags | O_NONBLOCK);

    int volume = 80;
    bool num_held[NUM_PRESETS] = { false };
    bool vup_held = false, vdn_held = false, tab_held = false;
    int esc_seq = 0;   /* console arrows arrive as ESC [ C/D */
    int running = 1;

    while (running) {
        /* --- console input. With a raw keyboard present (Tanmatsu matrix /
         * BLE) each key also reaches stdin via the console, so then stdin
         * only handles exit; otherwise it drives everything. Arrow keys
         * arrive as ESC [ C/D -- parse them, never exit on a bare ESC. --- */
        char ch;
        while (read(STDIN_FILENO, &ch, 1) == 1) {
            if (esc_seq == 1) { esc_seq = (ch == '[') ? 2 : 0; continue; }
            if (esc_seq == 2) {
                esc_seq = 0;
                if (have_kbd) continue;
                if (ch == 'C' && volume < 100) { volume += 10; snd_set_volume(volume); printf("volume %d%%\n", volume); }
                if (ch == 'D' && volume > 10)  { volume -= 10; snd_set_volume(volume); printf("volume %d%%\n", volume); }
                continue;
            }
            if (ch == 27) esc_seq = 1;
            else if (ch == 3 || ch == 'q') running = 0;
            else if (have_kbd) continue;
            else if (ch == '\t') select_pattern((g_pat + 1) % NUM_PATTERNS);
            else if (ch >= '1' && ch < '1' + NUM_PRESETS) {
                load_preset(ch - '1');
                select_pattern(g_p.def_pat);
            }
        }
        /* --- raw key matrix / BLE keyboard, edge-triggered --- */
        if (have_kbd) {
            for (int p = 0; p < NUM_PRESETS; p++) {
                bool down = bt_keyboard_is_pressed(KEY_1 + p);
                if (down && !num_held[p]) {
                    load_preset(p);
                    select_pattern(g_p.def_pat);
                }
                num_held[p] = down;
            }
            bool tab = bt_keyboard_is_pressed(KEY_TAB);
            if (tab && !tab_held) select_pattern((g_pat + 1) % NUM_PATTERNS);
            tab_held = tab;
            bool vup = bt_keyboard_is_pressed(KEY_RIGHT);
            bool vdn = bt_keyboard_is_pressed(KEY_LEFT);
            if (vup && !vup_held && volume < 100) {
                volume += 10; snd_set_volume(volume);
                printf("volume %d%%\n", volume);
            }
            if (vdn && !vdn_held && volume > 10) {
                volume -= 10; snd_set_volume(volume);
                printf("volume %d%%\n", volume);
            }
            vup_held = vup; vdn_held = vdn;
        }
        if (!running) break;

        /* --- render as much as the stream ring will take --- */
        int space = snd_stream_space();
        while (space >= CHUNK) {
            run_sequencer(g_buf, CHUNK);
            snd_stream_write(g_buf, CHUNK);
            space -= CHUNK;
        }
        vTaskDelay(1);
    }

    fcntl(STDIN_FILENO, F_SETFL, old_flags);
    snd_stream_close();
    printf("synth: bye\n");
    return 0;
}

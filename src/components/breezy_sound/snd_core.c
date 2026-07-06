/*
 * snd_core.c - 8-voice PICO-8-flavored synth mixer + PCM stream.
 *
 * One background task renders all voices into 256-frame mono chunks, sums in
 * the PCM stream, and hands the result to the board's snd_port (snd_port.h)
 * for I2S output at 44.1 kHz s16. Power handling: after all sources go
 * silent, feed a few chunks of silence to drain the DMA queue, then stop the
 * port; wake on the next note via task notification.
 *
 * All synthesis is fixed point: 32-bit phase accumulators, no floats in the
 * render loop (frequency setup uses float once per note_on, which is fine).
 */

#include "snd_core.h"
#include "snd_port.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "snd";

#define SND_RATE          44100
#define CHUNK_FRAMES      256                  /* ~5.8 ms */
#define DRAIN_CHUNKS      8                    /* ~46 ms of silence before power-down */
#define ATTACK_STEP       (32767 / (SND_RATE * 5 / 1000))    /* ~5 ms to full */
#define RELEASE_STEP      (32767 / (SND_RATE * 80 / 1000))   /* ~80 ms fade */

enum env_state { ENV_OFF = 0, ENV_ATTACK, ENV_SUSTAIN, ENV_RELEASE };

typedef struct {
    volatile uint8_t env_state;
    uint8_t  wave;
    uint8_t  vol;          /* 0-255 */
    uint32_t phase;
    uint32_t phase_inc;    /* freq * 2^32 / SND_RATE */
    uint32_t phase2;       /* detuned second oscillator (phaser/organ) */
    int32_t  env;          /* Q15 envelope level */
    uint32_t seq;          /* allocation age, for voice stealing */
    uint32_t noise;        /* xorshift state */
    uint32_t noise_step;   /* last sample-and-hold step position */
    int32_t  noise_hold;   /* held noise sample */
} voice_t;

static voice_t      g_voices[SND_NUM_VOICES];
static portMUX_TYPE g_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t g_task = NULL;
static uint32_t     g_seq = 0;
static bool         g_powered = false;  /* ports leave output stopped after init */

static int32_t g_master = 256;   /* master volume, Q8: 256 = 100% */
static int32_t g_gain = 65536;   /* limiter gain, Q16, <= 1.0 */
static int32_t g_env  = 0;       /* limiter peak-hold envelope */
static int16_t g_mono[CHUNK_FRAMES];
static int16_t g_out[CHUNK_FRAMES * 2];

/* PCM stream: SPSC ring of stereo frames. The writer (app) copies frames in
 * and advances g_str_head; the mixer task consumes and advances g_str_tail.
 * Both are free-running uint32 counters (indices mod STREAM_FRAMES). */
#define STREAM_FRAMES 4096              /* ~93 ms at 44.1 kHz */
static int16_t  g_stream_buf[STREAM_FRAMES * 2];
static volatile uint32_t g_str_head = 0;
static volatile uint32_t g_str_tail = 0;
static volatile bool     g_stream_on = false;
static int      g_stream_ch = 2;

/* Per-waveform base gain, percent: psychoacoustic equal-comfort levels,
 * tuned by ear on the reference speakers. Bright/harsh waves get pulled down
 * so switching instruments doesn't jump in perceived loudness. */
static const uint8_t k_wave_gain_pct[SND_WAVE_COUNT] = {
    [SND_WAVE_TRIANGLE]   = 100,
    [SND_WAVE_TILTED_SAW] =  50,
    [SND_WAVE_SAW]        =  20,
    [SND_WAVE_SQUARE]     =  20,
    [SND_WAVE_PULSE]      =  20,
    [SND_WAVE_ORGAN]      = 100,
    [SND_WAVE_NOISE]      =  20,
    [SND_WAVE_PHASER]     = 100,
    [SND_WAVE_EPIANO]     = 100,
};

/* Triangle from a 15-bit phase position, range +/-16384. */
static inline int32_t tri15(uint32_t pos)
{
    return (pos < 16384) ? (int32_t)(pos * 2) - 16384
                         : 49150 - (int32_t)(pos * 2);
}

/* Cheap pseudo-sine, range +/-16384: parabolic shaping of the triangle. */
static inline int32_t sin15(uint32_t phase)
{
    int32_t t = tri15((phase >> 17) & 0x7fff);
    int32_t a = t < 0 ? -t : t;
    return (t * (32768 - a)) >> 14;   /* peak 2^28, fits int32 */
}

static inline int32_t wave_sample(voice_t *v)
{
    uint32_t pos = v->phase >> 17;   /* 0..32767 */
    switch (v->wave) {
        case SND_WAVE_TRIANGLE:
            return tri15(pos);
        case SND_WAVE_TILTED_SAW:
            return (pos < 28672) ? (int32_t)(pos * 8 / 7) - 16384
                                 : (int32_t)((32767 - pos) * 8) - 16384;
        case SND_WAVE_SAW:
            return (int32_t)pos - 16384;
        case SND_WAVE_SQUARE:
            return (pos < 16384) ? 12288 : -12288;
        case SND_WAVE_PULSE:
            return (pos < 10922) ? 12288 : -12288;
        case SND_WAVE_ORGAN:  /* base triangle + octave-up triangle */
            return (tri15(pos) + tri15((v->phase >> 16) & 0x7fff)) / 2;
        case SND_WAVE_NOISE: {
            /* Pitched noise: sample-and-hold, 32 random steps per cycle, so
             * the key frequency sets the noise "pitch" (PICO-8 style). */
            uint32_t step = v->phase >> 27;
            if (step != v->noise_step) {
                v->noise_step = step;
                v->noise ^= v->noise << 13;
                v->noise ^= v->noise >> 17;
                v->noise ^= v->noise << 5;
                v->noise_hold = (int32_t)(v->noise & 0x7fff) - 16384;
            }
            return v->noise_hold;
        }
        case SND_WAVE_PHASER: /* two slightly detuned triangles */
            return (tri15(pos) + tri15(v->phase2 >> 17)) / 2;
        case SND_WAVE_EPIANO: /* sine + soft 4x "tine" overtone */
            return (3 * sin15(v->phase) + sin15(v->phase * 4)) / 4;
        default:
            return 0;
    }
}

/* Render one mono chunk. Returns the number of active voices. */
static int render_chunk(void)
{
    int active = 0;
    int32_t mix[CHUNK_FRAMES];
    memset(mix, 0, sizeof(mix));

    for (int vi = 0; vi < SND_NUM_VOICES; vi++) {
        voice_t *v = &g_voices[vi];
        if (v->env_state == ENV_OFF) continue;
        active++;

        for (int i = 0; i < CHUNK_FRAMES; i++) {
            switch (v->env_state) {
                case ENV_ATTACK:
                    v->env += ATTACK_STEP;
                    if (v->env >= 32767) { v->env = 32767; v->env_state = ENV_SUSTAIN; }
                    break;
                case ENV_RELEASE:
                    v->env -= RELEASE_STEP;
                    if (v->env <= 0) { v->env = 0; v->env_state = ENV_OFF; }
                    break;
                default:
                    break;
            }
            if (v->env_state == ENV_OFF) break;

            int32_t s = wave_sample(v);           /* +/-16384 */
            s = (s * v->env) >> 15;               /* envelope */
            s = (s * v->vol) >> 8;                /* per-voice volume */
            mix[i] += s;

            v->phase  += v->phase_inc;
            v->phase2 += v->phase_inc - (v->phase_inc >> 7);  /* ~0.8% detune */
        }
    }

    /* Master limiter, tuned on a host test bench (see git history of the
     * p4-tanmatsu example for the methodology).
     *
     * The gain follows a peak-hold envelope (instant rise, ~1.5 s decay)
     * instead of the raw per-chunk peak, so beating chord peaks don't pump
     * the gain at audible rates -- residual gain motion is slow "breathing".
     * limit_peak is set per board to what its speaker reproduces cleanly,
     * not to int16 range: on the bench, 2 notes at ~25k digital peak were
     * already audibly distorted by the speaker while digitally clean.
     * Attack is INSTANT: the whole chunk is rendered before gain is applied,
     * so the chunk peak is known up front; when the gain must drop we jump
     * to the target at sample 0 instead of ramping down across the chunk (a
     * downward ramp lets the peak through before the gain arrives -- on the
     * bench that overshot to ~25k on note onsets). Release still eases up
     * gradually, interpolated across the chunk (no zipper noise). A soft
     * knee at the limit + hard int16 clamp catch residual rounding. */
    const int32_t limit_peak = snd_port_desc.limit_peak;
    int32_t peak = 0;
    for (int i = 0; i < CHUNK_FRAMES; i++) {
        int32_t a = mix[i] < 0 ? -mix[i] : mix[i];
        if (a > peak) peak = a;
    }
    g_env -= g_env >> 8;                            /* slow fall */
    if (peak > g_env) g_env = peak;                 /* instant rise */
    int32_t target = (g_env > limit_peak)
                   ? (int32_t)(((int64_t)limit_peak << 16) / g_env)
                   : 65536;                         /* Q16, <= 1.0 */
    int32_t gain, gain_step;
    if (target < g_gain) {                          /* attack: drop now */
        gain = target;
        gain_step = 0;
        g_gain = target;
    } else {                                        /* release: ease up */
        int32_t gain_end = g_gain + ((target - g_gain) >> 5);
        gain = g_gain;
        gain_step = (gain_end - g_gain) / CHUNK_FRAMES;
        g_gain = gain_end;
    }
    for (int i = 0; i < CHUNK_FRAMES; i++) {
        int32_t s = (int32_t)(((int64_t)mix[i] * gain) >> 16);
        gain += gain_step;
        if (s > limit_peak)       s = limit_peak + (s - limit_peak) / 4;
        else if (s < -limit_peak) s = -limit_peak + (s + limit_peak) / 4;
        if (s > INT16_MAX) s = INT16_MAX;
        else if (s < INT16_MIN) s = INT16_MIN;
        /* Master volume last: pure attenuation, cannot exceed limit_peak. */
        g_mono[i] = (int16_t)((s * g_master) >> 8);
    }
    return active;
}

static void snd_task(void *arg)
{
    (void)arg;
    int silence_chunks = 0;

    for (;;) {
        int active = render_chunk();
        if (g_stream_on) active++;   /* open stream holds power up, even on underrun */

        if (active == 0) {
            if (!g_powered) {
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                continue;
            }
            if (silence_chunks >= DRAIN_CHUNKS) {
                snd_port_stop();
                g_powered = false;
                silence_chunks = 0;
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                continue;
            }
            memset(g_mono, 0, sizeof(g_mono));
            silence_chunks++;
        } else {
            if (!g_powered) {
                snd_port_start();
                g_powered = true;
            }
            silence_chunks = 0;
        }

        /* Output stage: sum the PCM stream on top of the synth (post-limiter:
         * stream material is pre-mastered, and mixing it pre-limiter would
         * make synth peaks duck the music). Stream underrun contributes
         * silence; timing never blocks on the app. Master volume was already
         * applied to g_mono; apply it to the stream here so snd_set_volume
         * governs both. The port's write blocks until DMA has room -- that
         * paces this task to the sample rate. */
        uint32_t avail = g_stream_on ? g_str_head - g_str_tail : 0;
        if (snd_port_desc.stereo) {
            for (int i = 0; i < CHUNK_FRAMES; i++) {
                int32_t l = g_mono[i], r = g_mono[i];
                if (avail > 0) {
                    uint32_t idx = (g_str_tail % STREAM_FRAMES) * 2;
                    l += (g_stream_buf[idx]     * g_master) >> 8;
                    r += (g_stream_buf[idx + 1] * g_master) >> 8;
                    g_str_tail = g_str_tail + 1;
                    avail--;
                }
                if (l > INT16_MAX) l = INT16_MAX; else if (l < INT16_MIN) l = INT16_MIN;
                if (r > INT16_MAX) r = INT16_MAX; else if (r < INT16_MIN) r = INT16_MIN;
                g_out[i * 2]     = (int16_t)l;
                g_out[i * 2 + 1] = (int16_t)r;
            }
            snd_port_write(g_out, CHUNK_FRAMES);
        } else {
            for (int i = 0; i < CHUNK_FRAMES && avail > 0; i++) {
                uint32_t idx = (g_str_tail % STREAM_FRAMES) * 2;
                int32_t s = g_mono[i];   /* mono out: downmix stream to (L+R)/2 */
                s += ((((int32_t)g_stream_buf[idx] + g_stream_buf[idx + 1]) / 2)
                      * g_master) >> 8;
                g_str_tail = g_str_tail + 1;
                avail--;
                if (s > INT16_MAX) s = INT16_MAX; else if (s < INT16_MIN) s = INT16_MIN;
                g_mono[i] = (int16_t)s;
            }
            snd_port_write(g_mono, CHUNK_FRAMES);
        }
    }
}

esp_err_t snd_init(void)
{
    if (g_task != NULL) return ESP_OK;

    esp_err_t err = snd_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "snd_port_init failed: %s", esp_err_to_name(err));
        return err;
    }

    if (xTaskCreate(snd_task, "snd_mixer", 4096, NULL, 7, &g_task) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create mixer task");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Mixer started (%d voices, %d Hz)", SND_NUM_VOICES, SND_RATE);
    return ESP_OK;
}

int snd_note_on(int voice, float freq_hz, int wave, int vol)
{
    if (g_task == NULL) return -1;
    if (freq_hz < 20.0f) freq_hz = 20.0f;
    if (freq_hz > 12000.0f) freq_hz = 12000.0f;
    if (wave < 0 || wave >= SND_WAVE_COUNT) wave = SND_WAVE_TRIANGLE;
    if (vol < 0) vol = 0;
    if (vol > 255) vol = 255;

    taskENTER_CRITICAL(&g_lock);
    if (voice < 0) {
        /* Prefer a free voice; otherwise steal the oldest. */
        uint32_t oldest = UINT32_MAX;
        int steal = 0;
        for (int i = 0; i < SND_NUM_VOICES; i++) {
            if (g_voices[i].env_state == ENV_OFF) { voice = i; break; }
            if (g_voices[i].seq < oldest) { oldest = g_voices[i].seq; steal = i; }
        }
        if (voice < 0) voice = steal;
    } else if (voice >= SND_NUM_VOICES) {
        taskEXIT_CRITICAL(&g_lock);
        return -1;
    }

    voice_t *v = &g_voices[voice];
    v->wave      = (uint8_t)wave;
    v->vol       = (uint8_t)((vol * k_wave_gain_pct[wave]) / 100);
    v->phase_inc = (uint32_t)(freq_hz * 4294967296.0f / SND_RATE);
    v->phase     = 0;
    v->phase2    = 0;
    v->env       = 0;
    v->seq       = ++g_seq;
    if (v->noise == 0) v->noise = 0x12345678 + voice;
    v->env_state = ENV_ATTACK;
    taskEXIT_CRITICAL(&g_lock);

    xTaskNotifyGive(g_task);
    return voice;
}

void snd_note_off(int voice)
{
    if (g_task == NULL || voice < 0 || voice >= SND_NUM_VOICES) return;
    taskENTER_CRITICAL(&g_lock);
    if (g_voices[voice].env_state == ENV_ATTACK ||
        g_voices[voice].env_state == ENV_SUSTAIN) {
        g_voices[voice].env_state = ENV_RELEASE;
    }
    taskEXIT_CRITICAL(&g_lock);
}

void snd_set_volume(int pct)
{
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    g_master = (pct * 256) / 100;
}

int snd_get_volume(void)
{
    return (g_master * 100) / 256;
}

int snd_stream_open(int rate, int channels)
{
    if (g_task == NULL || rate != SND_RATE ||
        (channels != 1 && channels != 2) || g_stream_on) {
        return -1;
    }
    g_str_head = 0;
    g_str_tail = 0;
    g_stream_ch = channels;
    g_stream_on = true;
    xTaskNotifyGive(g_task);   /* wake + power up if the mixer was idle */
    return 0;
}

int snd_stream_space(void)
{
    if (!g_stream_on) return 0;
    return (int)(STREAM_FRAMES - (g_str_head - g_str_tail));
}

int snd_stream_write(const int16_t *frames, int nframes)
{
    if (!g_stream_on || frames == NULL || nframes < 0) return -1;
    int space = (int)(STREAM_FRAMES - (g_str_head - g_str_tail));
    if (nframes > space) nframes = space;
    uint32_t head = g_str_head;
    for (int i = 0; i < nframes; i++) {
        uint32_t idx = (head % STREAM_FRAMES) * 2;
        if (g_stream_ch == 2) {
            g_stream_buf[idx]     = frames[i * 2];
            g_stream_buf[idx + 1] = frames[i * 2 + 1];
        } else {
            g_stream_buf[idx]     = frames[i];
            g_stream_buf[idx + 1] = frames[i];
        }
        head++;
    }
    g_str_head = head;   /* publish after the copy */
    return nframes;
}

void snd_stream_close(void)
{
    g_stream_on = false;
}

void snd_all_off(void)
{
    if (g_task == NULL) return;
    snd_stream_close();
    taskENTER_CRITICAL(&g_lock);
    for (int i = 0; i < SND_NUM_VOICES; i++) {
        if (g_voices[i].env_state == ENV_ATTACK ||
            g_voices[i].env_state == ENV_SUSTAIN) {
            g_voices[i].env_state = ENV_RELEASE;
        }
    }
    taskEXIT_CRITICAL(&g_lock);
}

/*
 * snd_port.c - breezy_sound port for the Waveshare S3 board (MAX98357A).
 *
 * We own the I2S channel (no BSP/codec): standard mode, mono 16-bit at
 * 44.1 kHz. Power handling is simple: the MAX98357A auto-standbys ~10ms
 * after BCLK stops, so start/stop is just enabling/disabling the channel.
 *
 * Pins match the earlier cmd_beep demo (esp32dos my_sound.c):
 * BCLK=GPIO9, LRCLK/WS=GPIO8, DIN=GPIO6.
 */

#include "snd_port.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/i2s_std.h"

static const char *TAG = "snd_port";

#define SND_I2S_BCLK  GPIO_NUM_9
#define SND_I2S_WS    GPIO_NUM_8
#define SND_I2S_DOUT  GPIO_NUM_6

#define SND_RATE      44100
#define CHUNK_FRAMES  256

/* Full scale, like the Tanmatsu port. The synth limiter runs pre-master-
 * volume on a bus that reaches +/-16384 per voice, so a low ceiling here
 * keeps the limiter/soft-knee engaged constantly -- audible crackle at any
 * volume. Loudness is the volume knob's job, not the limiter's. */
const snd_port_desc_t snd_port_desc = {
    .stereo     = false,
    .limit_peak = 32767,
};

static i2s_chan_handle_t g_i2s = NULL;

esp_err_t snd_port_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 6;
    chan_cfg.dma_frame_num = CHUNK_FRAMES;
    esp_err_t err = i2s_new_channel(&chan_cfg, &g_i2s, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(err));
        return err;
    }

    /* The MAX98357A is the standard-I2S (Philips) variant -- left-justified
     * is the MAX98357B. MSB mode here made the amp read samples one bit
     * early: doubled amplitude, sign wrap above 16384 */
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SND_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = SND_I2S_BCLK,
            .ws   = SND_I2S_WS,
            .dout = SND_I2S_DOUT,
            .din  = I2S_GPIO_UNUSED,
        },
    };
    err = i2s_channel_init_std_mode(g_i2s, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s init failed: %s", esp_err_to_name(err));
        i2s_del_channel(g_i2s);
        g_i2s = NULL;
        return err;
    }
    /* Channel stays disabled until the first note (amp in standby). */
    return ESP_OK;
}

void snd_port_start(void)
{
    i2s_channel_enable(g_i2s);
}

void snd_port_stop(void)
{
    i2s_channel_disable(g_i2s);
}

void snd_port_write(const int16_t *frames, int nframes)
{
    size_t written = 0;
    /* Mono slot mode: one int16 per frame. Blocks until DMA has room. */
    i2s_channel_write(g_i2s, frames, (size_t)nframes * sizeof(int16_t),
                      &written, portMAX_DELAY);
}

/*
 * snd_port.c - breezy_sound port for the Tanmatsu (ES8156 via badge-bsp).
 *
 * The BSP sets up the I2S channel and codec (44.1 kHz s16 stereo); we drive
 * the amp following the tanmatsu-launcher audio_mixer.c pattern: amp off +
 * I2S disabled while idle, amp on unless headphones are in while playing.
 */

#include "snd_port.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/i2s_common.h"
#include "driver/i2s_std.h"

#include "bsp/audio.h"
#include "bsp/input.h"

static const char *TAG = "snd_port";

#define MASTER_VOLUME_PCT 100   /* codec hardware volume */

/* limit_peak: what the tiny Tanmatsu speaker reproduces cleanly. */
const snd_port_desc_t snd_port_desc = {
    .stereo     = true,
    .limit_peak = 32767,
};

static i2s_chan_handle_t g_i2s = NULL;

esp_err_t snd_port_init(void)
{
    esp_err_t err = bsp_audio_get_i2s_handle(&g_i2s);
    if (err != ESP_OK || g_i2s == NULL) {
        ESP_LOGE(TAG, "No I2S handle from BSP (audio not initialized?)");
        return (err != ESP_OK) ? err : ESP_ERR_INVALID_STATE;
    }

    bsp_audio_set_volume(MASTER_VOLUME_PCT);

    /* The BSP leaves the channel enabled after init; the engine expects the
     * output stopped until the first note, so park it. */
    bsp_audio_set_amplifier(false);
    i2s_channel_disable(g_i2s);

    /* The BSP configures the I2S slot as MSB-justified, but it puts the
     * ES8156 in standard Philips mode (sdp sp_protocal=0). The one-bit
     * misalignment makes the codec read every sample doubled, wrapping sign
     * at +/-16384. Reconfigure our side to Philips to match the codec. */
    i2s_std_slot_config_t slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    esp_err_t serr = i2s_channel_reconfig_std_slot(g_i2s, &slot_cfg);
    if (serr != ESP_OK) {
        ESP_LOGE(TAG, "I2S slot reconfig failed: %s", esp_err_to_name(serr));
        return serr;
    }
    return ESP_OK;
}

void snd_port_start(void)
{
    i2s_channel_enable(g_i2s);
    /* Amp on unless headphones are in. */
    bool jack = false;
    if (bsp_input_read_action(BSP_INPUT_ACTION_TYPE_AUDIO_JACK, &jack) != ESP_OK) {
        jack = false;
    }
    bsp_audio_set_amplifier(!jack);
}

void snd_port_stop(void)
{
    bsp_audio_set_amplifier(false);
    i2s_channel_disable(g_i2s);
}

void snd_port_write(const int16_t *frames, int nframes)
{
    size_t written = 0;
    /* Stereo: two int16 per frame. Blocks until DMA has room. */
    i2s_channel_write(g_i2s, frames, (size_t)nframes * 2 * sizeof(int16_t),
                      &written, portMAX_DELAY);
}

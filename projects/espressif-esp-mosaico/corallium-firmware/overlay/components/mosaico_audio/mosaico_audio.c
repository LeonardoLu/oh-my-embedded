// SPDX-License-Identifier: MIT
#include "mosaico_audio.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "dev_audio_codec.h"
#include "driver/i2s_common.h"
#include "esp_board_manager.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "mosaico_audio";
static esp_codec_dev_handle_t s_codec;
static i2s_chan_handle_t s_output;
static SemaphoreHandle_t s_lock;
static TaskHandle_t s_task;
static bool s_open;
static int s_volume;

static esp_err_t apply_volume_locked(int volume)
{
    if (s_open && (esp_codec_dev_set_out_vol(s_codec, volume) != ESP_CODEC_DEV_OK ||
                   esp_codec_dev_set_out_mute(s_codec, volume == 0) != ESP_CODEC_DEV_OK)) {
        return ESP_FAIL;
    }
    s_volume = volume;
    return ESP_OK;
}

static void feedback_task(void *context)
{
    (void)context;
    /* A 100 ms, softly gated confirmation only follows a local volume action.
     * No continuous zero-PCM stream, audio mixer or capture task is started. */
    int16_t samples[160];
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (s_volume == 0) {
            xSemaphoreGive(s_lock);
            continue;
        }
        esp_codec_dev_sample_info_t format = {.sample_rate = 16000, .channel = 1, .bits_per_sample = 16};
        int result = esp_codec_dev_open(s_codec, &format);
        s_open = result == ESP_CODEC_DEV_OK;
        i2s_chan_info_t output = {0};
        if (s_open && (i2s_channel_get_info(s_output, &output) != ESP_OK ||
                       !output.is_enabled || output.total_dma_buf_size == 0)) {
            result = ESP_CODEC_DEV_DRV_ERR;
        }
        if (s_open && apply_volume_locked(s_volume) != ESP_OK) result = ESP_CODEC_DEV_DRV_ERR;
        xSemaphoreGive(s_lock);
        for (int frame = 0; frame < 10 && result == ESP_CODEC_DEV_OK; ++frame) {
            for (int index = 0; index < 160; ++index) {
                const int sample = frame * 160 + index;
                const int edge = sample < 128 ? sample : sample > 1471 ? 1599 - sample : 128;
                samples[index] = (int16_t)(4096.0f * edge / 128.0f * sinf(sample * (2.0f * 3.14159265f * 880.0f / 16000.0f)));
            }
            xSemaphoreTake(s_lock, portMAX_DELAY);
            if (s_volume == 0) {
                xSemaphoreGive(s_lock);
                break;
            }
            result = esp_codec_dev_write(s_codec, samples, sizeof(samples));
            xSemaphoreGive(s_lock);
        }
        /* codec write only copies into I2S DMA. Advancing one complete ring
         * with silence lets the last tone sample leave DMA before mute/close
         * turns off the PA. Read capacity after open reconfigures mono PCM. */
        memset(samples, 0, sizeof(samples));
        for (size_t remaining = output.total_dma_buf_size;
             remaining > 0 && result == ESP_CODEC_DEV_OK;) {
            xSemaphoreTake(s_lock, portMAX_DELAY);
            if (s_volume == 0) {
                xSemaphoreGive(s_lock);
                break;
            }
            result = esp_codec_dev_write(s_codec, samples, sizeof(samples));
            xSemaphoreGive(s_lock);
            remaining -= remaining < sizeof(samples) ? remaining : sizeof(samples);
        }
        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (s_open) {
            (void)esp_codec_dev_set_out_mute(s_codec, true);
        }
        /* open() can enable I2S before a later codec step fails. Always close
         * the attempted device so failures cannot leave its output running. */
        (void)esp_codec_dev_close(s_codec);
        s_open = false;
        xSemaphoreGive(s_lock);
        if (result != ESP_CODEC_DEV_OK) ESP_LOGW(TAG, "speaker feedback failed: %d", result);
    }
}

esp_err_t mosaico_audio_init(void)
{
    if (s_task) return ESP_OK;
    esp_err_t err = esp_board_manager_init_device_by_name("audio_dac");
    if (err != ESP_OK) return err;
    void *handle = NULL;
    err = esp_board_manager_get_device_handle("audio_dac", &handle);
    if (err != ESP_OK || handle == NULL) return err == ESP_OK ? ESP_ERR_INVALID_STATE : err;
    s_codec = ((dev_audio_codec_handles_t *)handle)->codec_dev;
    if (!s_codec) return ESP_ERR_INVALID_STATE;
    handle = NULL;
    err = esp_board_manager_get_periph_handle("i2s_audio_out", &handle);
    if (err != ESP_OK || handle == NULL) return err == ESP_OK ? ESP_ERR_INVALID_STATE : err;
    s_output = (i2s_chan_handle_t)handle;
    /* The official ES8311 close path disables I2S/DAC and its PA GPIO. */
    if (esp_codec_set_disable_when_closed(s_codec, true) != ESP_CODEC_DEV_OK) return ESP_FAIL;
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;
    if (xTaskCreate(feedback_task, "speaker_feedback", 4096, NULL, 5, &s_task) != pdPASS) {
        vSemaphoreDelete(s_lock); s_lock = NULL;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "output ready; DAC opens only for local feedback");
    return ESP_OK;
}

bool mosaico_audio_available(void) { return s_task != NULL; }

esp_err_t mosaico_audio_set_volume(int volume)
{
    if (volume < 0 || volume > 100) return ESP_ERR_INVALID_ARG;
    if (!s_task) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(500)) != pdTRUE) return ESP_ERR_TIMEOUT;
    const esp_err_t err = apply_volume_locked(volume);
    xSemaphoreGive(s_lock);
    return err;
}

int mosaico_audio_get_volume(void)
{
    if (!s_task || xSemaphoreTake(s_lock, pdMS_TO_TICKS(500)) != pdTRUE) return -1;
    const int result = s_volume;
    xSemaphoreGive(s_lock);
    return result;
}

esp_err_t mosaico_audio_preview(void)
{
    if (!s_task) return ESP_ERR_INVALID_STATE;
    xTaskNotifyGive(s_task);
    return ESP_OK;
}

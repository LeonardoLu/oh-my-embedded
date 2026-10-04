// SPDX-License-Identifier: MIT
#include "mosaico_audio.h"
#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
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
static atomic_uint s_stream;
static atomic_bool s_preview_pending;
static uint32_t s_next_stream;
static uint32_t s_cancel_epoch;
static int s_stream_gain;

#define AUDIO_CHUNK_SAMPLES 160U
#define AUDIO_PREVIEW_SAMPLES 1600U

typedef struct {
    uint8_t *pcm;
    size_t bytes;
    uint32_t stream;
    uint32_t epoch;
    int gain;
} audio_clip_t;

/* The same worker owns the active clip; the mutex protects this single queued
 * clip and all codec calls. Only the cancellation token is lock-free so Lua GC
 * and Quit never wait on a failing codec write. */
static audio_clip_t s_pending_clip;

static bool clip_alive_locked(const audio_clip_t *clip)
{
    return s_volume > 0 && clip->epoch == s_cancel_epoch &&
           (clip->stream == 0 || atomic_load(&s_stream) == clip->stream);
}

static void discard_pending_locked(void)
{
    free(s_pending_clip.pcm);
    memset(&s_pending_clip, 0, sizeof(s_pending_clip));
}

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
    int16_t samples[AUDIO_CHUNK_SAMPLES];
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        for (;;) {
            xSemaphoreTake(s_lock, portMAX_DELAY);
            const bool preview = atomic_exchange(&s_preview_pending, false);
            audio_clip_t clip = {.epoch = s_cancel_epoch, .gain = 100};
            if (!preview) {
                if (!s_pending_clip.pcm) {
                    xSemaphoreGive(s_lock);
                    break;
                }
                clip = s_pending_clip;
                memset(&s_pending_clip, 0, sizeof(s_pending_clip));
            }
            if (!clip_alive_locked(&clip) || clip.gain == 0) {
                free(clip.pcm);
                xSemaphoreGive(s_lock);
                continue;
            }
            esp_codec_dev_sample_info_t format = {
                .sample_rate = MOSAICO_AUDIO_SAMPLE_RATE, .channel = 1, .bits_per_sample = 16};
            int result = esp_codec_dev_open(s_codec, &format);
            s_open = result == ESP_CODEC_DEV_OK;
            i2s_chan_info_t output = {0};
            if (s_open && (i2s_channel_get_info(s_output, &output) != ESP_OK ||
                           !output.is_enabled || output.total_dma_buf_size == 0)) {
                result = ESP_CODEC_DEV_DRV_ERR;
            }
            if (s_open && apply_volume_locked(s_volume) != ESP_OK) result = ESP_CODEC_DEV_DRV_ERR;
            xSemaphoreGive(s_lock);
            const size_t sample_count = preview ? AUDIO_PREVIEW_SAMPLES : clip.bytes / 2U;
            for (size_t first = 0; first < sample_count && result == ESP_CODEC_DEV_OK;
                 first += AUDIO_CHUNK_SAMPLES) {
                memset(samples, 0, sizeof(samples));
                for (size_t index = 0; index < AUDIO_CHUNK_SAMPLES && first + index < sample_count; ++index) {
                    const size_t sample = first + index;
                    if (preview) {
                        const size_t edge = sample < 128U ? sample : sample > 1471U ? 1599U - sample : 128U;
                        samples[index] = (int16_t)(4096.0f * edge / 128.0f *
                            sinf(sample * (2.0f * 3.14159265f * 880.0f / MOSAICO_AUDIO_SAMPLE_RATE)));
                    } else {
                        const uint16_t raw = (uint16_t)clip.pcm[sample * 2U] |
                                             (uint16_t)clip.pcm[sample * 2U + 1U] << 8;
                        samples[index] = (int16_t)((int32_t)(int16_t)raw * clip.gain / 100);
                    }
                }
                xSemaphoreTake(s_lock, portMAX_DELAY);
                if (!clip_alive_locked(&clip)) {
                    xSemaphoreGive(s_lock);
                    break;
                }
                result = esp_codec_dev_write(s_codec, samples, sizeof(samples));
                xSemaphoreGive(s_lock);
            }
            /* Normal completion advances one actual DMA ring with silence.
             * Cancel/mute skips that tail, preventing queued old SFX from being
             * deliberately played after Quit. No idle zero-PCM stream exists. */
            memset(samples, 0, sizeof(samples));
            for (size_t remaining = output.total_dma_buf_size;
                 remaining > 0 && result == ESP_CODEC_DEV_OK;) {
                xSemaphoreTake(s_lock, portMAX_DELAY);
                if (!clip_alive_locked(&clip)) {
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
            /* open() can enable I2S before a later codec step fails. */
            (void)esp_codec_dev_close(s_codec);
            s_open = false;
            xSemaphoreGive(s_lock);
            free(clip.pcm);
            if (result != ESP_CODEC_DEV_OK) ESP_LOGW(TAG, "local audio failed: %d", result);
        }
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
    ESP_LOGI(TAG, "output ready; DAC opens only for local feedback and Works PCM");
    return ESP_OK;
}

bool mosaico_audio_available(void) { return s_task != NULL; }

esp_err_t mosaico_audio_set_volume(int volume)
{
    if (volume < 0 || volume > 100) return ESP_ERR_INVALID_ARG;
    if (!s_task) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(500)) != pdTRUE) return ESP_ERR_TIMEOUT;
    const esp_err_t err = apply_volume_locked(volume);
    if (err == ESP_OK && volume == 0) {
        ++s_cancel_epoch;
        discard_pending_locked();
        atomic_store(&s_preview_pending, false);
    }
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
    atomic_store(&s_preview_pending, true);
    xTaskNotifyGive(s_task);
    return ESP_OK;
}

esp_err_t mosaico_audio_stream_open(int gain_percent, uint32_t *out_stream)
{
    if (!out_stream || gain_percent < 0 || gain_percent > 100) return ESP_ERR_INVALID_ARG;
    *out_stream = 0;
    if (!s_task) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_lock, 0) != pdTRUE) return ESP_ERR_NOT_FINISHED;
    if (atomic_load(&s_stream) != 0) {
        xSemaphoreGive(s_lock);
        return ESP_ERR_NOT_FINISHED;
    }
    if (++s_next_stream == 0) ++s_next_stream;
    s_stream_gain = gain_percent;
    atomic_store(&s_stream, s_next_stream);
    *out_stream = s_next_stream;
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

esp_err_t mosaico_audio_stream_write(uint32_t stream, const void *pcm, size_t bytes)
{
    if (stream == 0 || (!pcm && bytes != 0) || bytes > MOSAICO_AUDIO_PCM_MAX_BYTES || bytes % 2U != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_task || atomic_load(&s_stream) != stream) return ESP_ERR_INVALID_STATE;
    if (bytes == 0) return ESP_OK;
    if (xSemaphoreTake(s_lock, 0) != pdTRUE) return ESP_ERR_NOT_FINISHED;
    esp_err_t err = ESP_OK;
    if (atomic_load(&s_stream) != stream) {
        err = ESP_ERR_INVALID_STATE;
    } else {
        if (s_pending_clip.pcm && s_pending_clip.stream != stream) discard_pending_locked();
        if (s_volume == 0 || s_stream_gain == 0) {
            /* Accept muted PCM without leaving old sound to play on unmute. */
        } else if (s_pending_clip.pcm) {
            err = ESP_ERR_NOT_FINISHED;
        } else {
            uint8_t *copy = malloc(bytes);
            if (!copy) {
                err = ESP_ERR_NO_MEM;
            } else {
                memcpy(copy, pcm, bytes);
                s_pending_clip = (audio_clip_t){
                    .pcm = copy, .bytes = bytes, .stream = stream,
                    .epoch = s_cancel_epoch, .gain = s_stream_gain};
            }
        }
    }
    xSemaphoreGive(s_lock);
    if (err == ESP_OK) xTaskNotifyGive(s_task);
    return err;
}

void mosaico_audio_stream_close(uint32_t stream)
{
    unsigned expected = stream;
    if (stream != 0 && atomic_compare_exchange_strong(&s_stream, &expected, 0) && s_task) {
        xTaskNotifyGive(s_task);
    }
}

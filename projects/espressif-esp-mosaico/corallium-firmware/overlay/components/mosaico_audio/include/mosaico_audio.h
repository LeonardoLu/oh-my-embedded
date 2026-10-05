// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/* Output-only local feedback and Works PCM; no microphone or audio hub. */
#define MOSAICO_AUDIO_PCM_MAX_BYTES 16384U
#define MOSAICO_AUDIO_SAMPLE_RATE 16000U

esp_err_t mosaico_audio_init(void);
bool mosaico_audio_available(void);
esp_err_t mosaico_audio_set_volume(int volume);
int mosaico_audio_get_volume(void);
esp_err_t mosaico_audio_preview(void);

/* One stream, 16 kHz/mono/s16le. A write copies at most one queued clip and
 * returns ESP_ERR_NOT_FINISHED when busy; playback never runs on its caller.
 * The stream gain attenuates PCM without changing the persisted master volume. */
esp_err_t mosaico_audio_stream_open(int gain_percent, uint32_t *out_stream);
esp_err_t mosaico_audio_stream_write(uint32_t stream, const void *pcm, size_t bytes);
/* Idempotent, does not wait for codec I/O. The worker checks cancellation before
 * each 320-byte chunk. A racing write may already have submitted one chunk plus
 * the actual DMA ring; the vendor I2S write fault timeout is 1000 ms. */
void mosaico_audio_stream_close(uint32_t stream);

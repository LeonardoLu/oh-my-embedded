// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include "esp_err.h"

/* Output-only system feedback; the microphone and Music remain disabled. */
esp_err_t mosaico_audio_init(void);
bool mosaico_audio_available(void);
esp_err_t mosaico_audio_set_volume(int volume);
int mosaico_audio_get_volume(void);
esp_err_t mosaico_audio_preview(void);

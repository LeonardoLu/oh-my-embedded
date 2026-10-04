// SPDX-License-Identifier: MIT
#pragma once
#include "esp_err.h"

/* Factory Works' PCM output subset, backed by the local speaker worker. */
esp_err_t works_audio_register(void);

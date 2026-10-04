// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
bool display_service_is_started(void);
esp_err_t display_service_set_rotation(uint16_t degrees);
esp_err_t display_service_get_rotation(uint16_t *degrees);
esp_err_t display_service_set_brightness(uint8_t brightness);
esp_err_t display_service_get_brightness(uint8_t *brightness);
esp_err_t display_service_prepare_brightness_fade(
    uint8_t start, uint8_t target, uint32_t duration_ms);

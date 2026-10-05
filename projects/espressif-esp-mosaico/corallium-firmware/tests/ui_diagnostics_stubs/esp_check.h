/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "esp_err.h"
#define ESP_RETURN_ON_ERROR(expr, tag, ...) do { \
    esp_err_t fixture_error=(expr); (void)(tag); \
    if(fixture_error!=ESP_OK) return fixture_error; \
} while(0)

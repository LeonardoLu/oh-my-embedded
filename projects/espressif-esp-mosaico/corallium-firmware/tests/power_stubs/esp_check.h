// SPDX-License-Identifier: MIT
#pragma once
#include "esp_err.h"
#define ESP_RETURN_ON_ERROR(expr, tag, ...) do { \
    (void)(tag); esp_err_t test_err = (expr); \
    if (test_err != ESP_OK) return test_err; \
} while (0)
#define ESP_RETURN_ON_FALSE(condition, err, tag, ...) do { \
    (void)(tag); if (!(condition)) return (err); \
} while (0)

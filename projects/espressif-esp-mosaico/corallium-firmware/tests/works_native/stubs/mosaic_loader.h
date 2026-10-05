// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include "esp_err.h"
esp_err_t mosaic_loader_invalidate_app(uint16_t app_id, uint32_t revision);

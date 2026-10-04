/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#define TINYUSB_CDC_ACM_0 0
bool tinyusb_cdcacm_initialized(int itf);
size_t tinyusb_cdcacm_write_queue(int itf, const uint8_t *data, size_t len);
esp_err_t tinyusb_cdcacm_write_flush(int itf, uint32_t timeout_ticks);

// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "cJSON.h"

esp_err_t corallium_start(void);
/* The only admission gate: invoked by a physical button or the BLE tile. */
esp_err_t corallium_open_pairing(void);
void corallium_close_pairing(void);
bool corallium_pairing_active(void);
bool corallium_connected(void);
bool corallium_time_valid(void);
cJSON *corallium_dispatch(const cJSON *request);
cJSON *corallium_status(void);
void corallium_clock_init(void);
void corallium_network_changed(bool connected);

// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "cJSON.h"

esp_err_t corallium_start(void);
/* Persistent manual BLE switch; default off. Saved on state resumes on boot.
 * Both operations save first and leave the running switch unchanged on error.
 * close is also safe before BLE initialization (factory-reset path). */
esp_err_t corallium_open_pairing(void);
esp_err_t corallium_close_pairing(void);
bool corallium_pairing_active(void);
bool corallium_connected(void);
bool corallium_time_valid(void);
cJSON *corallium_dispatch(const cJSON *request);
cJSON *corallium_status(void);
void corallium_clock_init(void);
void corallium_network_changed(bool connected);

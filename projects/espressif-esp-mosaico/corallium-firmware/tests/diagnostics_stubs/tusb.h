/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
bool tud_cdc_n_connected(uint8_t itf);
uint32_t tud_cdc_n_write_available(uint8_t itf);
void tud_cdc_n_read_flush(uint8_t itf);

#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { bool available, charging; uint16_t state_of_charge, voltage_mv; int16_t current_ma; uint16_t time_to_empty_min; } mosaic_settings_battery_t;
int mosaic_settings_get_battery(mosaic_settings_battery_t *);
int mosaic_settings_set_wifi_enabled(bool);

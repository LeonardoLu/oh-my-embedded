// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
/* BQ27220 current is positive into the cell. This is cell power, not USB input
 * or whole-system consumption while charging. Runtime is a gauge estimate. */
static inline double corallium_cell_power_mw(uint16_t mv, int16_t ma) {
    return (double)mv * -(double)ma / 1000.0;
}
static inline bool corallium_runtime_available(bool charging, int16_t ma, uint16_t minutes) {
    return !charging && ma < -2 && minutes != UINT16_MAX;
}

/* SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
 * LiquidDuck derivative; see liquidduck/NOTICE.md. */
#pragma once

#include <stddef.h>
#include <stdint.h>

#define IMU_LIQUID_WIDTH 456
#define IMU_LIQUID_HEIGHT 320
#define IMU_LIQUID_PALETTES 8

typedef enum {
    IMU_LIQUID_PIXEL,
    IMU_LIQUID_GRADIENT,
    IMU_LIQUID_WATER,
} imu_liquid_style_t;

typedef struct imu_liquid imu_liquid_t;

imu_liquid_t *imu_liquid_create(imu_liquid_style_t style);
void imu_liquid_destroy(imu_liquid_t *liquid);
void imu_liquid_step(imu_liquid_t *liquid, float dt, float gx, float gy);
void imu_liquid_render(imu_liquid_t *liquid, unsigned palette,
                       uint16_t *pixels, size_t stride_pixels);
const char *imu_liquid_palette_name(unsigned palette);
uint32_t imu_liquid_color(imu_liquid_style_t style, unsigned palette,
                          float density);

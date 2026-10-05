/* SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
 * LiquidDuck derivative; see liquidduck/NOTICE.md. */
#pragma once

#include <stddef.h>
#include <stdint.h>

#define LIQUID_WIDTH 456
#define LIQUID_HEIGHT 320
#define LIQUID_PALETTES 8

typedef enum {
    LIQUID_PIXEL,
    LIQUID_GRADIENT,
    LIQUID_WATER,
} liquid_engine_style_t;

typedef struct liquid_engine liquid_engine_t;

liquid_engine_t *liquid_engine_create(liquid_engine_style_t style);
void liquid_engine_destroy(liquid_engine_t *liquid);
void liquid_engine_step(liquid_engine_t *liquid, float dt, float gx, float gy);
void liquid_engine_render(liquid_engine_t *liquid, unsigned palette,
                       uint16_t *pixels, size_t stride_pixels);
const char *liquid_engine_palette_name(unsigned palette);
uint32_t liquid_engine_color(liquid_engine_style_t style, unsigned palette,
                          float density);

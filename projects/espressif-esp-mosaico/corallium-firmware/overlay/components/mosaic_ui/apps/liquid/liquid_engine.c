/* SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
 * LiquidDuck derivative; see liquidduck/NOTICE.md. */
#include "liquid_engine.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "liquidduck/flip.h"

static const char *const s_palette_names[LIQUID_PALETTES] = {
    "Matrix", "Cyberpunk", "Amber", "Mono", "Red", "Deep Sea", "Toxic", "Gold",
};
static const uint32_t s_palettes[LIQUID_PALETTES][7] = {
    {0x000500, 0x002200, 0x004400, 0x008800, 0x00CC00, 0x00FF00, 0x88FF88},
    {0x050005, 0x220033, 0x550066, 0x990099, 0xFF00FF, 0x00FFFF, 0xFFFFFF},
    {0x0C0500, 0x331100, 0x662200, 0x994400, 0xCC6600, 0xFF8800, 0xFFBB66},
    {0x080808, 0x222222, 0x444444, 0x777777, 0xAAAAAA, 0xDDDDDD, 0xFFFFFF},
    {0x0A0000, 0x330000, 0x660000, 0x990000, 0xCC0000, 0xFF0000, 0xFF8888},
    {0x000511, 0x001133, 0x002266, 0x0044AA, 0x0088FF, 0x00CCFF, 0xAAFFFF},
    {0x08000C, 0x1E002E, 0x3C005C, 0x5A008A, 0x7800B8, 0x39FF14, 0xE5FF00},
    {0x0D0B00, 0x332900, 0x5C4A00, 0x856B00, 0xAD8C00, 0xD6AD00, 0xFFF5CC},
};

struct liquid_engine {
    FlipFluid *fluid;
    liquid_engine_style_t style;
    int cols;
    int rows;
    float spacing;
    float grid[28 * 20];
};

static float clampf(float value, float low, float high)
{
    return value < low ? low : value > high ? high : value;
}

static uint16_t rgb565(uint32_t color)
{
    return ((color >> 8) & 0xf800) | ((color >> 5) & 0x07e0) |
           ((color >> 3) & 0x001f);
}

const char *liquid_engine_palette_name(unsigned palette)
{
    return s_palette_names[palette % LIQUID_PALETTES];
}

uint32_t liquid_engine_color(liquid_engine_style_t style, unsigned palette,
                          float density)
{
    const uint32_t *colors = s_palettes[palette % LIQUID_PALETTES];
    if (!isfinite(density) || density < (style == LIQUID_GRADIENT ? 0.5f : 0.1f)) {
        return colors[0];
    }
    if (style == LIQUID_PIXEL) return colors[5];
    const float range = style == LIQUID_GRADIENT ? 4.0f : 20.0f;
    const int index = (int)clampf(density * 6.0f / range, 1.0f, 6.0f);
    return colors[index];
}

liquid_engine_t *liquid_engine_create(liquid_engine_style_t style)
{
    if (style < LIQUID_PIXEL || style > LIQUID_WATER) return NULL;
    liquid_engine_t *liquid = calloc(1, sizeof(*liquid));
    if (liquid == NULL) return NULL;
    liquid->style = style;
    liquid->cols = style == LIQUID_WATER ? 28 : 14;
    liquid->rows = style == LIQUID_WATER ? 20 : 10;
    const float height = (float)(liquid->rows + 1) / (liquid->cols + 1);
    liquid->spacing = 1.0f / (liquid->cols + 1);
    liquid->fluid = flip_create(1.0f, height, liquid->cols, liquid->rows,
                               style == LIQUID_WATER ? 0.45f : 0.60f);
    if (liquid->fluid == NULL) {
        free(liquid);
        return NULL;
    }
    flip_set_gravity_scale(liquid->fluid, style == LIQUID_WATER ? 16.0f : 24.0f);
    flip_set_solver_quality(liquid->fluid, 1, style == LIQUID_WATER ? 12 : 16,
                            style == LIQUID_WATER ? 0.94f : 0.988f);
    int count = 0;
    float *positions = NULL;
    flip_get_particles(liquid->fluid, &count, &positions, NULL);
    for (int i = 0; i < count; ++i) {
        positions[2 * i + 1] = (liquid->rows + 2) * liquid->spacing - positions[2 * i + 1];
    }
    liquid_engine_step(liquid, 1.0f / 120.0f, 0, 1);
    return liquid;
}

void liquid_engine_destroy(liquid_engine_t *liquid)
{
    if (liquid == NULL) return;
    flip_destroy(liquid->fluid);
    free(liquid);
}

void liquid_engine_step(liquid_engine_t *liquid, float dt, float gx, float gy)
{
    if (liquid == NULL || !isfinite(dt) || dt <= 0 || !isfinite(gx) || !isfinite(gy)) return;
    dt = clampf(dt, 0, 0.05f);
    gx = clampf(gx, -2, 2);
    gy = clampf(gy, -2, 2);
    flip_step(liquid->fluid, dt * 0.5f, gx, gy);
    flip_step(liquid->fluid, dt * 0.5f, gx, gy);
}

static void update_grid(liquid_engine_t *liquid)
{
    if (liquid->style == LIQUID_WATER) {
        flip_get_led_grid(liquid->fluid, liquid->grid, liquid->cols, liquid->rows);
        return;
    }
    memset(liquid->grid, 0, sizeof(liquid->grid));
    int count = 0;
    float *positions = NULL;
    flip_get_particles(liquid->fluid, &count, &positions, NULL);
    for (int i = 0; i < count; ++i) {
        const int x = (int)clampf(positions[2 * i] / liquid->spacing - 1,
                                  0, liquid->cols - 1);
        const int y = (int)clampf(positions[2 * i + 1] / liquid->spacing - 1,
                                  0, liquid->rows - 1);
        liquid->grid[x * liquid->rows + y] += 1;
    }
}

void liquid_engine_render(liquid_engine_t *liquid, unsigned palette,
                       uint16_t *pixels, size_t stride_pixels)
{
    if (liquid == NULL || pixels == NULL || stride_pixels < LIQUID_WIDTH) return;
    update_grid(liquid);
    uint16_t colors[7];
    for (int i = 0; i < 7; ++i) colors[i] = rgb565(s_palettes[palette % LIQUID_PALETTES][i]);
    for (int y = 0; y < LIQUID_HEIGHT; ++y) {
        for (int x = 0; x < LIQUID_WIDTH; ++x) pixels[y * stride_pixels + x] = colors[0];
    }
    if (liquid->style != LIQUID_WATER) {
        for (int col = 0; col < liquid->cols; ++col) {
            for (int row = 0; row < liquid->rows; ++row) {
                const float density = liquid->grid[col * liquid->rows + row];
                if (density == 0) continue;
                const uint16_t color = rgb565(liquid_engine_color(liquid->style, palette, density));
                const int left = col * LIQUID_WIDTH / liquid->cols + 2;
                const int right = (col + 1) * LIQUID_WIDTH / liquid->cols - 2;
                const int top = row * LIQUID_HEIGHT / liquid->rows + 2;
                const int bottom = (row + 1) * LIQUID_HEIGHT / liquid->rows - 2;
                for (int y = top; y < bottom; ++y) {
                    const int edge = y - top < bottom - 1 - y ? y - top : bottom - 1 - y;
                    const int inset = edge == 0 ? 3 : edge < 3 ? 1 : 0;
                    for (int x = left + inset; x < right - inset; ++x) pixels[y * stride_pixels + x] = color;
                }
            }
        }
        return;
    }
    for (int y = 0; y < LIQUID_HEIGHT; ++y) {
        const float sy = clampf((y + 0.5f) * liquid->rows / LIQUID_HEIGHT - 0.5f, 0, liquid->rows - 1);
        const int y0 = (int)sy;
        const int y1 = y0 + 1 < liquid->rows ? y0 + 1 : y0;
        const float fy = sy - y0;
        for (int x = 0; x < LIQUID_WIDTH; ++x) {
            const float sx = clampf((x + 0.5f) * liquid->cols / LIQUID_WIDTH - 0.5f, 0, liquid->cols - 1);
            const int x0 = (int)sx;
            const int x1 = x0 + 1 < liquid->cols ? x0 + 1 : x0;
            const float fx = sx - x0;
            const float a = liquid->grid[x0 * liquid->rows + y0];
            const float b = liquid->grid[x1 * liquid->rows + y0];
            const float c = liquid->grid[x0 * liquid->rows + y1];
            const float d = liquid->grid[x1 * liquid->rows + y1];
            const float density = (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy;
            const int index = density < 0.1f ? 0 : (int)clampf(density * 6 / 20, 1, 6);
            pixels[y * stride_pixels + x] = colors[index];
        }
    }
}

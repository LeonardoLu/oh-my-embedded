// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define WEATHER_DOTS_SIZE 169U
#define WEATHER_DOTS_GRID 21
#define WEATHER_DOTS_BG UINT16_C(0x2124)

typedef enum {
    WEATHER_DOTS_UNKNOWN,
    WEATHER_DOTS_CLEAR,
    WEATHER_DOTS_FAIR,
    WEATHER_DOTS_CLOUD,
    WEATHER_DOTS_RAIN,
    WEATHER_DOTS_SNOW,
    WEATHER_DOTS_SLEET,
    WEATHER_DOTS_FOG,
    WEATHER_DOTS_THUNDER,
} weather_dots_kind_t;

static inline weather_dots_kind_t weather_dots_classify(const char *symbol,
                                                       bool valid)
{
    if (!valid || symbol == NULL || symbol[0] == '\0') return WEATHER_DOTS_UNKNOWN;
    if (strstr(symbol, "thunder")) return WEATHER_DOTS_THUNDER;
    if (strstr(symbol, "sleet")) return WEATHER_DOTS_SLEET;
    if (strstr(symbol, "snow")) return WEATHER_DOTS_SNOW;
    if (strstr(symbol, "rain")) return WEATHER_DOTS_RAIN;
    if (strstr(symbol, "fog")) return WEATHER_DOTS_FOG;
    if (strstr(symbol, "partlycloudy") || strstr(symbol, "fair")) return WEATHER_DOTS_FAIR;
    if (strstr(symbol, "cloudy")) return WEATHER_DOTS_CLOUD;
    if (strstr(symbol, "clearsky")) return WEATHER_DOTS_CLEAR;
    return WEATHER_DOTS_UNKNOWN;
}

static inline bool weather_dots_animated(weather_dots_kind_t kind)
{
    return kind == WEATHER_DOTS_RAIN || kind == WEATHER_DOTS_SNOW ||
           kind == WEATHER_DOTS_SLEET || kind == WEATHER_DOTS_THUNDER;
}

static inline bool weather_dots_circle(int x, int y, int cx, int cy, int radius)
{
    return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= radius * radius;
}

/* Symbol geometry stays on a fixed dot grid; precipitation moves one row
 * per phase without allocating scene nodes or inventing weather measurements. */
static inline uint16_t weather_dots_cell(weather_dots_kind_t kind, bool night,
                                         unsigned phase, int x, int y)
{
    const uint16_t cloud = UINT16_C(0xEF7D);
    if (kind == WEATHER_DOTS_UNKNOWN) {
        static const uint8_t question[] = {14, 17, 1, 2, 4, 4, 0, 4};
        if (y >= 6 && y < 14 && x >= 8 && x < 13 &&
            (question[y - 6] & (1U << (12 - x)))) return UINT16_C(0x9493);
        return 0;
    }
    if (kind == WEATHER_DOTS_CLEAR || kind == WEATHER_DOTS_FAIR) {
        const int cx = kind == WEATHER_DOTS_CLEAR ? 10 : 7;
        const int cy = kind == WEATHER_DOTS_CLEAR ? 10 : 6;
        if (night) {
            if (weather_dots_circle(x, y, cx, cy, 5) &&
                !weather_dots_circle(x, y, cx + 3, cy - 2, 5)) return UINT16_C(0xADBF);
            if ((x == cx + 7 && y == cy - 4) || (x == cx + 6 && y == cy + 3))
                return cloud;
        } else {
            const int dx = x - cx, dy = y - cy;
            if (weather_dots_circle(x, y, cx, cy, 3) ||
                (dx == 0 && (dy == -6 || dy == -5 || dy == 5 || dy == 6)) ||
                (dy == 0 && (dx == -6 || dx == -5 || dx == 5 || dx == 6)) ||
                ((dx == -4 || dx == 4) && (dy == -4 || dy == 4)))
                return UINT16_C(0xFA60);
        }
    }
    if (kind == WEATHER_DOTS_CLEAR) return 0;
    if (kind == WEATHER_DOTS_FOG) {
        if ((y == 7 || y == 10 || y == 13 || y == 16) &&
            x >= 4 + (y % 2) && x <= 16 - (y % 3)) return cloud;
        return 0;
    }
    if (weather_dots_circle(x, y, 7, 10, 3) ||
        weather_dots_circle(x, y, 11, 8, 4) ||
        weather_dots_circle(x, y, 15, 10, 3) ||
        (x >= 7 && x <= 15 && y >= 10 && y <= 12)) return cloud;
    if (y < 14 || y > 19) return 0;
    if (kind == WEATHER_DOTS_THUNDER) {
        const int bolt_x = y <= 16 ? 12 - (y - 14) : 13 - (y - 17);
        if (x == bolt_x || x == bolt_x + 1) return UINT16_C(0xFDE0);
    }
    if (kind == WEATHER_DOTS_RAIN || kind == WEATHER_DOTS_THUNDER ||
        kind == WEATHER_DOTS_SLEET) {
        if ((x == 5 || x == 9 || x == 16) && (y + phase + x) % 4 < 2)
            return UINT16_C(0x563F);
    }
    if (kind == WEATHER_DOTS_SNOW || kind == WEATHER_DOTS_SLEET) {
        if ((x == 6 || x == 11 || x == 15) && (y + phase + x) % 5 == 0)
            return cloud;
    }
    return 0;
}

static inline void weather_dots_render(uint16_t *pixels, weather_dots_kind_t kind,
                                       bool night, unsigned phase)
{
    for (unsigned y = 0; y < WEATHER_DOTS_SIZE; ++y) {
        for (unsigned x = 0; x < WEATHER_DOTS_SIZE; ++x) {
            pixels[y * WEATHER_DOTS_SIZE + x] = WEATHER_DOTS_BG;
        }
    }
    for (int y = 0; y < WEATHER_DOTS_GRID; ++y) {
        for (int x = 0; x < WEATHER_DOTS_GRID; ++x) {
            const uint16_t color = weather_dots_cell(kind, night, phase, x, y);
            if (color == 0) continue;
            const int cx = 14 + x * 7, cy = 14 + y * 7;
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    if (dx * dx + dy * dy <= 5)
                        pixels[(cy + dy) * WEATHER_DOTS_SIZE + cx + dx] = color;
                }
            }
        }
    }
}

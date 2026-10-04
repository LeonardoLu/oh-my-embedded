// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include "weather_dots.h"

static uint16_t first[WEATHER_DOTS_SIZE * WEATHER_DOTS_SIZE];
static uint16_t second[WEATHER_DOTS_SIZE * WEATHER_DOTS_SIZE];

int main(void)
{
    assert(weather_dots_classify("clearsky_day", true) == WEATHER_DOTS_CLEAR);
    assert(weather_dots_classify("fair_night", true) == WEATHER_DOTS_FAIR);
    assert(weather_dots_classify("partlycloudy_polartwilight", true) == WEATHER_DOTS_FAIR);
    assert(weather_dots_classify("cloudy", true) == WEATHER_DOTS_CLOUD);
    assert(weather_dots_classify("lightrainshowers_day", true) == WEATHER_DOTS_RAIN);
    assert(weather_dots_classify("heavysnow", true) == WEATHER_DOTS_SNOW);
    assert(weather_dots_classify("sleetshowers_night", true) == WEATHER_DOTS_SLEET);
    assert(weather_dots_classify("fog", true) == WEATHER_DOTS_FOG);
    assert(weather_dots_classify("rainandthunder", true) == WEATHER_DOTS_THUNDER);
    assert(weather_dots_classify(NULL, true) == WEATHER_DOTS_UNKNOWN);
    assert(weather_dots_classify("clearsky_day", false) == WEATHER_DOTS_UNKNOWN);
    assert(weather_dots_classify("unsupported", true) == WEATHER_DOTS_UNKNOWN);
    for (unsigned kind = WEATHER_DOTS_UNKNOWN; kind <= WEATHER_DOTS_THUNDER; ++kind) {
        weather_dots_render(first, kind, false, 0);
        weather_dots_render(second, kind, false, 1);
        unsigned lit = 0;
        for (unsigned y = 0; y < WEATHER_DOTS_SIZE; ++y) {
            for (unsigned x = 0; x < WEATHER_DOTS_SIZE; ++x) {
                const uint16_t pixel = first[y * WEATHER_DOTS_SIZE + x];
                if (x < 8 || x > 160 || y < 8 || y > 160) assert(pixel == WEATHER_DOTS_BG);
                lit += pixel != WEATHER_DOTS_BG;
            }
        }
        assert(lit > 0);
        assert((memcmp(first, second, sizeof(first)) != 0) == weather_dots_animated(kind));
    }
    weather_dots_render(first, WEATHER_DOTS_CLEAR, false, 0);
    weather_dots_render(second, WEATHER_DOTS_CLEAR, true, 0);
    assert(memcmp(first, second, sizeof(first)) != 0);
    puts("Weather symbols: unavailable, MET families, day/night, bounded dot frames and precipitation phases verified");
    return 0;
}

/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <string.h>

typedef enum {
    MOSAIC_WEATHER_ART_OVERCAST,
    MOSAIC_WEATHER_ART_SUNNY,
    MOSAIC_WEATHER_ART_CLOUDY,
    MOSAIC_WEATHER_ART_SNOW,
    MOSAIC_WEATHER_ART_WINDY,
    MOSAIC_WEATHER_ART_THUNDER,
} mosaic_weather_art_t;

/* Home and Weather share the factory art, including its day/night fallback.
 * Select one layer even when the provider combines snow, rain and thunder. */
static inline mosaic_weather_art_t mosaic_weather_art_select(
    const char *symbol, bool valid)
{
    if (!valid || symbol == NULL) return MOSAIC_WEATHER_ART_OVERCAST;
    if (strstr(symbol, "thunder") || strstr(symbol, "rain")) return MOSAIC_WEATHER_ART_THUNDER;
    if (strstr(symbol, "snow") || strstr(symbol, "sleet")) return MOSAIC_WEATHER_ART_SNOW;
    if (strstr(symbol, "wind")) return MOSAIC_WEATHER_ART_WINDY;
    if (strstr(symbol, "clear") || strstr(symbol, "fair")) return MOSAIC_WEATHER_ART_SUNNY;
    if (strstr(symbol, "partlycloudy")) return MOSAIC_WEATHER_ART_CLOUDY;
    return MOSAIC_WEATHER_ART_OVERCAST;
}

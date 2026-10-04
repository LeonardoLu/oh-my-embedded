// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include "mosaic_weather_art.h"

int main(void)
{
    const struct { const char *symbol; mosaic_weather_art_t art; } cases[] = {
        {"clearsky_day", MOSAIC_WEATHER_ART_SUNNY},
        {"clearsky_night", MOSAIC_WEATHER_ART_SUNNY},
        {"fair_polartwilight", MOSAIC_WEATHER_ART_SUNNY},
        {"partlycloudy_night", MOSAIC_WEATHER_ART_CLOUDY},
        {"cloudy", MOSAIC_WEATHER_ART_OVERCAST},
        {"fog", MOSAIC_WEATHER_ART_OVERCAST},
        {"lightrainshowers_day", MOSAIC_WEATHER_ART_THUNDER},
        {"rainandthunder", MOSAIC_WEATHER_ART_THUNDER},
        {"snowshowersandthunder_day", MOSAIC_WEATHER_ART_THUNDER},
        {"sleetshowersandthunder_night", MOSAIC_WEATHER_ART_THUNDER},
        {"heavysnow", MOSAIC_WEATHER_ART_SNOW},
        {"sleetshowers_night", MOSAIC_WEATHER_ART_SNOW},
        {"wind", MOSAIC_WEATHER_ART_WINDY},
        {"", MOSAIC_WEATHER_ART_OVERCAST},
        {"unsupported", MOSAIC_WEATHER_ART_OVERCAST},
        {NULL, MOSAIC_WEATHER_ART_OVERCAST},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        assert(mosaic_weather_art_select(cases[i].symbol, true) == cases[i].art);
        assert(mosaic_weather_art_select(cases[i].symbol, false) == MOSAIC_WEATHER_ART_OVERCAST);
    }
    puts("PASS: shared Weather/Home selection handles MET families, combined precipitation, night and unavailable data");
    return 0;
}

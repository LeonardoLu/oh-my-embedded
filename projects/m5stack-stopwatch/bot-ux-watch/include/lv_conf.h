#pragma once
#define LV_CONF_H
#include <esp_heap_caps.h>
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_ALLOC(size) heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define LV_MEM_CUSTOM_FREE(ptr) heap_caps_free(ptr)
#define LV_MEM_CUSTOM_REALLOC(ptr, size) heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define LV_TICK_CUSTOM 0
#define LV_DISP_DEF_REFR_PERIOD 16
#define LV_INDEV_DEF_READ_PERIOD 8
#define LV_DPI_DEF 160
#define LV_USE_LOG 0
#define LV_USE_PERF_MONITOR 0
#define LV_FONT_MONTSERRAT_14 1
#define LV_USE_THEME_DEFAULT 1
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0
#define LV_USE_DEMO_WIDGETS 0

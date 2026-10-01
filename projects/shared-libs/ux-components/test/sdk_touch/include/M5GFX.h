#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstring>
#include <cstdlib>
namespace m5gfx {
struct touch_point_t {int16_t x=0,y=0;uint8_t id=0,size=0;};
struct LGFX_Device {size_t getTouchRaw(touch_point_t*,size_t){return 0;}void convertRawXY(touch_point_t*,size_t){}};
}

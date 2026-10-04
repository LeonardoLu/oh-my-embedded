#pragma once
#include "UxRender.h"
namespace ux {
namespace space { constexpr int tiny=4, small=8, medium=12, large=16, section=24; }
namespace motion { constexpr unsigned pressMs=90, settleMs=180, pageMs=240;
inline float easeOut(float t) { t=clamp(t,0,1); return 1-(1-t)*(1-t)*(1-t); }
inline float approach(float current,float target,float dtMs,float timeMs=settleMs) {
    return current+(target-current)*(1-expf(-fmaxf(dtMs,0)/fmaxf(timeMs,1)));
}}
}

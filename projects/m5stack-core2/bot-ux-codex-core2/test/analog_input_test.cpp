#include "AnalogInput.h"

#include <assert.h>
#include <math.h>

namespace {
bool near(float actual, float expected)
{
    return fabsf(actual - expected) < 0.0001f;
}
}

int main()
{
    AnalogPosition p = AnalogInput::fromOffset(30.0f, 0.0f, 60.0f, 5.0f);
    assert(p.active && near(p.angle, 0.0f) && near(p.distance, 0.5f));
    p = AnalogInput::fromOffset(0.0f, 30.0f, 60.0f, 5.0f);
    assert(p.active && near(p.angle, 0.25f));
    p = AnalogInput::fromOffset(-30.0f, 0.0f, 60.0f, 5.0f);
    assert(p.active && near(p.angle, 0.5f));
    p = AnalogInput::fromOffset(0.0f, -30.0f, 60.0f, 5.0f);
    assert(p.active && near(p.angle, 0.75f));
    p = AnalogInput::fromOffset(2.0f, 2.0f, 60.0f, 5.0f);
    assert(!p.active && near(p.distance, 0.0f));
    p = AnalogInput::fromOffset(100.0f, 0.0f, 60.0f, 5.0f);
    assert(p.active && near(p.distance, 1.0f));
    return 0;
}

#include "AnalogInput.h"

#include <math.h>

AnalogPosition AnalogInput::fromOffset(float dx, float dy, float radius, float deadzone)
{
    const float magnitude = sqrtf(dx * dx + dy * dy);
    if (radius <= 0.0f || magnitude <= deadzone) return {0.0f, 0.0f, false};
    float angle = atan2f(dy, dx) / (2.0f * 3.14159265358979323846f);
    if (angle < 0.0f) angle += 1.0f;
    float distance = magnitude / radius;
    if (distance > 1.0f) distance = 1.0f;
    return {angle, distance, true};
}

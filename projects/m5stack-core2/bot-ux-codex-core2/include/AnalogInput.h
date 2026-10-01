#pragma once

struct AnalogPosition {
    float angle;
    float distance;
    bool active;
};

class AnalogInput {
public:
    // App units: angle 0=right, .25=down, .5=left, .75=up; distance 0..1.
    static AnalogPosition fromOffset(float dx, float dy, float radius, float deadzone);
};

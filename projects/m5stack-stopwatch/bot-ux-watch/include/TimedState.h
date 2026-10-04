#pragma once

#include <stdint.h>

class TimedState {
public:
    void start(uint32_t nowMs, uint32_t durationMs) {
        _untilMs = nowMs + durationMs;
        _active = true;
    }

    bool active(uint32_t nowMs) {
        if (!_active) return false;
        if ((int32_t)(_untilMs - nowMs) > 0) return true;
        _active = false;
        return false;
    }

private:
    uint32_t _untilMs = 0;
    bool _active = false;
};

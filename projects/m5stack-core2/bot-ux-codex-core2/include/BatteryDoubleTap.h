#pragma once

#include <stdint.h>

class BatteryDoubleTap {
public:
    static constexpr uint32_t kWindowMs = 420;

    bool tap(uint32_t nowMs)
    {
        if (_armed && nowMs - _firstTapMs <= kWindowMs)
        {
            _armed = false;
            return true;
        }
        _firstTapMs = nowMs;
        _armed = true;
        return false;
    }

    void cancel() { _armed = false; }
    bool armed() const { return _armed; }

private:
    uint32_t _firstTapMs = 0;
    bool _armed = false;
};

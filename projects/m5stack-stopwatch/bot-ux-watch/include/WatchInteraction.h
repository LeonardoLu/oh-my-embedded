#pragma once

#include <math.h>
#include <stdint.h>

namespace watchinteraction {

struct Rgb8 { uint8_t r, g, b; };

struct GazeVector { float x, y; };

inline GazeVector touchGaze(int16_t x, int16_t y, int16_t cx, int16_t cy, int16_t bodyR) {
    int32_t dx=(int32_t)x-cx,dy=(int32_t)y-cy;
    if(bodyR>0&&dx*dx+dy*dy<=(int32_t)bodyR*bodyR) return {0.0f,0.0f};
    float length=sqrtf((float)dx*dx+(float)dy*dy);
    return length>0.0f?GazeVector{dx/length,dy/length}:GazeVector{0.0f,0.0f};
}

inline Rgb8 hsvRgb(uint16_t hue, uint8_t saturation, uint8_t value) {
    hue %= 360;
    if (saturation > 100) saturation = 100;
    if (value > 100) value = 100;
    uint16_t v = (uint16_t)value * 255 / 100;
    uint16_t c = v * saturation / 100;
    uint16_t part = hue % 60;
    uint16_t x = (hue / 60) % 2 == 0 ? c * part / 60 : c * (60 - part) / 60;
    uint16_t m = v - c;
    uint16_t r = 0, g = 0, b = 0;
    switch (hue / 60) {
        case 0: r = c; g = x; break;
        case 1: r = x; g = c; break;
        case 2: g = c; b = x; break;
        case 3: g = x; b = c; break;
        case 4: r = x; b = c; break;
        default: r = c; b = x; break;
    }
    return {(uint8_t)(r + m), (uint8_t)(g + m), (uint8_t)(b + m)};
}

class SettingsChord {
public:
    bool poll(bool aPressed, bool bPressed, uint32_t nowMs) {
        if (!(aPressed && bPressed)) {
            _tracking = false;
            _fired = false;
            return false;
        }
        if (!_tracking) {
            _tracking = true;
            _startedMs = nowMs;
        }
        if (!_fired && nowMs - _startedMs >= 3000) {
            _fired = true;
            return true;
        }
        return false;
    }

    bool tracking() const { return _tracking; }

private:
    uint32_t _startedMs = 0;
    bool _tracking = false;
    bool _fired = false;
};

class ScrollList {
public:
    void configure(uint8_t count, uint8_t visible) {
        _count = count;
        _visible = visible ? visible : 1;
        if (_selected >= _count) _selected = _count ? _count - 1 : 0;
        reveal(_selected);
    }

    void select(uint8_t item) {
        if (!_count) return;
        _selected = item < _count ? item : _count - 1;
        reveal(_selected);
    }

    void move(int8_t delta) {
        if (!_count || !delta) return;
        int next = (int)_selected + delta;
        if (next < 0) next = 0;
        if (next >= _count) next = _count - 1;
        select((uint8_t)next);
    }

    uint8_t selected() const { return _selected; }
    uint8_t first() const { return _first; }
    uint8_t visible() const { return _visible; }

private:
    void reveal(uint8_t item) {
        if (item < _first) _first = item;
        if (item >= _first + _visible) _first = item - _visible + 1;
        uint8_t maxFirst = _count > _visible ? _count - _visible : 0;
        if (_first > maxFirst) _first = maxFirst;
    }

    uint8_t _count = 0;
    uint8_t _visible = 1;
    uint8_t _selected = 0;
    uint8_t _first = 0;
};

class MotionFilter {
public:
    struct Result {
        float x;
        float y;
        float shake;
        bool poke;
    };

    Result update(float x, float y, float gyroMagnitude, uint32_t nowMs) {
        _x += (deadzone(x) - _x) * 0.10f;
        _y += (deadzone(y) - _y) * 0.10f;
        float targetShake = gyroMagnitude > 65.0f ? (gyroMagnitude - 65.0f) / 250.0f : 0.0f;
        if (targetShake > 1.0f) targetShake = 1.0f;
        _shake += (targetShake - _shake) * 0.16f;

        bool poke = false;
        if (!_armed && _shake < 0.16f) _armed = true;
        if (_armed && _shake > 0.62f && nowMs - _lastPokeMs >= 7000) {
            _armed = false;
            _lastPokeMs = nowMs;
            poke = true;
        }
        return { _x, _y, _shake, poke };
    }

    void reset() { _x = _y = _shake = 0.0f; _armed = false; }

private:
    static float deadzone(float value) {
        if (value > -0.10f && value < 0.10f) return 0.0f;
        float adjusted = value > 0.0f ? value - 0.10f : value + 0.10f;
        adjusted /= 0.90f;
        if (adjusted < -1.0f) return -1.0f;
        if (adjusted > 1.0f) return 1.0f;
        return adjusted;
    }

    float _x = 0.0f, _y = 0.0f, _shake = 0.0f;
    uint32_t _lastPokeMs = (uint32_t)-7000;
    bool _armed = false;
};

class AmbientCycle {
public:
    enum class Phase : uint8_t { Safe, LookingAround };

    explicit AmbientCycle(uint32_t seed = 0x5A17u) : _seed(seed) {}

    void begin(uint32_t nowMs) {
        _paused = false;
        restart(nowMs);
    }

    void restart(uint32_t nowMs) {
        _phase = Phase::Safe;
        _index = 0; // Idle is the visible entry into every automatic session.
        schedule(nowMs, 5000, 15000);
    }

    void setPaused(bool paused, uint32_t nowMs) {
        if (paused == _paused) return;
        _paused = paused;
        if (!_paused) restart(nowMs);
    }

    bool update(uint32_t nowMs) {
        if (_paused) return false;
        if ((int32_t)(nowMs - _nextMs) < 0) return false;
        if (_phase == Phase::Safe) {
            _phase = Phase::LookingAround;
            schedule(nowMs, 30000, 60000);
        } else {
            _phase = Phase::Safe;
            _index = (uint8_t)(randomValue() % 7);
            schedule(nowMs, 5000, 15000);
        }
        return true;
    }

    Phase phase() const { return _phase; }
    bool paused() const { return _paused; }
    uint8_t index() const { return _index; }
    uint32_t nextMs() const { return _nextMs; }

private:
    uint32_t randomValue() {
        _seed = _seed * 1664525u + 1013904223u;
        return _seed;
    }

    void schedule(uint32_t nowMs, uint32_t minimumMs, uint32_t maximumMs) {
        _nextMs = nowMs + minimumMs + randomValue() % (maximumMs - minimumMs + 1);
    }

    uint32_t _nextMs = 0;
    uint32_t _seed;
    uint8_t _index = 0;
    Phase _phase = Phase::Safe;
    bool _paused = true;
};

} // namespace watchinteraction

namespace watchinteraction {
class SingleDoubleClick {
public:
    enum class Event { None, Single, Double };
    Event poll(bool release, uint32_t now) {
        if (release && _pending && now - _at <= 320) { _pending = false; return Event::Double; }
        bool single = _pending && now - _at > 320;
        if (single) _pending = false;
        if (release) { _pending = true; _at = now; }
        return single ? Event::Single : Event::None;
    }
    void cancel() { _pending = false; }
private:
    bool _pending = false;
    uint32_t _at = 0;
};
}

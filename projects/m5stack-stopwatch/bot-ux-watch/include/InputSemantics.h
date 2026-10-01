#pragma once

#include <stdint.h>

namespace watchinput {

enum class Gesture : uint8_t { None, Tap, Long, SwipeUp, SwipeDown, SwipeLeft, SwipeRight };

class ButtonGesture {
public:
    Gesture poll(bool wasPressed, bool isPressed, bool wasReleased, uint32_t nowMs) {
        if (wasPressed) {
            _downMs = nowMs;
            _longSent = false;
        }
        if (isPressed && !_longSent && nowMs - _downMs >= kLongMs) {
            _longSent = true;
            return Gesture::Long;
        }
        if (wasReleased && !_longSent && nowMs - _downMs <= 1000) return Gesture::Tap;
        return Gesture::None;
    }

    void consume(bool isPressed) { _longSent = isPressed; }

private:
    static constexpr uint32_t kLongMs = 2000;
    uint32_t _downMs = 0;
    bool _longSent = false;
};

class TouchGesture {
public:
    Gesture poll(bool wasPressed, bool isPressed, bool wasReleased,
                 int16_t x, int16_t y, uint32_t nowMs, uint32_t longMs=2000) {
        if (wasPressed) {
            _down = true;
            _moved = _longSent = _swipeSent = false;
            _x0 = _x = x;
            _y0 = _y = y;
            _downMs = nowMs;
            return Gesture::None;
        }
        if (isPressed && _down) {
            _x = x;
            _y = y;
            int16_t dx = _x - _x0;
            int16_t dy = _y - _y0;
            if (magnitude(dx) > 10 || magnitude(dy) > 10) _moved = true;
            if (_moved && !_longSent && !_swipeSent
                && magnitude(dy) > 36 && magnitude(dy) > magnitude(dx)) {
                _swipeSent = true;
                return dy < 0 ? Gesture::SwipeUp : Gesture::SwipeDown;
            }
            if (_moved && !_longSent && !_swipeSent
                && magnitude(dx) > 36 && magnitude(dx) > magnitude(dy)) {
                _swipeSent = true;
                return dx < 0 ? Gesture::SwipeLeft : Gesture::SwipeRight;
            }
            if (!_moved && !_longSent && nowMs - _downMs >= longMs) {
                _longSent = true;
                return Gesture::Long;
            }
        }
        if (wasReleased && _down) {
            _x = x; _y = y;
            if (magnitude(x - _x0) > 10 || magnitude(y - _y0) > 10) _moved = true;
            _down = false;
            if (!_longSent && !_swipeSent && nowMs - _downMs <= 1000) {
                _tapX = _x;
                _tapY = _y;
                return Gesture::Tap;
            }
        }
        return Gesture::None;
    }

    void consume() {
        _down = false;
        _moved = false;
        _longSent = false;
        _swipeSent = false;
    }

    int16_t tapX() const { return _tapX; }
    int16_t tapY() const { return _tapY; }
    int16_t startX() const { return _x0; }
    int16_t startY() const { return _y0; }

private:
    static int32_t magnitude(int16_t value) {
        return value < 0 ? -(int32_t)value : value;
    }

    bool _down = false;
    bool _moved = false;
    bool _longSent = false;
    bool _swipeSent = false;
    int16_t _x0 = 0, _y0 = 0, _x = 0, _y = 0;
    int16_t _tapX = 0, _tapY = 0;
    uint32_t _downMs = 0;
};

} // namespace watchinput

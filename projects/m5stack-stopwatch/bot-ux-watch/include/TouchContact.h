#pragma once
#include <stdint.h>

namespace watchinput {
// Edges belong to physical contact, never to an SDK flick/hold classification.
// A release has no coordinates, so retain the last valid converted point.
class TouchContact {
public:
    void sample(bool contact, int16_t x, int16_t y) {
        _began = contact && !_contact;
        _ended = !contact && _contact;
        _contact = contact;
        if (contact) { this->x=x; this->y=y; }
    }
    bool wasPressed() const { return _began; }
    bool isPressed() const { return _contact; }
    bool wasReleased() const { return _ended; }
    int16_t x=0, y=0;
private:
    bool _contact=false, _began=false, _ended=false;
};

class RegionDoubleTap {
public:
    bool tap(bool inside, uint32_t now) {
        if (!inside) { reset(); return false; }
        if (_waiting && now-_first<=420) { reset(); return true; }
        _waiting=true; _first=now; return false;
    }
    void reset() { _waiting=false; }
private:
    bool _waiting=false;
    uint32_t _first=0;
};
}

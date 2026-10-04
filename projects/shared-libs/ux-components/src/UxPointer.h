#pragma once
#include "UxInput.h"
#include <stdint.h>

namespace ux {
// One owner for a pointer sequence. Buttons capture their down target; only a
// scroll surface can promote the sequence into a drag. Touch-up-inside within one second activates the captured target.
class PointerSession {
public:
    void begin(int target, Rect bounds, int x, int y, uint32_t now, bool scrollable) {
        _active = true; _scroll = false; _cancelled = false;
        _target = target; _bounds = bounds; _x0 = _x = x; _y0 = _y = y;
        _started = now; _scrollable = scrollable;
    }
    void move(int x, int y) {
        if (!_active) return;
        _x = x; _y = y;
        if (_scroll || _cancelled) return;
        const int dx = absInt(x - _x0), dy = absInt(y - _y0);
        if (_scrollable) {
            if (dy >= 20 && dy >= dx) { _scroll = true; return; }
        } // Non-scroll buttons may leave and re-enter before release.
    }
    int end(int x, int y, uint32_t now) {
        move(x, y);
        int clicked = pressed() && now - _started <= 1000 ? _target : -1;
        _active = false;
        return clicked;
    }
    void cancel() { _active = false; _scroll = false; _cancelled = true; }
    bool active() const { return _active; }
    bool scrolling() const { return _active && _scroll; }
    bool pressed() const { return _active && !_scroll && !_cancelled && _target >= 0 && inside(_x, _y, 0); }
    int pressedTarget() const { return pressed() ? _target : -1; }
    Rect bounds() const { return _bounds; }
    int startX() const { return _x0; }
    int startY() const { return _y0; }
    int x() const { return _x; }
    int y() const { return _y; }
    uint32_t startedAt() const { return _started; }
private:
    static int absInt(int v) { return v < 0 ? -v : v; }
    bool inside(int x, int y, int slop) const {
        return x >= _bounds.x - slop && x < _bounds.x + _bounds.w + slop
            && y >= _bounds.y - slop && y < _bounds.y + _bounds.h + slop;
    }
    bool _active = false, _scroll = false, _cancelled = false, _scrollable = false;
    int _target = -1, _x0 = 0, _y0 = 0, _x = 0, _y = 0;
    uint32_t _started = 0;
    Rect _bounds{0,0,0,0};
};
}

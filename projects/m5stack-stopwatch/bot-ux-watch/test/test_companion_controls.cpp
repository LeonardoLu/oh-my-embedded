#include "WatchInteraction.h"
#include "InputSemantics.h"
#include <cassert>
#include <stdint.h>

int main() {
    using Click = watchinteraction::SingleDoubleClick;
    Click b;
    assert(b.poll(true, 100) == Click::Event::None);
    assert(b.poll(false, 420) == Click::Event::None);
    assert(b.poll(true, 420) == Click::Event::Double);
    assert(b.poll(false, 900) == Click::Event::None);
    assert(b.poll(true, 1000) == Click::Event::None);
    assert(b.poll(false, 1321) == Click::Event::Single);
    assert(b.poll(false, 1500) == Click::Event::None);
    b.poll(true, 1600); b.cancel();
    assert(b.poll(false, 2100) == Click::Event::None);
    assert(b.poll(true, UINT32_MAX - 80) == Click::Event::None);
    assert(b.poll(true, 40) == Click::Event::Double);

    using watchinput::Gesture;
    watchinput::TouchGesture tap;
    tap.poll(true, true, false, 100, 100, 0);
    assert(tap.poll(false, false, true, 108, 105, 100) == Gesture::Tap);
    assert(tap.tapX() == 108 && tap.tapY() == 105);
    tap.poll(true, true, false, 100, 100, 200);
    assert(tap.poll(false, false, true, 125, 100, 260) == Gesture::Tap);
    tap.poll(true, true, false, 100, 100, 300);
    assert(tap.poll(false, true, false, 100, 100, 2300) == Gesture::Long);
    assert(tap.poll(false, false, true, 100, 100, 2400) == Gesture::None);
}

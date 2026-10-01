#include "InputSemantics.h"

#include <assert.h>

using watchinput::ButtonGesture;
using watchinput::Gesture;
using watchinput::TouchGesture;

static void testButtonTapAndLongPress() {
    ButtonGesture button;
    assert(button.poll(true, true, false, 100) == Gesture::None);
    assert(button.poll(false, false, true, 200) == Gesture::Tap);

    assert(button.poll(true, true, false, 1000) == Gesture::None);
    assert(button.poll(false, true, false, 2999) == Gesture::None);
    assert(button.poll(false, true, false, 3000) == Gesture::Long);
    assert(button.poll(false, true, false, 3100) == Gesture::None);
    assert(button.poll(false, false, true, 3200) == Gesture::None);
}

static void testConsumedButtonWake() {
    ButtonGesture button;
    assert(button.poll(true, true, false, 100) == Gesture::None);
    button.consume(true);
    assert(button.poll(false, true, false, 900) == Gesture::None);
    assert(button.poll(false, false, true, 910) == Gesture::None);
    assert(button.poll(true, true, false, 1000) == Gesture::None);
    assert(button.poll(false, false, true, 1010) == Gesture::Tap);
}

static void testTouchTap() {
    TouchGesture touch;
    assert(touch.poll(true, true, false, 120, 180, 100) == Gesture::None);
    assert(touch.poll(false, true, false, 123, 181, 150) == Gesture::None);
    assert(touch.poll(false, false, true, 123, 181, 180) == Gesture::Tap);
    assert(touch.tapX() == 123 && touch.tapY() == 181);
}

static void testTouchLongDoesNotRepeatOrTap() {
    TouchGesture touch;
    assert(touch.poll(true, true, false, 200, 200, 100) == Gesture::None);
    assert(touch.poll(false, true, false, 200, 200, 2100) == Gesture::Long);
    assert(touch.poll(false, true, false, 200, 130, 2200) == Gesture::None);
    assert(touch.poll(false, false, true, 200, 130, 2220) == Gesture::None);
}

static void testSwipeDoesNotRepeatBecomeLongOrTap() {
    TouchGesture touch;
    assert(touch.poll(true, true, false, 200, 240, 100) == Gesture::None);
    assert(touch.poll(false, true, false, 202, 180, 180) == Gesture::SwipeUp);
    assert(touch.poll(false, true, false, 203, 130, 900) == Gesture::None);
    assert(touch.poll(false, false, true, 203, 130, 920) == Gesture::None);
}

static void testHorizontalSwipesAndOrigin() {
    TouchGesture touch;
    assert(touch.poll(true, true, false, 360, 18, 100) == Gesture::None);
    assert(touch.startX() == 360 && touch.startY() == 18);
    assert(touch.poll(false, true, false, 292, 22, 180) == Gesture::SwipeLeft);
    assert(touch.poll(false, false, true, 292, 22, 200) == Gesture::None);

    assert(touch.poll(true, true, false, 100, 220, 300) == Gesture::None);
    assert(touch.poll(false, true, false, 170, 214, 380) == Gesture::SwipeRight);
    assert(touch.poll(false, false, true, 170, 214, 400) == Gesture::None);
}

static void testConsumedTouchWake() {
    TouchGesture touch;
    assert(touch.poll(true, true, false, 50, 50, 100) == Gesture::None);
    touch.consume();
    assert(touch.poll(false, true, false, 50, 50, 900) == Gesture::None);
    assert(touch.poll(false, false, true, 50, 50, 910) == Gesture::None);
    assert(touch.poll(true, true, false, 60, 70, 1000) == Gesture::None);
    assert(touch.poll(false, false, true, 60, 70, 1010) == Gesture::Tap);
}

static void testDurationBoundaries() {
    TouchGesture t;
    t.poll(true,true,false,200,200,100);
    assert(t.poll(false,false,true,215,214,1100)==Gesture::Tap);
    t.poll(true,true,false,200,200,2000);
    assert(t.poll(false,false,true,200,200,3001)==Gesture::None);
    t.poll(true,true,false,200,200,4000);
    assert(t.poll(false,true,false,200,200,5999)==Gesture::None);
    assert(t.poll(false,true,false,200,200,6000)==Gesture::Long);
    assert(t.poll(false,false,true,200,200,6100)==Gesture::None);
}
static void testFaceLongUsesThreeSeconds() {
    TouchGesture t;
    t.poll(true,true,false,200,200,100,3000);
    assert(t.poll(false,true,false,200,200,3099,3000)==Gesture::None);
    assert(t.poll(false,true,false,200,200,3100,3000)==Gesture::Long);
    assert(t.poll(false,false,true,200,200,3200,3000)==Gesture::None);
}
int main() {
    testFaceLongUsesThreeSeconds();
    testDurationBoundaries();
    testButtonTapAndLongPress();
    testConsumedButtonWake();
    testTouchTap();
    testTouchLongDoesNotRepeatOrTap();
    testSwipeDoesNotRepeatBecomeLongOrTap();
    testHorizontalSwipesAndOrigin();
    testConsumedTouchWake();
    return 0;
}

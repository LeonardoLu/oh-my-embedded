#include "WatchInteraction.h"
#include "InputSemantics.h"

#include <assert.h>

using namespace watchinteraction;

static void testTouchGazeUsesBodyAndNormalizedOutsideDirection() {
    auto center=touchGaze(100,100,100,100,40);
    auto edge=touchGaze(140,100,100,100,40);
    auto right=touchGaze(141,100,100,100,40);
    auto diagonal=touchGaze(20,20,100,100,40);
    assert(center.x==0.0f&&center.y==0.0f);
    assert(edge.x==0.0f&&edge.y==0.0f);
    assert(right.x==1.0f&&right.y==0.0f);
    float length=sqrtf(diagonal.x*diagonal.x+diagonal.y*diagonal.y);
    assert(length>0.999f&&length<1.001f&&diagonal.x<0.0f&&diagonal.y<0.0f);
}

static void testHsvKeyColorsAndSectorBoundary() {
    auto red = hsvRgb(0, 100, 100);
    auto yellowEdge = hsvRgb(59, 100, 100);
    auto yellow = hsvRgb(60, 100, 100);
    auto green = hsvRgb(120, 100, 100);
    auto cyan = hsvRgb(180, 100, 100);
    auto blue = hsvRgb(240, 100, 100);
    assert(red.r == 255 && red.g == 0 && red.b == 0);
    assert(yellowEdge.r == 255 && yellowEdge.g >= 250 && yellowEdge.b == 0);
    assert(yellow.r == 255 && yellow.g == 255 && yellow.b == 0);
    assert(green.r == 0 && green.g == 255 && green.b == 0);
    assert(cyan.r == 0 && cyan.g == 255 && cyan.b == 255);
    assert(blue.r == 0 && blue.g == 0 && blue.b == 255);
    auto gray = hsvRgb(311, 0, 50);
    assert(gray.r == gray.g && gray.g == gray.b);
}

static void testSettingsChordNeedsBothButtonsForThreeSeconds() {
    SettingsChord chord;
    assert(!chord.poll(true, false, 0));
    assert(!chord.poll(true, true, 100));
    assert(!chord.poll(true, true, 3099));
    assert(chord.poll(true, true, 3100));
    assert(!chord.poll(true, true, 4000));
    assert(!chord.poll(false, true, 4100));
    assert(!chord.poll(true, true, 5000));
    assert(chord.poll(true, true, 8000));
}

static void testChordConsumptionSuppressesBothReleaseTaps() {
    SettingsChord chord;
    watchinput::ButtonGesture a, b;
    assert(!chord.poll(true, true, 100));
    a.consume(true);
    b.consume(true);
    assert(chord.poll(true, true, 3100));
    a.consume(true);
    b.consume(true);
    assert(a.poll(false, false, true, 3200) == watchinput::Gesture::None);
    assert(b.poll(false, false, true, 3200) == watchinput::Gesture::None);
}

static void testScrollListKeepsSelectionVisibleAndStopsAtEdges() {
    ScrollList list;
    list.configure(7, 4);
    list.select(4);
    assert(list.selected() == 4 && list.first() == 1);
    list.move(2);
    assert(list.selected() == 6 && list.first() == 3);
    list.move(1);
    assert(list.selected() == 6 && list.first() == 3);
    list.select(0);
    list.move(-1);
    assert(list.selected() == 0 && list.first() == 0);
}

static void testMotionFilterDeadzoneHysteresisAndCooldown() {
    MotionFilter filter;
    auto quiet = filter.update(0.05f, -0.08f, 5.0f, 100);
    assert(quiet.x == 0.0f && quiet.y == 0.0f && !quiet.poke);
    for (int i = 0; i < 20; ++i) quiet = filter.update(0.55f, 0.0f, 5.0f, 200 + i * 20);
    assert(quiet.x > 0.35f && quiet.x < 0.60f);
    bool poked = false;
    for (int i = 0; i < 20; ++i) poked |= filter.update(0, 0, 420.0f, 1000 + i * 20).poke;
    assert(poked);
    for (int i = 0; i < 40; ++i) assert(!filter.update(0, 0, 420.0f, 1600 + i * 20).poke);
    for (int i = 0; i < 30; ++i) filter.update(0, 0, 0.0f, 2500 + i * 20);
    for (int i = 0; i < 20; ++i) assert(!filter.update(0, 0, 420.0f, 3500 + i * 20).poke);
    for (int i = 0; i < 30; ++i) filter.update(0, 0, 0.0f, 7800 + i * 20);
    poked = false;
    for (int i = 0; i < 20; ++i) poked |= filter.update(0, 0, 420.0f, 8500 + i * 20).poke;
    assert(poked);
}

static uint32_t elapsed(uint32_t from, uint32_t to) {
    return to-from;
}

static void testAmbientCycleAlternatesExactDurationCategories() {
    AmbientCycle cycle(0x12345678u);
    cycle.begin(1000);
    assert(cycle.phase()==AmbientCycle::Phase::Safe&&cycle.index()==0);
    uint32_t due=cycle.nextMs();
    assert(elapsed(1000,due)>=5000&&elapsed(1000,due)<=15000);
    assert(!cycle.update(due-1));
    assert(cycle.update(due));
    assert(cycle.phase()==AmbientCycle::Phase::LookingAround);
    uint32_t lookingDue=cycle.nextMs();
    assert(elapsed(due,lookingDue)>=30000&&elapsed(due,lookingDue)<=60000);
    assert(cycle.update(lookingDue));
    assert(cycle.phase()==AmbientCycle::Phase::Safe&&cycle.index()<7);
    assert(elapsed(lookingDue,cycle.nextMs())>=5000&&elapsed(lookingDue,cycle.nextMs())<=15000);

    for(int i=0;i<20;++i) {
        due=cycle.nextMs();
        auto before=cycle.phase();
        assert(cycle.update(due));
        assert(cycle.phase()!=before);
        uint32_t duration=elapsed(due,cycle.nextMs());
        if(cycle.phase()==AmbientCycle::Phase::LookingAround)
            assert(duration>=30000&&duration<=60000);
        else {
            assert(duration>=5000&&duration<=15000);
            assert(cycle.index()<7);
        }
    }
}

static void testAmbientCycleHandlesRolloverAndOneLateTransition() {
    AmbientCycle cycle(0xABCDEF01u);
    uint32_t start=0xfffff000u;
    cycle.begin(start);
    uint32_t due=cycle.nextMs();
    assert(elapsed(start,due)>=5000&&elapsed(start,due)<=15000);
    assert(!cycle.update(due-1));
    assert(cycle.update(due));
    assert(cycle.phase()==AmbientCycle::Phase::LookingAround);

    // A long blocked frame advances one visible phase and schedules from now.
    uint32_t late=due+180000;
    assert(cycle.update(late));
    assert(cycle.phase()==AmbientCycle::Phase::Safe);
    assert(elapsed(late,cycle.nextMs())>=5000&&elapsed(late,cycle.nextMs())<=15000);
}

static void testAmbientPauseResumeRestartsVisibleIdle() {
    AmbientCycle cycle(7);
    cycle.begin(500);
    uint32_t firstDue=cycle.nextMs();
    cycle.setPaused(true,1000);
    assert(cycle.paused()&&!cycle.update(firstDue+90000));
    cycle.setPaused(false,firstDue+90000);
    assert(!cycle.paused()&&cycle.phase()==AmbientCycle::Phase::Safe&&cycle.index()==0);
    uint32_t resumed=cycle.nextMs();
    assert(elapsed(firstDue+90000,resumed)>=5000&&elapsed(firstDue+90000,resumed)<=15000);
    assert(!cycle.update(resumed-1)&&cycle.update(resumed));
}

int main() {
    testTouchGazeUsesBodyAndNormalizedOutsideDirection();
    testHsvKeyColorsAndSectorBoundary();
    testSettingsChordNeedsBothButtonsForThreeSeconds();
    testChordConsumptionSuppressesBothReleaseTaps();
    testScrollListKeepsSelectionVisibleAndStopsAtEdges();
    testMotionFilterDeadzoneHysteresisAndCooldown();
    testAmbientCycleAlternatesExactDurationCategories();
    testAmbientCycleHandlesRolloverAndOneLateTransition();
    testAmbientPauseResumeRestartsVisibleIdle();
    return 0;
}

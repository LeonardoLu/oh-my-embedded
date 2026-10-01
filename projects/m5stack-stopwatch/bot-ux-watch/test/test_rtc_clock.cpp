#include "RtcClock.h"
#include <cassert>
#include <cstdio>

FakeM5 M5;
uint32_t testMillis=0;

int main() {
    RtcClock clock;
    auto& hw=M5.Rtc.driver;
    hw.failRead=true;
    assert(!clock.refresh() && !clock.hasTime());
    assert(hw.writes==0 && hw.clears==0); // Boot failure must never seed build time.
    hw.failRead=false;
    assert(clock.refresh() && clock.value().time.hours==12);
    hw.failRead=true;
    assert(!clock.refresh() && clock.hasTime() && clock.value().time.minutes==34);
    assert(clock.setTime(9,10)==RtcClock::Save::Failed && hw.writes==0);
    hw.failRead=false;
    hw.failFlags=true;
    assert(!clock.refresh() && clock.hasTime());
    hw.failFlags=false;

    // A valid-looking date with VLF is not a trusted clock; both fields need consent.
    hw.flags=0x82;
    assert(!clock.refresh() && !clock.hasTime() && clock.state()==RtcClock::State::Unset);
    assert(clock.setTime(9,10)==RtcClock::Save::Pending && hw.writes==0);
    assert(clock.setDate(2026,2,30)==RtcClock::Save::Failed && hw.writes==0);
    assert(clock.setDate(2028,2,29)==RtcClock::Save::Saved);
    assert(clock.value().date.date==29 && clock.value().time.hours==9);
    assert(hw.flags==0x80 && hw.lastFlagWrite==0xbd); // Preserve unrelated flags.
    assert(hw.value.date.weekDay==2);

    // Manual partial edits preserve a freshly read opposite half, including midnight.
    hw.value=m5::rtc_datetime_t(m5::rtc_date_t(2029,1,1,1),m5::rtc_time_t(0,0,1));
    assert(clock.setTime(15,20)==RtcClock::Save::Saved);
    assert(hw.value.date.year==2029 && hw.value.date.date==1);
    hw.value.time=m5::rtc_time_t(16,22,33);
    assert(clock.setDate(2029,3,1)==RtcClock::Save::Saved);
    assert(hw.value.time.hours==16 && hw.value.time.seconds==33);

    // Lost time can also be set date-first, and cancelling does not confirm a half.
    hw.flags=2;
    assert(clock.setDate(2027,4,5)==RtcClock::Save::Pending);
    clock.cancelDraft();
    assert(clock.setTime(6,7)==RtcClock::Save::Pending);
    assert(clock.setDate(2027,4,6)==RtcClock::Save::Saved);
    assert(hw.value.time.hours==6 && hw.value.date.date==6);

    auto requested=m5::rtc_datetime_t(m5::rtc_date_t(2026,9,13,0),m5::rtc_time_t(10,11,12));
    hw.flags=2; hw.failWrite=true;
    assert(!clock.set(requested) && (hw.flags&2));
    hw.failWrite=false; hw.ignoreWrite=true;
    int clears=hw.clears;
    assert(!clock.set(requested) && hw.clears==clears);
    hw.ignoreWrite=false; hw.failClear=true;
    assert(!clock.set(requested) && !clock.hasTime());
    hw.failClear=false;
    assert(clock.set(requested));
    hw.stopOnSecondFlagRead=true; hw.flagReads=0;
    assert(!clock.refresh() && !clock.hasTime());
    hw.stopOnSecondFlagRead=false; hw.flags=0;
    hw.value.date.month=0;
    assert(!clock.refresh() && !clock.hasTime());
    assert(clock.state()==RtcClock::State::Unset);
    assert(clock.setDate(2026,9,13)==RtcClock::Save::Pending);
    assert(clock.setTime(10,11)==RtcClock::Save::Saved);
    assert(hw.writes>0);

    // Retry RTC discovery after a transient boot initialization failure, at most 1 Hz.
    M5=FakeM5{}; M5.Rtc.enabled=false; M5.Rtc.failBegin=true;
    RtcClock recovery;
    assert(!recovery.refresh() && M5.Rtc.begins==1);
    assert(!recovery.refresh() && M5.Rtc.begins==1);
    testMillis=1000; M5.Rtc.failBegin=false;
    assert(recovery.refresh() && M5.Rtc.begins==2 && M5.Rtc.driver.writes==0);
    puts("PASS rtc clock: read failures, VLF, verified writes, editors, calendar, discovery retry");
}

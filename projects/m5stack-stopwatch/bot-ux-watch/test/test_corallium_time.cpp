#include "CoralliumTime.h"
#include <cassert>
int main() {
    using namespace coralliumtime;
    assert(unixMs(2020,1,1,0,0,0,0)==MinUnixMs);
    assert(unixMs(2020,1,1,8,0,0,480)==MinUnixMs);
    assert(unixMs(2024,3,1,0,0,0,0)-unixMs(2024,2,28,0,0,0,0)==2*86400000LL);
    assert(unixMs(2100,1,1,0,0,0,0)==MaxUnixMs);
    assert(valid(MinUnixMs,0)); assert(valid(MaxUnixMs-1,0));
    assert(!valid(MinUnixMs-1,0)); assert(!valid(MaxUnixMs,0));
    assert(!valid(MinUnixMs,-720)); assert(!valid(MaxUnixMs-1,840));
    assert(!valid(MinUnixMs+86400000LL,-721)); assert(!valid(MinUnixMs,841));
    assert(valid(2177452800000LL,480)); // Beyond signed 32-bit Unix seconds.
    // Manual local RTC edits and RTC rollover must not publish an out-of-range UTC.
    assert(!valid(unixMs(2020,1,1,0,0,0,480),480));
    assert(!valid(unixMs(2099,12,31,23,59,59,-720),-720));
    assert(!valid(unixMs(2020,1,1,7,59,59,480),480));
    assert(valid(unixMs(2020,1,1,8,0,0,480),480));
    assert(valid(unixMs(2099,12,31,11,59,59,-720),-720));
    assert(!valid(unixMs(2099,12,31,12,0,0,-720),-720));
}

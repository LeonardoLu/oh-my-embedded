#include "TouchTraceBuffer.h"

#include <assert.h>
#include <stdio.h>

struct Event {
    int value;
};

int main() {
    watchtrace::Ring<Event, 4> detail;
    watchtrace::Ring<Event, 128> critical;

    for (int i = 0; i < 20; ++i) {
        if (watchtrace::keepAcquisition(i != 0, false, false, false, true)) {
            detail.push({i});
        }
    }
    assert(detail.count() == 1);
    assert(detail.dropped() == 0);

    // Twenty trials with two contacts and one screen transition apiece remain
    // available even while idle acquisitions continue indefinitely.
    for (int i = 0; i < 100; ++i) critical.push({i});
    for (int i = 0; i < 1000; ++i) {
        if (watchtrace::keepAcquisition(true, false, false, false, true)) {
            detail.push({i});
        }
    }
    assert(critical.count() == 100);
    assert(critical.dropped() == 0);
    for (int i = 0; i < 100; ++i) assert(critical.at(i).value == i);

    detail.clear();
    assert(watchtrace::keepAcquisition(false, true, false, false, false));
    assert(watchtrace::keepAcquisition(true, true, false, false, false));
    assert(watchtrace::keepAcquisition(true, false, true, false, false));
    assert(watchtrace::keepAcquisition(true, true, true, true, false));
    assert(!watchtrace::keepAcquisition(true, true, true, false, false));

    puts("touch_trace_buffer: PASS");
    return 0;
}

#include "CoralliumEventGate.h"
#include <cassert>
#include <stdint.h>

int main() {
    corallium::EventGate events;
    // Bonded encryption/CCCD may restore long after boot, before the app listens.
    assert(!events.due(600000));
    events.handshake(600050); // Complete valid BLE device.info received.
    assert(!events.due(600050));
    assert(!events.due(605049));
    assert(events.due(605050));
    assert(!events.due(605050));
    assert(events.due(610050));

    // Disconnect/close discards the previous session's readiness, including
    // when the next client restores exactly the same bonded CCCD.
    events.reset();
    assert(!events.due(1000000));
    events.handshake(1000000);
    assert(!events.due(1004999));
    events.reset();
    assert(!events.due(1005000));

    // Event cadence remains valid across the Arduino millis wrap.
    events.handshake(UINT32_MAX-999);
    assert(!events.due(3999));
    assert(events.due(4000));
}

#pragma once
#include <stdint.h>

namespace corallium {
// Restored BLE subscriptions are not proof that this app session is receiving.
class EventGate {
public:
    void reset() { _ready=false; }
    void handshake(uint32_t now) { _last=now; _ready=true; }
    bool due(uint32_t now) {
        if(!_ready||uint32_t(now-_last)<5000) return false;
        _last=now;
        return true;
    }
private:
    volatile bool _ready=false;
    uint32_t _last=0;
};
}

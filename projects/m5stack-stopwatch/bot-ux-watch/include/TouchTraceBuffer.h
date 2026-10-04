#pragma once

#include <stddef.h>
#include <stdint.h>

namespace watchtrace {

template <typename Event, size_t Capacity>
class Ring {
public:
    static_assert(Capacity > 0, "trace ring capacity must be positive");

    void clear() { _head = _count = 0; _dropped = 0; }

    void push(const Event& event) {
        size_t index = (_head + _count) % Capacity;
        if (_count == Capacity) {
            index = _head;
            _head = (_head + 1) % Capacity;
            ++_dropped;
        } else {
            ++_count;
        }
        _events[index] = event;
    }

    const Event& at(size_t index) const { return _events[(_head + index) % Capacity]; }
    size_t count() const { return _count; }
    uint32_t dropped() const { return _dropped; }

private:
    Event _events[Capacity]{};
    size_t _head = 0;
    size_t _count = 0;
    uint32_t _dropped = 0;
};

inline bool keepAcquisition(bool known, bool contact, bool priorContact,
                            bool heldCheckpoint, bool delayed) {
    return !known || contact != priorContact || (contact && (heldCheckpoint || delayed));
}

}  // namespace watchtrace

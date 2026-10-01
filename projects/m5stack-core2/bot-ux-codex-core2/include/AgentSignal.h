#pragma once

#include <stdint.h>

#include "LightingState.h"

// Projection of this Codex build's documented lighting palette. These are
// host display signals, not task identities or confirmed completion events.
namespace agentsignal {

enum class Signal : uint8_t {
    Unknown, Off, Idle, Working, NeedsInput, NewReply, Error
};

inline Signal decode(const LightingZone& zone, bool ready)
{
    if (!ready) return Signal::Unknown;
    if (zone.effect == 0) return Signal::Off;
    // The verified per-slot writer emits only solid (1) or breath (4).
    if (zone.effect != 1 && zone.effect != 4) return Signal::Unknown;
    if (zone.color == 0) return Signal::Off;
    // Brightness is a user lighting preference, not an agent lifecycle state.
    switch (zone.color)
    {
        case 0xFFFFFF: return Signal::Idle;
        case 0x304FFE: return Signal::Working;
        case 0xFF6D00: return Signal::NeedsInput;
        case 0x00FF4C: return Signal::NewReply;
        case 0xFF0033: return Signal::Error;
        default: return Signal::Unknown;
    }
}

inline const char* name(Signal signal)
{
    switch (signal)
    {
        case Signal::Off: return "Lights off";
        case Signal::Idle: return "Idle";
        case Signal::Working: return "Working";
        case Signal::NeedsInput: return "Needs input";
        case Signal::NewReply: return "New reply";
        case Signal::Error: return "Error";
        default: return "Unknown";
    }
}

inline bool shouldNotify(Signal previous, Signal next)
{
    if (previous == next || previous == Signal::Unknown || previous == Signal::Off)
        return false;
    return next == Signal::NeedsInput || next == Signal::Error ||
           (previous == Signal::Working && next == Signal::NewReply);
}

struct Snapshot {
    LightingZone zone{};
    Signal signal = Signal::Unknown;
    uint32_t revision = 0;
    uint32_t changedAtMs = 0;
};

class Model {
public:
    static constexpr uint8_t kSlotCount = LightingState::kSlotCount;

    explicit Model(uint32_t cooldownMs = 2500) : _cooldownMs(cooldownMs) {}

    // Call with a coherent LightingState snapshot from the main loop. ready
    // means the complete app handshake is ready, not just a BLE connection.
    // Return one bit per slot whose new signal merits a short notification.
    uint8_t update(const LightingState& lighting, bool ready, uint32_t nowMs)
    {
        bool allOff = ready;
        for (uint8_t i = 0; i < kSlotCount; ++i)
            if (decode(lighting.slots[i], ready) != Signal::Off) allOff = false;
        const bool baseline = !_baselineReady;
        _baselineReady = ready && !allOff;

        uint8_t notifications = 0;
        bool changed = false;
        for (uint8_t i = 0; i < kSlotCount; ++i)
        {
            Snapshot& current = _slots[i];
            const Signal next = decode(lighting.slots[i], ready);
            const LightingZone zone = ready ? lighting.slots[i] : LightingZone{};
            if (current.signal == next && sameZone(current.zone, zone)) continue;
            if (ready && !baseline && !allOff && shouldNotify(current.signal, next) &&
                (!_hasNotified[i] || static_cast<uint32_t>(nowMs - _notifiedAt[i]) >= _cooldownMs))
            {
                notifications |= static_cast<uint8_t>(1u << i);
                _hasNotified[i] = true;
                _notifiedAt[i] = nowMs;
            }
            current.zone = zone;
            current.signal = next;
            current.changedAtMs = nowMs;
            ++current.revision;
            changed = true;
        }
        if (changed) ++_revision;
        return notifications;
    }

    // The caller supplies an existing slot index in [0, kSlotCount).
    const Snapshot& slot(uint8_t index) const { return _slots[index]; }
    uint32_t revision() const { return _revision; }

private:
    static bool sameZone(const LightingZone& a, const LightingZone& b)
    {
        return a.color == b.color && a.brightness == b.brightness &&
               a.effect == b.effect && a.speed == b.speed;
    }

    Snapshot _slots[kSlotCount]{};
    uint32_t _notifiedAt[kSlotCount]{};
    bool _hasNotified[kSlotCount]{};
    uint32_t _revision = 0;
    uint32_t _cooldownMs;
    bool _baselineReady = false;
};

} // namespace agentsignal

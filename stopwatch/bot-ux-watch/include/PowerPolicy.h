#pragma once

#include <stdint.h>

namespace watchpower {

// M5PM1 L1 supplies RX8130 and IMU. Retain it through double-click off,
// preserving LED, charge, other rails and unrelated power-hold bits.
template<class Pmic>
bool retainRtcPower(Pmic& pm) {
    uint8_t cfg=0,hold=0,verify=0;
    return pm.readRegister(0x06,&cfg,1)
        && ((cfg&0x04) || pm.writeRegister8(0x06,cfg|0x04))
        && pm.readRegister(0x07,&hold,1)
        && ((hold&0x20) || pm.writeRegister8(0x07,hold|0x20))
        && pm.readRegister(0x06,&verify,1) && (verify&0x04)
        && pm.readRegister(0x07,&verify,1) && (verify&0x20);
}

constexpr uint8_t kTimeoutCount = 6;

inline uint32_t timeoutMs(uint8_t index) {
    switch (index) {
        case 0: return 5U * 1000U;
        case 1: return 15U * 1000U;
        case 2: return 60U * 1000U;
        case 3: return 5U * 60U * 1000U;
        case 4: return 10U * 60U * 1000U;
        default: return 15U * 60U * 1000U;
    }
}

inline const char* timeoutLabel(uint8_t index) {
    switch (index) {
        case 0: return "5 S";
        case 1: return "15 S";
        case 2: return "1 MIN";
        case 3: return "5 MIN";
        case 4: return "10 MIN";
        default: return "15 MIN";
    }
}

enum class ScreenState : uint8_t { Active, Dimmed, Off };
enum class OffWaitMode : uint8_t { AwakePoll, ButtonLightSleep };

inline bool audioShouldRun(ScreenState state, bool soundEnabled) {
    return state == ScreenState::Active && soundEnabled;
}

inline bool audioSafeForLightSleep(ScreenState state, bool suspended) {
    return state == ScreenState::Off && suspended;
}

inline OffWaitMode offWaitMode(ScreenState state, bool buttonWakeOnly,
                               bool externalPower, bool inputActive) {
    return state == ScreenState::Off && buttonWakeOnly
        && !externalPower && !inputActive
        ? OffWaitMode::ButtonLightSleep : OffWaitMode::AwakePoll;
}

inline bool shouldWakeAndConsume(ScreenState state, bool wakePressed) {
    return state != ScreenState::Active && wakePressed;
}

struct WakeGateResult {
    bool blockControls;
    bool consumedPowerClick;
};

// Holds controls behind a complete release after waking. PMIC power presses
// also need to absorb the later SDK click that represents the same press.
class WakeInputGate {
public:
    void beginWake(bool powerButtonWake) {
        _releaseGate = true;
        if (powerButtonWake) {
            _consumePowerClick = true;
            _powerReleaseGrace = false;
        }
    }

    WakeGateResult update(uint32_t now, bool anyInputActive,
                          bool powerButtonPressed, bool powerButtonClicked) {
        bool clickConsumed = false;
        if (_consumePowerClick) {
            if (powerButtonPressed) {
                _powerReleaseGrace = false;
            } else if (!_powerReleaseGrace) {
                _powerReleaseGrace = true;
                _powerReleaseDeadline = now + 1500;
            }
            if (powerButtonClicked) {
                clickConsumed = true;
                _consumePowerClick = false;
                _powerReleaseGrace = false;
            } else if (_powerReleaseGrace
                    && (int32_t)(now - _powerReleaseDeadline) >= 0) {
                _consumePowerClick = false;
                _powerReleaseGrace = false;
            }
        }

        bool block = _releaseGate;
        if (_releaseGate && !anyInputActive) _releaseGate = false;
        return {block,clickConsumed};
    }

    bool releasePending() const { return _releaseGate; }
    bool powerClickPending() const { return _consumePowerClick; }

private:
    uint32_t _powerReleaseDeadline = 0;
    bool _releaseGate = false;
    bool _consumePowerClick = false;
    bool _powerReleaseGrace = false;
};

// Host-independent inactivity policy. External power and held input both keep
// the watch active. Refreshing lastActivity while externally powered means an
// unplug always begins a new battery timeout interval.
class IdleScreenPolicy {
public:
    void begin(uint32_t now) {
        _lastActivity = now;
        _state = ScreenState::Active;
        _begun = true;
    }

    ScreenState update(uint32_t now, bool externalPower, bool inputActive,
                       uint8_t dimTimeout, uint8_t offTimeout) {
        if (!_begun) begin(now);
        if (externalPower || inputActive) {
            wake(now);
            return _state;
        }

        const uint32_t elapsed = now - _lastActivity;
        if (elapsed >= timeoutMs(offTimeout)) _state = ScreenState::Off;
        else if (elapsed >= timeoutMs(dimTimeout)) _state = ScreenState::Dimmed;
        else _state = ScreenState::Active;
        return _state;
    }

    void wake(uint32_t now) {
        _lastActivity = now;
        _state = ScreenState::Active;
        _begun = true;
    }

    ScreenState state() const { return _state; }
    uint32_t lastActivity() const { return _lastActivity; }

private:
    uint32_t _lastActivity = 0;
    ScreenState _state = ScreenState::Active;
    bool _begun = false;
};

} // namespace watchpower

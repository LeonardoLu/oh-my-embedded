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
constexpr uint32_t kForcedWakeWindowMs = 60U * 1000U;

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

inline uint8_t dimBrightnessLevel(uint8_t activeLevel, uint8_t requestedLevel) {
    if (activeLevel < 1) activeLevel = 1;
    if (activeLevel > 5) activeLevel = 5;
    if (requestedLevel < 1) requestedLevel = 1;
    if (requestedLevel > 5) requestedLevel = 5;
    return requestedLevel < activeLevel ? requestedLevel : activeLevel;
}

enum class ScreenState : uint8_t { Active, Dimmed, Off };
enum class OffWaitMode : uint8_t { AwakePoll, ButtonLightSleep };

struct IdleConfig {
    bool enabled = true;
    uint8_t dimTimeout = 1;
    uint8_t offTimeout = 2;
    bool keepAwakeWhileExternalPower = true;
    bool forcedSleepEnabled = false;
    uint8_t forcedSleepStartHour = 23;
    uint8_t forcedSleepEndHour = 7;
};

inline bool savingApplies(const IdleConfig& config, bool externalPower) {
    return config.enabled
        && (!externalPower || !config.keepAwakeWhileExternalPower);
}

// Equal endpoints intentionally describe an empty interval. This keeps an
// accidentally incomplete schedule from making the watch inaccessible.
inline bool hourInRange(uint8_t hour, uint8_t start, uint8_t end) {
    if (hour > 23 || start > 23 || end > 23 || start == end) return false;
    return start < end ? hour >= start && hour < end
                       : hour >= start || hour < end;
}

inline bool forcedSleepApplies(const IdleConfig& config, bool clockReady,
                               uint8_t hour) {
    return config.enabled && config.forcedSleepEnabled
        && clockReady && hourInRange(hour, config.forcedSleepStartHour,
                                     config.forcedSleepEndHour);
}

inline bool policyApplies(const IdleConfig& config, bool externalPower,
                          bool clockReady, uint8_t hour) {
    return savingApplies(config, externalPower)
        || forcedSleepApplies(config, clockReady, hour);
}

inline bool audioShouldRun(ScreenState state, bool soundEnabled) {
    return state == ScreenState::Active && soundEnabled;
}

inline bool audioSafeForLightSleep(ScreenState state, bool suspended) {
    return state == ScreenState::Off && suspended;
}

inline OffWaitMode offWaitMode(ScreenState state, bool buttonWakeOnly,
                               bool savingBypassed, bool inputActive) {
    return state == ScreenState::Off && buttonWakeOnly
        && !savingBypassed && !inputActive
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

// Host-independent inactivity policy. A bypassed power source and held input
// keep the watch active. Refreshing lastActivity while policy is bypassed means
// enabling it or unplugging begins a fresh timeout interval.
class IdleScreenPolicy {
public:
    void begin(uint32_t now) {
        _lastActivity = now;
        _state = ScreenState::Active;
        _begun = true;
        _forcedWakeActive = false;
    }

    ScreenState update(uint32_t now, bool externalPower, bool inputActive,
                       bool clockReady, uint8_t hour,
                       const IdleConfig& config) {
        if (!_begun) begin(now);
        if (!config.enabled) {
            _lastActivity = now;
            _state = ScreenState::Active;
            _forcedWakeActive = false;
            return _state;
        }

        const bool forced = forcedSleepApplies(config, clockReady, hour);
        if (!forced && !savingApplies(config, externalPower)) {
            _lastActivity = now;
            _state = ScreenState::Active;
            _forcedWakeActive = false;
            return _state;
        }
        if (inputActive) {
            _lastActivity = now;
            _state = ScreenState::Active;
            if (forced) grantForcedWake(now);
            return _state;
        }
        if (forced) {
            if (!_forcedWakeActive
                    || (int32_t)(now - _forcedWakeUntil) >= 0) {
                _forcedWakeActive = false;
                _state = ScreenState::Off;
                return _state;
            }
            _state = now - _lastActivity >= timeoutMs(config.dimTimeout)
                ? ScreenState::Dimmed : ScreenState::Active;
            return _state;
        }
        _forcedWakeActive = false;

        const uint32_t elapsed = now - _lastActivity;
        if (elapsed >= timeoutMs(config.offTimeout)) _state = ScreenState::Off;
        else if (elapsed >= timeoutMs(config.dimTimeout)) _state = ScreenState::Dimmed;
        else _state = ScreenState::Active;
        return _state;
    }

    ScreenState update(uint32_t now, bool externalPower, bool inputActive,
                       uint8_t dimTimeout, uint8_t offTimeout) {
        IdleConfig config;
        config.dimTimeout = dimTimeout;
        config.offTimeout = offTimeout;
        return update(now, externalPower, inputActive, false, 0, config);
    }

    void wake(uint32_t now) {
        _lastActivity = now;
        _state = ScreenState::Active;
        _begun = true;
    }

    // A deliberate wake inside a forced interval must leave enough time to
    // reach settings. Continued input renews the same bounded window.
    void wakeForUser(uint32_t now) {
        wake(now);
        grantForcedWake(now);
    }

    ScreenState state() const { return _state; }
    uint32_t lastActivity() const { return _lastActivity; }
    bool forcedWakeActive(uint32_t now) const {
        return _forcedWakeActive && (int32_t)(now - _forcedWakeUntil) < 0;
    }

private:
    void grantForcedWake(uint32_t now) {
        _forcedWakeUntil = now + kForcedWakeWindowMs;
        _forcedWakeActive = true;
    }

    uint32_t _lastActivity = 0;
    uint32_t _forcedWakeUntil = 0;
    ScreenState _state = ScreenState::Active;
    bool _begun = false;
    bool _forcedWakeActive = false;
};

} // namespace watchpower

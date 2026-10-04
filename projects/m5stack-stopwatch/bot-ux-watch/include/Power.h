// StopWatch power facade. M5Unified owns the detected M5PM1 instance.
#pragma once

#include <stdint.h>

class Power {
public:
    void begin();

    void update();
    bool setIndicator(bool enabled);
    bool indicatorReady() const { return _indicatorReady; }
    bool homeKeyReady() const { return _homeKeyReady; }
    bool rtcHoldReady() const { return _rtcHoldReady; }
    uint8_t bootKeyConfig() const { return _bootKeyCfg; }
    uint8_t bootOffConfig() const { return _bootOffCfg; }
    uint16_t batteryMillivolts() const { return _filteredMv; }
    uint8_t batteryPct() const { return _batteryPct; }
    bool charging() const { return _charging; }
    bool externalPower() const { return _externalPower; }
    bool readPowerButton(bool* pressed);

    void applyLevel(uint8_t level); // 1..5 -> 0..255 -> display
    void setBrightness(uint8_t v);  // raw 0..255
    void sleepDisplay();
    void wakeDisplay(uint8_t level);
    bool lightSleepForButtonPoll(uint64_t fallbackUs = 1000000);
    bool displaySleeping() const { return _displaySleeping; }

    static uint8_t levelToValue(uint8_t level);

private:
    bool _rtcHoldReady = false;
    uint16_t _filteredMv = 0;
    uint8_t _batteryPct = 100;
    bool _charging = false;
    bool _externalPower = false;
    bool _displaySleeping = false;
    uint8_t _bootKeyCfg=0, _bootOffCfg=0;
    bool _indicatorReady=false, _indicator=false, _homeKeyReady=false;
};

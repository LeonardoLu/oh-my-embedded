// Round-safe StopWatch face: RTC date/time, honest power status and bot hero.
#pragma once

#include <M5Unified.h>
#include <BotUx.h>
#include <stdint.h>

class WatchFace {
public:
    // One persistent 466x90 RGB565 HUD scratch band; false means allocation failed.
    bool begin(M5Canvas* botSprite);
    void setHour24(bool v) { if (_hour24 != v) _drawInvalid = true; _hour24 = v; }
    void setShowSeconds(bool v) { if (_showSeconds != v) _drawInvalid = true; _showSeconds = v; }
    void setLayout(bool swap, bool showDescription, uint8_t language);
    void setBattery(uint8_t pct);
    void setCharging(bool v) { _charging = v; }
    void update(uint32_t nowMs);
    void invalidate() { _drawInvalid = true; _lastRtcReadMs = 0; }
    void draw(lgfx::LovyanGFX* target, uint16_t bg, uint16_t ink, uint16_t muted,
              uint16_t accent, uint16_t panel, uint16_t warning, float statusProgress);
    botux::BotUx& bot() { return _bot; }
    uint8_t hour() const { return _hh; }
    uint8_t minute() const { return _mm; }
    uint8_t second() const { return _ss; }
    int16_t year() const { return _year; }
    uint8_t month() const { return _month; }
    uint8_t day() const { return _day; }

private:
    void _drawClock(bool top, uint16_t ink, uint16_t muted);
    void _drawDescription(bool top, uint16_t ink);
    void _drawBatteryPanel(uint16_t bg, float progress);
    botux::BotUx _bot;
    M5Canvas _hud;
    bool _ready = false;
    lgfx::LovyanGFX* _lastTarget = nullptr;
    uint32_t _now = 0, _lastRtcReadMs = 0;
    uint8_t _hh = 0, _mm = 0, _ss = 0;
    int16_t _year = 2000;
    uint8_t _month = 1, _day = 1, _weekDay = 0;
    bool _hour24 = true, _showSeconds = false, _clockValid = false;
    bool _swap = false, _showDescription = true;
    uint8_t _language = 0, _battery = 100;
    bool _charging = false;
    uint8_t _drawHour = 0xFF, _drawMinute = 0xFF, _drawSecond = 0xFF;
    uint8_t _drawDay = 0xFF, _drawMonth = 0xFF;
    int16_t _drawYear = -1;
    bool _drawInvalid = true, _panelWasVisible = false;
    uint16_t _colors[6] = {};
    char _description[96] = {};
};

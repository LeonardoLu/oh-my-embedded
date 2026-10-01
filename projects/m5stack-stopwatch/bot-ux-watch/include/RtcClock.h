#pragma once

#include <M5Unified.h>

// RTC is the only wall-clock source. A failed read never writes or replaces it.
class RtcClock {
public:
    enum class State { Unset, Ready, ReadError };
    enum class Save { Failed, Pending, Saved };
    bool refresh();
    bool set(const m5::rtc_datetime_t& value);
    Save setTime(uint8_t hour, uint8_t minute);
    Save setDate(int16_t year, uint8_t month, uint8_t day);
    m5::rtc_datetime_t editorValue();
    void cancelDraft() { _draftDate = _draftTime = false; }
    bool hasTime() const { return _hasTime; }
    State state() const { return _state; }
    const m5::rtc_datetime_t& value() const { return _value; }
    static bool valid(const m5::rtc_datetime_t& value);

private:
    Save savePart(bool date);
    m5::rtc_datetime_t _value;
    m5::rtc_datetime_t _draft{m5::rtc_date_t(2026,1,1,4),m5::rtc_time_t(0,0,0)};
    State _state = State::Unset;
    uint32_t _lastInitMs = 0;
    bool _initAttempted = false;
    bool _hasTime = false, _draftDate = false, _draftTime = false;
};

extern RtcClock watchClock;

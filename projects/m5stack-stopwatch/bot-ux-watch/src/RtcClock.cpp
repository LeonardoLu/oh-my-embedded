#include "RtcClock.h"
#include "CalendarMath.h"

RtcClock watchClock;

namespace {
constexpr uint8_t kFlags = 0x1d, kVlf = 0x02;

int decodeBcd(uint8_t value) {
    return (value&15)>9 || (value>>4)>9 ? -1 : (value>>4)*10+(value&15);
}

uint32_t secondsSince2020(const m5::rtc_datetime_t& v) {
    uint32_t days = 0;
    for(int year=2020;year<v.date.year;++year)
        days += watchcalendar::leapYear(year) ? 366 : 365;
    for(int month=1;month<v.date.month;++month)
        days += watchcalendar::daysInMonth(v.date.year,month);
    days += v.date.date-1;
    return ((days*24+v.time.hours)*60+v.time.minutes)*60+v.time.seconds;
}
}

bool RtcClock::valid(const m5::rtc_datetime_t& v) {
    return v.date.year>=2020 && v.date.year<=2099
        && v.date.month>=1 && v.date.month<=12 && v.date.date>=1
        && v.date.date<=watchcalendar::daysInMonth(v.date.year,v.date.month)
        && v.time.hours>=0 && v.time.hours<24
        && v.time.minutes>=0 && v.time.minutes<60
        && v.time.seconds>=0 && v.time.seconds<60;
}

bool RtcClock::refresh() {
    auto* rtc=M5.Rtc.getRtcInstancePtr();
    if(!rtc && (!_initAttempted || millis()-_lastInitMs>=1000)) {
        _initAttempted=true; _lastInitMs=millis();
        M5.Rtc.begin(&M5.In_I2C,M5.getBoard());
        rtc=M5.Rtc.getRtcInstancePtr();
    }
    uint8_t flags=0;
    m5::rtc_datetime_t next;
    if(!rtc || !rtc->readRegister(kFlags,&flags,1)) {
        _state=State::ReadError;
        return false;
    }
    if(flags&kVlf) {
        _hasTime=false; _state=State::Unset;
        return false;
    }
    // Read the seven calendar registers together. The SDK's boolean accessor
    // conflates invalid register contents with bus failures; distinguish them
    // here so invalid clocks remain repairable in the date/time editors.
    uint8_t raw[7]={};
    if(!rtc->readRegister(0x10,raw,7) || !rtc->readRegister(kFlags,&flags,1)) {
        _state=State::ReadError;
        return false;
    }
    next.time=m5::rtc_time_t(decodeBcd(raw[2]&0x3f),decodeBcd(raw[1]&0x7f),decodeBcd(raw[0]&0x7f));
    next.date=m5::rtc_date_t(2000+decodeBcd(raw[6]),decodeBcd(raw[5]&0x1f),decodeBcd(raw[4]&0x3f),0);
    if((flags&kVlf) || raw[3]==0 || (raw[3]&0x80) || (raw[3]&(raw[3]-1)) || !valid(next)) {
        _hasTime=false; _state=State::Unset;
        return false;
    }
    next.date.weekDay=watchcalendar::weekDay(next.date.year,next.date.month,next.date.date);
    _value=next; _hasTime=true; _state=State::Ready;
    return true;
}

bool RtcClock::set(const m5::rtc_datetime_t& value) {
    auto* rtc=M5.Rtc.getRtcInstancePtr();
    if(!rtc || !valid(value)) return false;
    auto next=value;
    next.date.weekDay=watchcalendar::weekDay(next.date.year,next.date.month,next.date.date);
    if(!rtc->setDateTime(&next.date,&next.time)) return false;
    _hasTime=false;
    _state=State::ReadError;
    m5::rtc_datetime_t verify;
    if(!M5.Rtc.getDateTime(&verify) || !valid(verify)) return false;
    uint32_t expected=secondsSince2020(next), actual=secondsSince2020(verify);
    if(actual<expected || actual-expected>1) return false;
    // RX8130 flags are write-zero-to-clear. Preserve all flags except VLF;
    // only an explicit, verified date/time setting can acknowledge lost time.
    if(!rtc->writeRegister8(kFlags,0xbd) || !refresh()) return false;
    cancelDraft();
    return true;
}

m5::rtc_datetime_t RtcClock::editorValue() {
    refresh();
    return _hasTime ? _value : _draft;
}

RtcClock::Save RtcClock::savePart(bool date) {
    if(_state==State::ReadError) return Save::Failed;
    if(!_hasTime && !(_draftDate && _draftTime)) return Save::Pending;
    auto next=_hasTime ? _value : _draft;
    if(date) next.date=_draft.date;
    else next.time=_draft.time;
    return set(next) ? Save::Saved : Save::Failed;
}

RtcClock::Save RtcClock::setTime(uint8_t hour,uint8_t minute) {
    refresh();
    if(hour>23 || minute>59) return Save::Failed;
    _draft.time=m5::rtc_time_t(hour,minute,0); _draftTime=true;
    return savePart(false);
}

RtcClock::Save RtcClock::setDate(int16_t year,uint8_t month,uint8_t day) {
    refresh();
    auto next=_draft;
    next.date=m5::rtc_date_t(year,month,day,0);
    if(!valid(next)) return Save::Failed;
    _draft.date=next.date; _draftDate=true;
    return savePart(true);
}

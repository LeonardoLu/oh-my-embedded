#pragma once
// RTC value structures: Copyright (c) M5Stack, MIT license (M5Unified RTC_Base.hpp).
#include <cstdint>
#include <ctime>
// RTC value types mirror the pinned M5Unified contract, including failure defaults.
namespace m5 {
  struct __attribute__((packed)) rtc_time_t
  {
    std::int8_t hours;
    std::int8_t minutes;
    std::int8_t seconds;

    rtc_time_t(std::int8_t hours_ = -1, std::int8_t minutes_ = -1, std::int8_t seconds_ = -1)
    : hours   { hours_   }
    , minutes { minutes_ }
    , seconds { seconds_ }
    {}

    rtc_time_t(const tm& t)
    : hours   { (int8_t)t.tm_hour }
    , minutes { (int8_t)t.tm_min  }
    , seconds { (int8_t)t.tm_sec  }
    {}
  };

  struct __attribute__((packed)) rtc_date_t
  {
    /// year 1900-2099
    std::int16_t year;

    /// month 1-12
    std::int8_t month;

    /// date 1-31
    std::int8_t date;

    /// weekDay 0:sun / 1:mon / 2:tue / 3:wed / 4:thu / 5:fri / 6:sat
    std::int8_t weekDay;

    rtc_date_t(std::int16_t year_ = 2000, std::int8_t month_ = 1, std::int8_t date_ = -1, std::int8_t weekDay_ = -1)
    : year    { year_    }
    , month   { month_   }
    , date    { date_    }
    , weekDay { weekDay_ }
    {}

    rtc_date_t(const tm& t)
    : year    { (int16_t)(t.tm_year + 1900) }
    , month   { (int8_t )(t.tm_mon  + 1   ) }
    , date    { (int8_t ) t.tm_mday         }
    , weekDay { (int8_t ) t.tm_wday         }
    {}
  };

  struct __attribute__((packed)) rtc_datetime_t
  {
    rtc_date_t date;
    rtc_time_t time;
    rtc_datetime_t() = default;
    rtc_datetime_t(const rtc_date_t& d, const rtc_time_t& t) : date { d }, time { t } {};
    rtc_datetime_t(const tm& t) : date { t }, time { t } {}
    tm get_tm(void) const;
    void set_tm(tm& time);
    void set_tm(tm* t) { if (t) set_tm(*t); }
  };

}
struct FakeRtcDriver {
    m5::rtc_datetime_t value{m5::rtc_date_t(2026,9,13,0),m5::rtc_time_t(12,34,56)};
    uint8_t flags=0, lastFlagWrite=0;
    bool failFlags=false,failRead=false,failWrite=false,failClear=false,ignoreWrite=false;
    bool stopOnSecondFlagRead=false;
    int writes=0,clears=0,flagReads=0;
    bool readRegister(uint8_t reg,uint8_t* out,int) {
        if(reg==0x10) {
            if(failRead) return false;
            auto bcd=[](int v) { return (v/10)*16+v%10; };
            out[0]=bcd(value.time.seconds); out[1]=bcd(value.time.minutes); out[2]=bcd(value.time.hours);
            out[3]=1<<value.date.weekDay; out[4]=bcd(value.date.date);
            out[5]=bcd(value.date.month); out[6]=bcd(value.date.year%100);
            return true;
        }
        if(failFlags) return false;
        if(stopOnSecondFlagRead && ++flagReads==2) flags|=2;
        *out=flags; return true;
    }
    bool writeRegister8(uint8_t,uint8_t mask) {
        ++clears; lastFlagWrite=mask;
        if(failClear) return false;
        flags&=mask; return true;
    }
    bool setDateTime(const m5::rtc_date_t* date,const m5::rtc_time_t* time) {
        ++writes; if(failWrite) return false;
        if(!ignoreWrite) { value.date=*date; value.time=*time; }
        return true;
    }
};
struct FakeRtc {
    FakeRtcDriver driver;
    bool enabled=true,failBegin=false;
    int begins=0;
    FakeRtcDriver* getRtcInstancePtr() { return enabled?&driver:nullptr; }
    bool getDateTime(m5::rtc_datetime_t* out) {
        if(!enabled || driver.failRead) return false;
        *out=driver.value; return true;
    }
    bool begin(void*,int) { ++begins; enabled=!failBegin; return enabled; }
};
struct FakeM5 { FakeRtc Rtc; int In_I2C=0; int getBoard() { return 0; } };
extern FakeM5 M5;
extern uint32_t testMillis;
inline uint32_t millis() { return testMillis; }

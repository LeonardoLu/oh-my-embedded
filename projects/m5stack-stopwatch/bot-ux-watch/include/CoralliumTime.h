#pragma once
#include <stdint.h>
#include "CalendarMath.h"
namespace coralliumtime {
constexpr int64_t MinUnixMs=1577836800000LL, MaxUnixMs=4102444800000LL;
inline int64_t localSeconds(int year,int month,int day,int hour,int minute,int second) {
    int64_t days=0;
    for(int y=1970;y<year;++y) days+=watchcalendar::daysInMonth(y,2)==29?366:365;
    for(int m=1;m<month;++m) days+=watchcalendar::daysInMonth(year,m);
    return (days+day-1)*86400+hour*3600+minute*60+second;
}
inline int64_t unixMs(int year,int month,int day,int hour,int minute,int second,int offset) {
    return (localSeconds(year,month,day,hour,minute,second)-offset*60)*1000;
}
inline bool valid(int64_t unixMs,int offset) {
    if(unixMs<MinUnixMs||unixMs>=MaxUnixMs||offset< -720||offset>840) return false;
    // The RX8130CE integration deliberately supports only local 2020–2099.
    const int64_t local=unixMs+(int64_t)offset*60000;
    return local>=MinUnixMs&&local<MaxUnixMs;
}
}

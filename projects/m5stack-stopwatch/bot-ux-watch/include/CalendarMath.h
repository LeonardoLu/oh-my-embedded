#pragma once

#include <stdint.h>

namespace watchcalendar {

constexpr bool leapYear(int16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

constexpr uint8_t daysInMonth(int16_t year, uint8_t month) {
    return month == 2 ? (leapYear(year) ? 29 : 28)
         : (month == 4 || month == 6 || month == 9 || month == 11) ? 30
         : 31;
}

inline uint8_t weekDay(int16_t year, uint8_t month, uint8_t day) {
    static const uint8_t offsets[12] = { 0,3,2,5,0,3,5,1,4,6,2,4 };
    int16_t y = year;
    if (month < 3) --y;
    return (uint8_t)((y + y / 4 - y / 100 + y / 400 + offsets[month - 1] + day) % 7);
}

} // namespace watchcalendar

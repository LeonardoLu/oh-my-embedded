#include "CalendarMath.h"

#include <assert.h>

int main() {
    static_assert(watchcalendar::daysInMonth(2024, 2) == 29, "leap February");
    static_assert(watchcalendar::daysInMonth(2100, 2) == 28, "century rule");
    static_assert(watchcalendar::daysInMonth(2026, 9) == 30, "September");
    static_assert(watchcalendar::daysInMonth(2026, 1) == 31, "January");
    static_assert(watchcalendar::daysInMonth(2026, 11) == 30, "November");
    assert(watchcalendar::weekDay(2026, 9, 6) == 0); // Sunday
    assert(watchcalendar::weekDay(2024, 2, 29) == 4); // Thursday
    assert(watchcalendar::weekDay(2026, 1, 1) == 4); // Thursday
    assert(watchcalendar::weekDay(2099, 12, 31) == 4); // Thursday
    return 0;
}

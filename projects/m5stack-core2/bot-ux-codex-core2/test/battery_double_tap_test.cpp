#include "BatteryDoubleTap.h"

#include <assert.h>
#include <stdint.h>

int main()
{
    BatteryDoubleTap taps;
    assert(!taps.tap(1000));
    assert(taps.armed());
    assert(taps.tap(1000 + BatteryDoubleTap::kWindowMs));
    assert(!taps.armed());

    assert(!taps.tap(2000));
    taps.cancel();
    assert(!taps.tap(2100));
    assert(!taps.tap(2100 + BatteryDoubleTap::kWindowMs + 1));
    assert(taps.tap(2600));

    taps.cancel();
    assert(!taps.tap(UINT32_MAX - 100));
    assert(taps.tap(50));
    return 0;
}

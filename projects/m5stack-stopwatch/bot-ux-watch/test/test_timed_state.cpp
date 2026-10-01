#include "TimedState.h"

#include <assert.h>
#include <stdint.h>

int main() {
    TimedState state;
    assert(!state.active(0));
    assert(!state.active(0x90000000U));

    state.start(100, 1400);
    assert(state.active(100));
    assert(state.active(1499));
    assert(!state.active(1500));
    assert(!state.active(1501));

    state.start(UINT32_MAX - 699U, 1400);
    assert(state.active(UINT32_MAX - 699U));
    assert(state.active(699));
    assert(!state.active(700));
    return 0;
}

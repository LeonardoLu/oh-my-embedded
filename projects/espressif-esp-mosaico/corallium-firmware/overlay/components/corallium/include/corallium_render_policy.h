// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Native GSP owns animation frames independently of the application dispatcher.
 * Only Hub idle polling is relaxed; foreground games retain their 16 ms tick. */
static inline uint32_t corallium_dispatch_interval_ms(bool hub, bool paused, int64_t quiet_us) {
    if (paused) return 100;
    return hub && quiet_us >= 2000000 ? 50 : 16;
}

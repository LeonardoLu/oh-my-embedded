#pragma once

#include <stdint.h>

struct LightingZone {
    uint32_t color = 0;
    uint8_t brightness = 0;
    uint8_t effect = 0;
    uint8_t speed = 0;

    bool active() const { return brightness != 0 && effect != 0; }
};

struct LightingState {
    static constexpr uint8_t kSlotCount = 6;

    LightingZone slots[kSlotCount]{};
    LightingZone keys{};
    LightingZone ambient{};
};

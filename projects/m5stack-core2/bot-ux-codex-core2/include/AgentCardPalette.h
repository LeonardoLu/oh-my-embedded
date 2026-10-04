#pragma once

#include <stdint.h>

// Slot identity is local presentation. It stays independent of the exact host
// status color shown by the card's marker and by the selected Bot.
namespace agentcard {

struct Colors {
    uint32_t fill;
    uint32_t ink;
};

inline Colors colors(uint8_t theme, uint8_t slot)
{
    static constexpr uint32_t fills[3][6] = {
        {0xBED4FF, 0xBFE9C9, 0xFFD39D, 0xDFC4FF, 0xFFC2D0, 0xAFE8EC},
        {0xF3BA91, 0xD7D68E, 0xF3A6A6, 0xD5B4E8, 0xB5D7B8, 0xA8D7DF},
        {0x174A7A, 0x195840, 0x704316, 0x533374, 0x742E45, 0x0D5962},
    };
    static constexpr uint32_t inks[3] = {0x0C182A, 0x371B10, 0xFFFCF2};
    const uint8_t safeTheme = theme < 3 ? theme : 0;
    const uint8_t safeSlot = slot < 6 ? slot : 0;
    return {fills[safeTheme][safeSlot], inks[safeTheme]};
}

} // namespace agentcard

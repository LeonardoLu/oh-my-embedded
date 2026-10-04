#include "AgentCardPalette.h"

#include <assert.h>
#include <math.h>

namespace {

double channel(uint8_t value)
{
    const double v = value / 255.0;
    return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
}

double luminance(uint32_t color)
{
    return channel(static_cast<uint8_t>(color >> 16)) * 0.2126
         + channel(static_cast<uint8_t>(color >> 8)) * 0.7152
         + channel(static_cast<uint8_t>(color)) * 0.0722;
}

double contrast(uint32_t first, uint32_t second)
{
    const double a = luminance(first);
    const double b = luminance(second);
    return (a > b ? a + 0.05 : b + 0.05) / (a > b ? b + 0.05 : a + 0.05);
}

uint16_t rgb565(uint32_t color)
{
    return static_cast<uint16_t>(((color >> 8) & 0xF800)
        | ((color >> 5) & 0x07E0) | ((color >> 3) & 0x001F));
}

void sixSlotsStayDistinctAndReadable()
{
    for (uint8_t theme = 0; theme < 3; ++theme)
    {
        for (uint8_t slot = 0; slot < 6; ++slot)
        {
            const agentcard::Colors current = agentcard::colors(theme, slot);
            assert(contrast(current.fill, current.ink) >= 7.0);
            for (uint8_t prior = 0; prior < slot; ++prior)
                assert(rgb565(current.fill) != rgb565(agentcard::colors(theme, prior).fill));
        }
    }
}

void invalidInputsUsePaperSlotOne()
{
    const agentcard::Colors fallback = agentcard::colors(9, 9);
    const agentcard::Colors first = agentcard::colors(0, 0);
    assert(fallback.fill == first.fill);
    assert(fallback.ink == first.ink);
}

}

int main()
{
    sixSlotsStayDistinctAndReadable();
    invalidInputsUsePaperSlotOne();
}

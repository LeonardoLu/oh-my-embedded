#include "BottomLedFrame.h"

#include <assert.h>
#include <string.h>

using agentsignal::Signal;

static LightingZone zone(Signal signal, uint8_t brightness = 255)
{
    LightingZone result;
    result.brightness = brightness;
    result.effect = 1;
    switch (signal)
    {
        case Signal::Idle: result.color = 0xFFFFFF; break;
        case Signal::Working: result.color = 0x304FFE; break;
        case Signal::NeedsInput: result.color = 0xFF6D00; break;
        case Signal::NewReply: result.color = 0x00FF4C; break;
        case Signal::Error: result.color = 0xFF0033; break;
        case Signal::Unknown: result.color = 0x123456; break;
        case Signal::Off: result = LightingZone{}; break;
    }
    return result;
}

static LightingState working()
{
    LightingState result;
    for (uint8_t i = 0; i < LightingState::kSlotCount; ++i)
        result.slots[i] = zone(Signal::Working);
    return result;
}

static bool equal(const bottomled::Color (&a)[10], const bottomled::Color (&b)[10])
{
    return memcmp(a, b, sizeof(a)) == 0;
}

static bool black(const bottomled::Color& color)
{
    return color.r == 0 && color.g == 0 && color.b == 0;
}

static uint16_t energy(const bottomled::Color& color)
{
    return color.r + color.g + color.b;
}

static bool scaledIdentity(const bottomled::Color& actual, uint8_t theme, uint8_t slot)
{
    const uint32_t identity = agentcard::colors(theme, slot).fill;
    for (uint16_t scale = 1; scale <= 255; ++scale)
    {
        const auto expected = bottomled::scaleColor(identity, static_cast<uint8_t>(scale));
        if (actual.r == expected.r && actual.g == expected.g && actual.b == expected.b) return true;
    }
    return false;
}

static void offBrightnessAndReadiness()
{
    LightingState lighting = working();
    bottomled::Events events;
    bottomled::Color colors[10], allBlack[10]{};
    bottomled::frame(lighting, events, 1000, false, true, 0, 0, 0, true, colors);
    assert(equal(colors, allBlack));
    bottomled::frame(lighting, events, 1000, false, true, 0, 0, 2, false, colors);
    assert(equal(colors, allBlack));
    bottomled::frame(lighting, events, 1000, false, false, 0, 0, 1, true, colors);
    assert(equal(colors, allBlack));

    bottomled::frame(lighting, events, 1000, false, false, 0, 0, 2, true, colors);
    for (uint8_t led : bottomled::kAgentLed) assert(black(colors[led]));
    for (uint8_t led = 3; led <= 6; ++led) assert(!black(colors[led]));

    bottomled::frame(lighting, events, 1000, true, false, 0, 0, 2, true, colors);
    bottomled::Color later[10];
    bottomled::frame(lighting, events, 3700, true, false, 0, 0, 2, true, later);
    assert(equal(colors, later));
}

static void statusMapsEverySlotToIdentity()
{
    LightingState lighting = working();
    const LightingState original = lighting;
    bottomled::Events events;
    bottomled::Color colors[10];
    for (uint8_t theme = 0; theme < 3; ++theme)
    {
        bottomled::frame(lighting, events, 650, false, true, 0, theme, 1, true, colors);
        for (uint8_t slot = 0; slot < LightingState::kSlotCount; ++slot)
        {
            const uint8_t led = bottomled::kAgentLed[slot];
            const uint8_t level = bottomled::zoneLevel(lighting.slots[slot], Signal::Working,
                                                        slot, 650, false);
            const auto expected = bottomled::scaleColor(agentcard::colors(theme, slot).fill, level);
            assert(memcmp(&colors[led], &expected, sizeof(expected)) == 0);
            assert(scaledIdentity(colors[led], theme, slot));
        }
        for (uint8_t led = 3; led <= 6; ++led) assert(black(colors[led]));
    }
    assert(memcmp(&lighting, &original, sizeof(lighting)) == 0);

    lighting.slots[1].brightness = 0;
    lighting.slots[2] = zone(Signal::Unknown);
    lighting.slots[3] = zone(Signal::Off);
    bottomled::frame(lighting, events, 650, false, true, 0, 0, 1, true, colors);
    assert(black(colors[bottomled::kAgentLed[1]]));
    assert(black(colors[bottomled::kAgentLed[2]]));
    assert(black(colors[bottomled::kAgentLed[3]]));

    lighting.slots[0] = zone(Signal::Working, 128);
    bottomled::frame(lighting, events, 650, false, true, 0, 0, 1, true, colors);
    const auto half = colors[bottomled::kAgentLed[0]];
    lighting.slots[0] = zone(Signal::Working, 255);
    bottomled::frame(lighting, events, 650, false, true, 0, 0, 1, true, colors);
    assert(energy(colors[bottomled::kAgentLed[0]]) > energy(half));
}

static void eachStateHasItsOwnRhythm()
{
    const Signal signals[] = {Signal::Idle, Signal::Working, Signal::NeedsInput,
                              Signal::NewReply, Signal::Error};
    uint16_t reducedEnergy[5]{};
    for (uint8_t i = 0; i < 5; ++i)
    {
        LightingState lighting;
        lighting.slots[0] = zone(signals[i]);
        bottomled::Events events;
        bottomled::Color first[10], later[10];
        bottomled::frame(lighting, events, 0, false, true, 0, 2, 1, true, first);
        bottomled::frame(lighting, events, 425, false, true, 0, 2, 1, true, later);
        if (signals[i] == Signal::Idle) assert(equal(first, later));
        else assert(!equal(first, later));

        bottomled::frame(lighting, events, 0, true, true, 0, 2, 1, true, first);
        bottomled::frame(lighting, events, 425, true, true, 0, 2, 1, true, later);
        assert(equal(first, later));
        reducedEnergy[i] = energy(first[0]);
    }
    for (uint8_t i = 1; i < 5; ++i) assert(reducedEnergy[i] > reducedEnergy[i - 1]);
}

static void statusIgnoresLocalEvents()
{
    LightingState lighting = working();
    bottomled::Events events, none;
    events.hasInteraction = true;
    events.interactionAt = 1000;
    events.interactionSlot = 2;
    events.notificationAt = 1000;
    events.notificationMask = 0x3F;
    events.freshReplyMask = 0x3F;
    events.hasControl = true;
    events.controlAt = 1000;
    events.controlHeld = true;
    bottomled::Color actual[10], baseline[10];
    bottomled::frame(lighting, events, 1450, false, true, 2, 1, 1, true, actual);
    bottomled::frame(lighting, none, 1450, false, true, 2, 1, 1, true, baseline);
    assert(equal(actual, baseline));
}

static void aliveAddsIdentityPreservingFeedback()
{
    LightingState lighting = working();
    bottomled::Events events, none;
    bottomled::Color feedback[10], baseline[10];
    bottomled::frame(lighting, none, 1450, false, true, 1, 0, 2, true, baseline);
    assert(scaledIdentity(baseline[1], 0, 1));
    for (uint8_t led = 3; led <= 6; ++led) assert(scaledIdentity(baseline[led], 0, 1));

    events.hasInteraction = true;
    events.interactionAt = 1000;
    events.interactionSlot = 1;
    events.notificationAt = 1000;
    events.notificationMask = 0x02;
    bottomled::frame(lighting, events, 1450, false, true, 1, 0, 2, true, feedback);
    assert(!equal(feedback, baseline));
    for (uint8_t slot = 0; slot < LightingState::kSlotCount; ++slot)
        assert(scaledIdentity(feedback[bottomled::kAgentLed[slot]], 0, slot));

    bottomled::frame(lighting, events, 3000, false, true, 1, 0, 2, true, feedback);
    bottomled::frame(lighting, none, 3000, false, true, 1, 0, 2, true, baseline);
    assert(equal(feedback, baseline));

    events.interactionAt = 0xFFFFFF00u;
    events.notificationAt = 0xFFFFFF00u;
    bottomled::frame(lighting, events, 0x100u, false, true, 1, 0, 2, true, feedback);
    bottomled::frame(lighting, none, 0x100u, false, true, 1, 0, 2, true, baseline);
    assert(!equal(feedback, baseline));
}

static void freshReplyUsesReplyIdentityAndCannotActivateMutedSlots()
{
    LightingState lighting = working();
    lighting.slots[2] = zone(Signal::NewReply);
    bottomled::Events attention, none;
    attention.freshReplyMask = 0x04;
    bottomled::Color vivid[10], baseline[10], later[10];
    bottomled::frame(lighting, attention, 300, false, true, 0, 1, 2, true, vivid);
    bottomled::frame(lighting, none, 300, false, true, 0, 1, 2, true, baseline);
    assert(energy(vivid[2]) > energy(baseline[2]));
    for (uint8_t led = 3; led <= 6; ++led) assert(scaledIdentity(vivid[led], 1, 2));
    bottomled::frame(lighting, attention, 600, false, true, 0, 1, 2, true, later);
    assert(!equal(vivid, later));

    bottomled::frame(lighting, attention, 300, true, true, 0, 1, 2, true, vivid);
    bottomled::frame(lighting, attention, 600, true, true, 0, 1, 2, true, later);
    assert(equal(vivid, later));

    lighting.slots[2].brightness = 0;
    bottomled::frame(lighting, attention, 300, false, true, 2, 1, 2, true, vivid);
    assert(black(vivid[2]));
    for (uint8_t led = 3; led <= 6; ++led) assert(black(vivid[led]));

    lighting.slots[2] = zone(Signal::Unknown);
    bottomled::frame(lighting, attention, 300, false, true, 2, 1, 2, true, vivid);
    assert(black(vivid[2]));
    for (uint8_t led = 3; led <= 6; ++led) assert(black(vivid[led]));
}

static void boundedWorkingChanges()
{
    LightingState lighting = working();
    bottomled::Events events;
    bottomled::Color previous[10], next[10];
    bottomled::frame(lighting, events, 0, false, true, 2, 2, 2, true, previous);
    for (uint32_t now = 50; now <= 8000; now += 50)
    {
        bottomled::frame(lighting, events, now, false, true, 2, 2, 2, true, next);
        for (uint8_t i = 0; i < 10; ++i)
        {
            const int deltaR = static_cast<int>(next[i].r) - previous[i].r;
            const int deltaG = static_cast<int>(next[i].g) - previous[i].g;
            const int deltaB = static_cast<int>(next[i].b) - previous[i].b;
            assert(deltaR >= -35 && deltaR <= 35);
            assert(deltaG >= -35 && deltaG <= 35);
            assert(deltaB >= -35 && deltaB <= 35);
            previous[i] = next[i];
        }
    }
}

int main()
{
    offBrightnessAndReadiness();
    statusMapsEverySlotToIdentity();
    eachStateHasItsOwnRhythm();
    statusIgnoresLocalEvents();
    aliveAddsIdentityPreservingFeedback();
    freshReplyUsesReplyIdentityAndCannotActivateMutedSlots();
    boundedWorkingChanges();
}

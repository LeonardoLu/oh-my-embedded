#include "AgentSignal.h"

#include <assert.h>
#include <string.h>

using agentsignal::Signal;

static LightingZone zone(uint32_t color, uint8_t effect = 1, uint8_t brightness = 255)
{
    LightingZone result;
    result.color = color;
    result.effect = effect;
    result.brightness = brightness;
    return result;
}

static void paletteAndLimits()
{
    assert(agentsignal::decode(zone(0x304FFE), true) == Signal::Working);
    assert(agentsignal::decode(zone(0xFF6D00), true) == Signal::NeedsInput);
    assert(agentsignal::decode(zone(0x00FF4C), true) == Signal::NewReply);
    assert(agentsignal::decode(zone(0xFF0033), true) == Signal::Error);
    assert(agentsignal::decode(zone(0xFFFFFF), true) == Signal::Idle);
    assert(agentsignal::decode(zone(0x304FFE), false) == Signal::Unknown);
    assert(agentsignal::decode(zone(0x304FFD), true) == Signal::Unknown);
    assert(agentsignal::decode(zone(0), true) == Signal::Off);
    assert(agentsignal::decode(zone(0xFFFFFF, 0), true) == Signal::Off);
    assert(agentsignal::decode(zone(0x304FFE, 4, 0), true) == Signal::Working);
    assert(agentsignal::decode(zone(0xFFFFFF, 4), true) == Signal::Idle);
    assert(agentsignal::decode(zone(0x304FFE, 3), true) == Signal::Unknown);
    assert(agentsignal::decode(zone(0, 3), true) == Signal::Unknown);
    assert(strcmp(agentsignal::name(Signal::NewReply), "New reply") == 0);
    assert(!agentsignal::shouldNotify(Signal::Working, Signal::Idle));
    assert(!agentsignal::shouldNotify(Signal::Idle, Signal::NewReply));
}

static void changesAndNotifications()
{
    agentsignal::Model model;
    LightingState lighting;
    lighting.slots[0] = zone(0x304FFE);
    lighting.slots[1] = zone(0xFFFFFF);
    assert(model.update(lighting, true, 100) == 0);
    const uint32_t revision = model.revision();
    assert(model.update(lighting, true, 200) == 0);
    assert(model.revision() == revision);
    assert(model.slot(0).changedAtMs == 100);

    // Lighting effects change the presentation, not the semantic signal.
    lighting.slots[0].effect = 4;
    assert(model.update(lighting, true, 300) == 0);
    assert(model.slot(0).signal == Signal::Working);
    assert(model.slot(0).changedAtMs == 300);
    assert(model.revision() == revision + 1);
    lighting.slots[0] = zone(0x00FF4C);
    assert(model.update(lighting, true, 400) == 1);
    assert(model.update(lighting, true, 500) == 0);
    lighting.slots[0] = zone(0xFF0033);
    assert(model.update(lighting, true, 600) == 0); // same-slot cooldown
    assert(model.update(lighting, true, 4000) == 0); // no delayed replay
    lighting.slots[1] = zone(0xFF6D00);
    assert(model.update(lighting, true, 4100) == 2); // other slot independent
    lighting.slots[0] = zone(0xFFFFFF);
    model.update(lighting, true, 4200);
    lighting.slots[0] = zone(0xFF0033);
    assert(model.update(lighting, true, 4300) == 1);
}

static void reconnectAndLightsOff()
{
    agentsignal::Model model;
    LightingState lighting;
    lighting.slots[0] = zone(0xFF0033);
    assert(model.update(lighting, true, 100) == 0); // first snapshot is silent
    model.update(lighting, false, 200);
    assert(model.slot(0).signal == Signal::Unknown);
    lighting.slots[0] = zone(0xFF6D00);
    assert(model.update(lighting, true, 300) == 0); // reconnect baseline
    LightingState off;
    assert(model.update(off, true, 400) == 0);
    assert(model.slot(0).signal == Signal::Off);
    lighting.slots[0] = zone(0xFF0033);
    assert(model.update(lighting, true, 500) == 0); // inactivity restore baseline
    lighting.slots[0] = zone(0x123456);
    model.update(lighting, true, 600);
    lighting.slots[0] = zone(0xFF6D00);
    assert(model.update(lighting, true, 700) == 0); // unknown is not evidence
}

static void cooldownAcrossMillisWrap()
{
    agentsignal::Model model(100);
    LightingState lighting;
    lighting.slots[5] = zone(0x304FFE);
    model.update(lighting, true, 0xFFFFFFA0u);
    lighting.slots[5] = zone(0xFF6D00);
    assert(model.update(lighting, true, 0xFFFFFFB0u) == 32);
    lighting.slots[5] = zone(0xFFFFFF);
    model.update(lighting, true, 0xFFFFFFC0u);
    lighting.slots[5] = zone(0xFF0033);
    assert(model.update(lighting, true, 0x30u) == 32);
}

int main()
{
    paletteAndLimits();
    changesAndNotifications();
    reconnectAndLightsOff();
    cooldownAcrossMillisWrap();
}

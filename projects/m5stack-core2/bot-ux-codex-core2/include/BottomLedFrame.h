#pragma once

#include <math.h>
#include <stdint.h>

#include "AgentCardPalette.h"
#include "AgentSignal.h"
#include "LightingState.h"

namespace bottomled {
constexpr uint8_t kCount = 10;
constexpr uint8_t kAgentLed[LightingState::kSlotCount] = {0, 1, 2, 7, 8, 9};

struct Color { uint8_t r, g, b; };

struct Events {
    uint32_t interactionAt = 0;
    uint32_t notificationAt = 0;
    uint8_t interactionSlot = 0;
    uint8_t notificationMask = 0;
    bool hasInteraction = false;
    uint32_t controlAt = 0;
    uint8_t freshReplyMask = 0;
    bool hasControl = false;
    bool controlHeld = false;
};

inline Color scaleColor(uint32_t color, uint8_t scale)
{
    return Color{static_cast<uint8_t>(((color >> 16) & 255u) * scale / 255u),
                 static_cast<uint8_t>(((color >> 8) & 255u) * scale / 255u),
                 static_cast<uint8_t>((color & 255u) * scale / 255u)};
}

inline uint32_t mixColor(uint32_t first, uint32_t second, uint8_t amount)
{
    const uint8_t inverse = 255 - amount;
    const uint8_t r = static_cast<uint8_t>((((first >> 16) & 255u) * inverse
                                          + ((second >> 16) & 255u) * amount) / 255u);
    const uint8_t g = static_cast<uint8_t>((((first >> 8) & 255u) * inverse
                                          + ((second >> 8) & 255u) * amount) / 255u);
    const uint8_t b = static_cast<uint8_t>(((first & 255u) * inverse
                                          + (second & 255u) * amount) / 255u);
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

inline float hump(float phase)
{
    if (phase <= 0.0f || phase >= 1.0f) return 0.0f;
    const float sine = sinf(phase * 3.14159265f);
    return sine * sine;
}

inline float cycle(uint32_t nowMs, uint16_t period, uint16_t offset = 0)
{
    return ((nowMs + offset) % period) / static_cast<float>(period);
}

inline float smoothPulse(uint32_t nowMs, uint16_t period, uint16_t offset = 0)
{
    return 0.5f - 0.5f * cosf(cycle(nowMs, period, offset) * 6.2831853f);
}

inline float doubleBeat(uint32_t nowMs, uint16_t offset = 0)
{
    const float phase = cycle(nowMs, 1700, offset);
    const float first = hump(phase / 0.30f);
    const float second = hump((phase - 0.36f) / 0.30f);
    return first > second ? first : second;
}

inline bool activeSignal(agentsignal::Signal signal)
{
    return signal != agentsignal::Signal::Unknown && signal != agentsignal::Signal::Off;
}

inline bool visibleSignal(const LightingZone& zone, agentsignal::Signal signal)
{
    return zone.brightness != 0 && activeSignal(signal);
}

// RGB carries local slot identity. Motion carries the projected host state:
// Idle steady; Working slow breath; Needs input urgent pulse; New reply quick
// pulse; Error double beat. Reduced Motion retains distinct static levels.
inline uint8_t stateLevel(agentsignal::Signal signal, uint8_t slot,
                          uint32_t nowMs, bool reduced)
{
    if (!activeSignal(signal)) return 0;
    if (reduced)
    {
        switch (signal)
        {
            case agentsignal::Signal::Idle: return 76;
            case agentsignal::Signal::Working: return 156;
            case agentsignal::Signal::NeedsInput: return 196;
            case agentsignal::Signal::NewReply: return 220;
            case agentsignal::Signal::Error: return 242;
            default: return 0;
        }
    }
    const uint16_t offset = static_cast<uint16_t>(slot) * 137u;
    switch (signal)
    {
        case agentsignal::Signal::Idle: return 76;
        case agentsignal::Signal::Working:
            return static_cast<uint8_t>(104.0f + 116.0f * smoothPulse(nowMs, 2600, offset));
        case agentsignal::Signal::NeedsInput:
        {
            const float pulse = smoothPulse(nowMs, 1250, offset);
            return static_cast<uint8_t>(112.0f + 143.0f * pulse * pulse);
        }
        case agentsignal::Signal::NewReply:
            return static_cast<uint8_t>(130.0f + 125.0f * smoothPulse(nowMs, 850, offset));
        case agentsignal::Signal::Error:
            return static_cast<uint8_t>(82.0f + 173.0f * doubleBeat(nowMs, offset));
        default: return 0;
    }
}

inline int8_t slotForLed(uint8_t led)
{
    for (uint8_t slot = 0; slot < LightingState::kSlotCount; ++slot)
        if (kAgentLed[slot] == led) return static_cast<int8_t>(slot);
    return -1;
}

inline uint8_t clampLevel(float level)
{
    if (level <= 0.0f) return 0;
    if (level >= 255.0f) return 255;
    return static_cast<uint8_t>(level);
}

inline uint8_t zoneLevel(const LightingZone& zone, agentsignal::Signal signal,
                         uint8_t slot, uint32_t nowMs, bool reduced)
{
    return static_cast<uint8_t>(static_cast<uint16_t>(
        stateLevel(signal, slot, nowMs, reduced)) * zone.brightness / 255u);
}

// Pure, allocation-free color generation. Global LED brightness remains in
// the strip driver; enabled is false when its user level is zero. Theme affects
// presentation RGB only and is never written back into LightingState.
inline void frame(const LightingState& lighting, const Events& events,
                  uint32_t nowMs, bool reduced, bool ready, uint8_t selected,
                  uint8_t theme, uint8_t mode, bool enabled, Color (&colors)[kCount])
{
    for (uint8_t i = 0; i < kCount; ++i) colors[i] = Color{};
    if (!enabled || mode == 0) return;

    if (!ready)
    {
        if (mode == 1) return;
        // Only non-agent pixels show controller presence before app-ready.
        const float breath = reduced ? 0.5f : smoothPulse(nowMs, 5400);
        for (uint8_t i = 3; i <= 6; ++i)
        {
            const float wave = reduced ? 0.5f
                : smoothPulse(nowMs, 6800, static_cast<uint16_t>((i - 3) * 850));
            const uint32_t color = mixColor(0x6287A4, 0x8B729F,
                static_cast<uint8_t>(wave * 128.0f));
            colors[i] = scaleColor(color,
                static_cast<uint8_t>(42.0f + 38.0f * breath + 20.0f * wave));
        }
        return;
    }

    agentsignal::Signal signals[LightingState::kSlotCount]{};
    for (uint8_t slot = 0; slot < LightingState::kSlotCount; ++slot)
    {
        signals[slot] = agentsignal::decode(lighting.slots[slot], true);
        if (!activeSignal(signals[slot])) continue;
        colors[kAgentLed[slot]] = scaleColor(agentcard::colors(theme, slot).fill,
                                              zoneLevel(lighting.slots[slot], signals[slot],
                                                        slot, nowMs, reduced));
    }
    if (mode == 1) return;

    if (selected >= LightingState::kSlotCount) selected = 0;
    const uint32_t interactionAge = nowMs - events.interactionAt;
    const bool interacting = !reduced && events.hasInteraction && interactionAge < 900u;
    const float interactionPhase = interactionAge / 900.0f;
    const float sweepHead = interacting ?
        kAgentLed[events.interactionSlot < LightingState::kSlotCount ? events.interactionSlot : 0]
        + 6.0f * interactionPhase - 3.0f : 0.0f;
    const uint32_t notificationAge = nowMs - events.notificationAt;
    const float notice = !reduced && notificationAge < 1800u ?
        hump(notificationAge / 1800.0f) : 0.0f;

    // Alive adds selection and bounded interaction emphasis without replacing
    // a slot's identity hue or activating an Off/Unknown slot.
    for (uint8_t led = 0; led < kCount; ++led)
    {
        const int8_t slot = slotForLed(led);
        if (slot < 0 || !activeSignal(signals[slot])) continue;
        const float hostScale = lighting.slots[slot].brightness / 255.0f;
        if (hostScale <= 0.0f) continue;
        float level = stateLevel(signals[slot], static_cast<uint8_t>(slot), nowMs, reduced);
        if (slot == selected) level += 24.0f;
        if (!reduced)
        {
            const float wave = smoothPulse(nowMs, 8000, static_cast<uint16_t>(led) * 800u);
            level += 24.0f * wave * wave;
        }
        if (interacting)
        {
            const float distance = fabsf(led - sweepHead);
            if (distance < 2.5f)
                level += 62.0f * (1.0f - distance / 2.5f) * hump(interactionPhase);
        }
        if (events.notificationMask & (1u << slot)) level += 82.0f * notice;
        const bool fresh = signals[slot] == agentsignal::Signal::NewReply
            && (events.freshReplyMask & (1u << slot));
        if (fresh)
        {
            const float attention = reduced ? 0.72f
                : smoothPulse(nowMs, 700, static_cast<uint16_t>(led) * 70u);
            level += 76.0f * attention;
        }
        level *= hostScale;
        colors[led] = scaleColor(agentcard::colors(theme, static_cast<uint8_t>(slot)).fill,
                                 clampLevel(level));
    }

    int8_t focus = visibleSignal(lighting.slots[selected], signals[selected])
        ? static_cast<int8_t>(selected) : -1;
    for (uint8_t slot = 0; slot < LightingState::kSlotCount; ++slot)
        if (visibleSignal(lighting.slots[slot], signals[slot])
            && signals[slot] == agentsignal::Signal::NewReply
            && (events.freshReplyMask & (1u << slot))) { focus = slot; break; }
    if (focus < 0) return;

    const uint32_t focusColor = agentcard::colors(theme, static_cast<uint8_t>(focus)).fill;
    const bool freshFocus = signals[focus] == agentsignal::Signal::NewReply
        && (events.freshReplyMask & (1u << focus));
    const uint32_t controlAge = nowMs - events.controlAt;
    const bool controlPulse = !reduced && events.hasControl && controlAge < 900u;
    const float controlHead = -1.0f + 6.0f * (controlAge / 900.0f);
    for (uint8_t led = 3; led <= 6; ++led)
    {
        const float hostScale = lighting.slots[focus].brightness / 255.0f;
        if (hostScale <= 0.0f) continue;
        float level = 30.0f + stateLevel(signals[focus], static_cast<uint8_t>(focus), nowMs, reduced) * 0.22f;
        if (!reduced) level += 24.0f * smoothPulse(nowMs, 3200,
                                                   static_cast<uint16_t>((led - 3) * 480));
        if (freshFocus)
        {
            const float attention = reduced ? 0.75f
                : smoothPulse(nowMs, 900, static_cast<uint16_t>((led - 3) * 150));
            level += 104.0f * attention;
        }
        if (events.controlHeld)
        {
            const float held = reduced ? 0.6f : smoothPulse(nowMs, 1700);
            level += 72.0f * held;
        }
        if (controlPulse)
        {
            const float distance = fabsf((led - 3) - controlHead);
            if (distance < 1.8f)
                level += 104.0f * (1.0f - distance / 1.8f) * hump(controlAge / 900.0f);
        }
        level *= hostScale;
        colors[led] = scaleColor(focusColor, clampLevel(level));
    }
}

} // namespace bottomled

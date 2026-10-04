#include "FreshReplyAttention.h"

#include <assert.h>

static LightingZone zone(uint32_t color)
{
    LightingZone result;
    result.color = color;
    result.effect = 1;
    result.brightness = 255;
    return result;
}

static void eventStartsOnceAndExpires()
{
    agentsignal::Model signals;
    freshreply::Attention attention;
    LightingState lighting;
    lighting.slots[0] = zone(0x304FFE); // authoritative Working signal
    assert(signals.update(lighting, true, 100) == 0); // initial baseline is silent
    lighting.slots[0] = zone(0x00FF4C); // authoritative NewReply signal
    const uint8_t notifications = signals.update(lighting, true, 200);
    assert(freshreply::fromNotifications(notifications, signals) == 1);
    attention.start(freshreply::fromNotifications(notifications, signals), 200);
    assert(attention.mask(200) == 1);
    assert(attention.mask(30199) == 1);
    assert(attention.mask(30200) == 0);

    // Re-reading persistent green produces no event and cannot renew attention.
    assert(signals.update(lighting, true, 1000) == 0);
    assert(freshreply::fromNotifications(0, signals) == 0);

    freshreply::Attention stale;
    stale.start(1, 31000);
    lighting.slots[0] = zone(0x304FFE);
    signals.update(lighting, true, 31100);
    stale.retain(freshreply::current(signals));
    assert(stale.mask(31100) == 0); // a newer host state ends stale attention
}

static void interactionDismissesAllSlots()
{
    freshreply::Attention attention;
    attention.start(0x21, 1000);
    assert(attention.mask(1500) == 0x21);
    attention.dismiss();
    assert(attention.mask(1500) == 0);
}

static void onlyFreshRepliesAttract()
{
    agentsignal::Model signals;
    LightingState lighting;
    lighting.slots[1] = zone(0xFFFFFF);
    lighting.slots[2] = zone(0x304FFE);
    signals.update(lighting, true, 100);
    lighting.slots[1] = zone(0xFF6D00); // NeedsInput notification
    lighting.slots[2] = zone(0x00FF4C); // NewReply notification
    const uint8_t notifications = signals.update(lighting, true, 3000);
    assert(notifications == 0x06);
    assert(freshreply::fromNotifications(notifications, signals) == 0x04);

    signals.update(lighting, false, 3100);
    assert(freshreply::current(signals) == 0);
    lighting.slots[2] = zone(0x00FF4C);
    const uint8_t reconnect = signals.update(lighting, true, 3200);
    assert(reconnect == 0); // reconnect baseline never creates 30-second attention
    assert(freshreply::fromNotifications(reconnect, signals) == 0);
}

static void expiryHandlesMillisWrap()
{
    freshreply::Attention attention;
    attention.start(0x08, 0xFFFFFF00u);
    assert(attention.mask(0x00000100u) == 0x08);
    assert(attention.mask(0x0000742Fu) == 0x08);
    assert(attention.mask(0x00007430u) == 0);
}

int main()
{
    eventStartsOnceAndExpires();
    interactionDismissesAllSlots();
    onlyFreshRepliesAttract();
    expiryHandlesMillisWrap();
}

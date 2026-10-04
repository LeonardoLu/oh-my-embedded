#pragma once

#include <stdint.h>

#include "AgentSignal.h"

namespace freshreply {

constexpr uint32_t kDurationMs = 30000;

inline uint8_t fromNotifications(uint8_t notifications, const agentsignal::Model& signals)
{
    uint8_t replies = 0;
    for (uint8_t i = 0; i < agentsignal::Model::kSlotCount; ++i)
        if ((notifications & (1u << i)) &&
            signals.slot(i).signal == agentsignal::Signal::NewReply)
            replies |= static_cast<uint8_t>(1u << i);
    return replies;
}

inline uint8_t current(const agentsignal::Model& signals)
{
    uint8_t replies = 0;
    for (uint8_t i = 0; i < agentsignal::Model::kSlotCount; ++i)
        if (signals.slot(i).signal == agentsignal::Signal::NewReply)
            replies |= static_cast<uint8_t>(1u << i);
    return replies;
}

// Tracks only fresh NewReply transitions supplied by AgentSignal. Reading a
// persistent green host zone cannot renew or create the attention window.
class Attention {
public:
    void start(uint8_t mask, uint32_t nowMs)
    {
        for (uint8_t i = 0; i < agentsignal::Model::kSlotCount; ++i)
            if (mask & (1u << i)) _until[i] = nowMs + kDurationMs;
    }

    void dismiss()
    {
        for (auto& until : _until) until = 0;
    }

    void retain(uint8_t mask)
    {
        for (uint8_t i = 0; i < agentsignal::Model::kSlotCount; ++i)
            if (!(mask & (1u << i))) _until[i] = 0;
    }

    uint8_t mask(uint32_t nowMs) const
    {
        uint8_t active = 0;
        for (uint8_t i = 0; i < agentsignal::Model::kSlotCount; ++i)
            if (_until[i] && static_cast<int32_t>(_until[i] - nowMs) > 0)
                active |= static_cast<uint8_t>(1u << i);
        return active;
    }

private:
    uint32_t _until[agentsignal::Model::kSlotCount]{};
};

} // namespace freshreply

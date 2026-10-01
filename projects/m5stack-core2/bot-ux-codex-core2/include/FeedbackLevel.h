#pragma once

#include <stdint.h>

namespace corefeedback {

constexpr uint8_t kDefaultLevel = 3;
constexpr uint8_t kLevelCount = 6;
// M5Unified defaults the Core2 speaker master to 64. Its mixer applies the
// square of that value, so 128 gives the app four times the prior amplitude.
// With Core2 gain 16 and channel 255, the app's one mono channel remains bounded
// near +/-7,442 after the pinned mixer's normalization and final shift.
constexpr uint8_t kSpeakerMasterVolume = 128;

inline uint8_t normalizeLevel(uint8_t level)
{
    return level < kLevelCount ? level : kDefaultLevel;
}

// With the higher hardware master, levels 1-3 preserve their former effective
// drive. Levels 4-5 deliberately open more of the available speaker headroom.
inline uint8_t synthVolume(uint8_t level)
{
    static const uint8_t volumes[kLevelCount] = {0, 16, 30, 45, 96, 255};
    return volumes[normalizeLevel(level)];
}

} // namespace corefeedback

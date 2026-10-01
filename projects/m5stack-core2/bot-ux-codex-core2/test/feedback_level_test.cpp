#include "FeedbackLevel.h"

#include <assert.h>

int main()
{
    assert(corefeedback::normalizeLevel(0) == 0);
    assert(corefeedback::normalizeLevel(5) == 5);
    assert(corefeedback::normalizeLevel(6) == corefeedback::kDefaultLevel);
    assert(corefeedback::normalizeLevel(255) == corefeedback::kDefaultLevel);
    assert(corefeedback::synthVolume(0) == 0);
    assert(corefeedback::synthVolume(1) == 16);
    assert(corefeedback::synthVolume(2) == 30);
    assert(corefeedback::synthVolume(3) == 45);
    assert(corefeedback::synthVolume(4) == 96);
    assert(corefeedback::synthVolume(5) == 255);
    assert(corefeedback::kSpeakerMasterVolume == 128);
    for (uint8_t level = 1; level < corefeedback::kLevelCount; ++level)
        assert(corefeedback::synthVolume(level) > corefeedback::synthVolume(level - 1));

    // M5Unified squares its master gain. Default level 3 preserves the old
    // 180 synth x 64^2 speaker drive; maximum is exactly four times louder.
    const uint32_t oldDefaultDrive = 180u * 64u * 64u;
    const uint32_t oldMaximumDrive = 255u * 64u * 64u;
    const uint32_t newDefaultDrive = corefeedback::synthVolume(3) * 128u * 128u;
    const uint32_t newMaximumDrive = corefeedback::synthVolume(5) * 128u * 128u;
    assert(newDefaultDrive == oldDefaultDrive);
    assert(newMaximumDrive == oldMaximumDrive * 4u);
}

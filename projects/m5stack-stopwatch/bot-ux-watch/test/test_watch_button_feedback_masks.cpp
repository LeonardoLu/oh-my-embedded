#include "WatchButtonFeedback.h"
#include "WatchButtonFeedbackMasks.h"

#include <assert.h>
#include <vector>

int main() {
    std::vector<uint8_t> decoded(watchbuttonmasks::DecodedSize);
    assert(watchbuttonmasks::decode(decoded.data(), decoded.size()));
    assert(!watchbuttonmasks::decode(nullptr, decoded.size()));
    assert(!watchbuttonmasks::decode(decoded.data(), decoded.size() - 1));
    assert(!watchbuttonmasks::decodeData(watchbuttonmasks::Encoded,
        watchbuttonmasks::EncodedSize - 1, decoded.data(), decoded.size()));
    const uint8_t shortLiteral[] = {2, 17, 18};
    assert(!watchbuttonmasks::decodeData(shortLiteral, sizeof(shortLiteral),
                                         decoded.data(), 3));
    const uint8_t outputOverrun[] = {0x7F};
    assert(!watchbuttonmasks::decodeData(outputOverrun, sizeof(outputOverrun),
                                         decoded.data(), 63));
    const uint8_t exact[] = {1, 7, 9, 0x42, 0x80, 0xC1, 23};
    uint8_t small[8] = {};
    assert(watchbuttonmasks::decodeData(exact, sizeof(exact), small, sizeof(small)));
    const uint8_t expected[] = {7, 9, 0, 0, 0, 255, 23, 23};
    for (size_t i = 0; i < sizeof(small); ++i) assert(small[i] == expected[i]);

    size_t index = 0;
    const watchbuttons::Button buttons[] = {
        watchbuttons::A, watchbuttons::B, watchbuttons::Power
    };
    for (auto button : buttons) {
        auto bounds = watchbuttons::dirtyBounds(button);
        for (uint8_t step = 0; step <= watchbuttons::Feedback::ExpandSteps; ++step) {
            auto blob = watchbuttons::expandedBlob(button, step);
            for (int16_t y = 0; y < bounds.h; ++y)
                for (int16_t x = 0; x < bounds.w; ++x) {
                    uint8_t expected = watchbuttons::blobCoverage(
                        blob, bounds.x + x + 0.5f, bounds.y + y + 0.5f);
                    assert(index < decoded.size());
                    assert(decoded[index++] == expected);
                }
        }
    }
    assert(index == decoded.size());
    return 0;
}

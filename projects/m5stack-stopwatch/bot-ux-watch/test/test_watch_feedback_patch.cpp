#include "WatchFeedbackPatch.h"

#include <assert.h>

using watchbuttons::Bounds;

static bool contains(const Bounds& bounds, int x, int y) {
    return x >= bounds.x && x < bounds.x + bounds.w
        && y >= bounds.y && y < bounds.y + bounds.h;
}

static Bounds amoledFlushBounds(const Bounds& bounds) {
    int16_t left = bounds.x & ~1;
    int16_t top = bounds.y & ~1;
    int16_t right = (bounds.x + bounds.w - 1) | 1;
    int16_t bottom = (bounds.y + bounds.h - 1) | 1;
    return {left, top, (int16_t)(right - left + 1),
            (int16_t)(bottom - top + 1)};
}

static void verify(const Bounds& source, const Bounds* cuts, size_t cutCount) {
    auto regions = watchfeedbackpatch::visibleRegions(source, cuts, cutCount);
    assert(regions.count > 0);
    uint32_t expectedCount = 0;
    for (int y = 0; y < 466; ++y) for (int x = 0; x < 466; ++x) {
        bool expected = contains(source, x, y);
        for (size_t i = 0; i < cutCount; ++i)
            if (contains(cuts[i], x, y)) expected = false;
        uint8_t found = 0;
        for (uint8_t i = 0; i < regions.count; ++i) {
            const auto& region = regions.items[i];
            assert(region.w > 0 && region.h > 0);
            assert(region.x >= source.x && region.y >= source.y);
            assert(region.x + region.w <= source.x + source.w);
            assert(region.y + region.h <= source.y + source.h);
            if (contains(region, x, y)) ++found;
        }
        assert(found == (expected ? 1 : 0));
        if (expected) ++expectedCount;
    }
    assert(watchfeedbackpatch::pixelCount(regions) == expectedCount);
}

int main() {
    using namespace watchbuttons;
    constexpr Bounds bot{90, 90, 286, 286};
    constexpr Bounds hud[] = {{0, 0, 466, 90}, {0, 376, 466, 90}};
    const Button buttons[] = {A, B, Power};

    for (Button button : buttons) {
        Bounds source = dirtyBounds(button);
        verify(source, &bot, 1);
        Bounds cuts[] = {bot, hud[0], hud[1]};
        verify(source, cuts, 3);

        // Excluding both submitted HUD bands is equivalent to first clipping
        // the liquid patch to the middle band and then excluding the bot.
        constexpr Bounds middle{0, 90, 466, 286};
        Bounds middleSource = watchfeedbackpatch::intersection(source, middle);
        auto middleRegions = watchfeedbackpatch::visibleRegions(middleSource, &bot, 1);
        auto noHudRegions = watchfeedbackpatch::visibleRegions(source, &bot, 1);
        assert(watchfeedbackpatch::pixelCount(middleRegions)
               <= watchfeedbackpatch::pixelCount(noHudRegions));
        for (uint8_t i = 0; i < noHudRegions.count; ++i)
            assert(watchfeedbackpatch::empty(watchfeedbackpatch::intersection(
                amoledFlushBounds(noHudRegions.items[i]), bot)));
        for (uint8_t i = 0; i < middleRegions.count; ++i) {
            Bounds flushed = amoledFlushBounds(middleRegions.items[i]);
            assert(watchfeedbackpatch::empty(
                watchfeedbackpatch::intersection(flushed, bot)));
            assert(watchfeedbackpatch::empty(
                watchfeedbackpatch::intersection(flushed, hud[0])));
            assert(watchfeedbackpatch::empty(
                watchfeedbackpatch::intersection(flushed, hud[1])));
        }

        auto regions = watchfeedbackpatch::visibleRegions(source, &bot, 1);
        for (uint8_t step = 0; step <= Feedback::ExpandSteps; ++step) {
            Blob blob = expandedBlob(button, step);
            for (int y = source.y; y < source.y + source.h; ++y)
                for (int x = source.x; x < source.x + source.w; ++x) {
                    uint8_t coverage = blobCoverage(blob, x + 0.5f, y + 0.5f);
                    if (!coverage) continue;
                    uint8_t found = 0;
                    for (uint8_t i = 0; i < regions.count; ++i)
                        if (contains(regions.items[i], x, y)) ++found;
                    assert(found == 1);

                    uint8_t productionFound = y < 90 || y >= 376;
                    for (uint8_t i = 0; i < middleRegions.count; ++i)
                        if (contains(middleRegions.items[i], x, y)) ++productionFound;
                    assert(productionFound == 1);
                }
        }
    }

    // Interior, edge, no-overlap and complete cuts establish general behavior;
    // this helper is not specialized to the current three button rectangles.
    Bounds source{10, 20, 30, 40};
    Bounds interior{17, 31, 8, 9};
    verify(source, &interior, 1);
    Bounds edge{5, 30, 20, 50};
    verify(source, &edge, 1);
    Bounds outside{100, 100, 2, 2};
    verify(source, &outside, 1);
    Bounds all{0, 0, 466, 466};
    auto none = watchfeedbackpatch::visibleRegions(source, &all, 1);
    assert(none.count == 0 && watchfeedbackpatch::pixelCount(none) == 0);
    return 0;
}

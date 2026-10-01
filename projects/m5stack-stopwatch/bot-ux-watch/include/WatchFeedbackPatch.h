#pragma once

#include <stddef.h>
#include <stdint.h>

#include "WatchButtonFeedback.h"

namespace watchfeedbackpatch {

using watchbuttons::Bounds;

constexpr uint8_t MaxRegions = 12;

struct Regions {
    Bounds items[MaxRegions];
    uint8_t count;
};

inline bool empty(const Bounds& bounds) {
    return bounds.w <= 0 || bounds.h <= 0;
}

inline Bounds intersection(const Bounds& a, const Bounds& b) {
    int16_t x = a.x > b.x ? a.x : b.x;
    int16_t y = a.y > b.y ? a.y : b.y;
    int16_t right = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w;
    int16_t bottom = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;
    return Bounds{x, y, (int16_t)(right > x ? right - x : 0),
                  (int16_t)(bottom > y ? bottom - y : 0)};
}

inline bool append(Regions& regions, const Bounds& bounds) {
    if (empty(bounds)) return true;
    if (regions.count >= MaxRegions) return false;
    regions.items[regions.count++] = bounds;
    return true;
}

// Subtract one half-open rectangle into non-overlapping top, bottom, left and
// right pieces. Pixels outside the cut occur exactly once in the result.
inline bool subtractOne(const Bounds& source, const Bounds& cut, Regions& result) {
    Bounds overlap = intersection(source, cut);
    if (empty(overlap)) return append(result, source);
    int16_t sourceRight = source.x + source.w;
    int16_t sourceBottom = source.y + source.h;
    int16_t overlapRight = overlap.x + overlap.w;
    int16_t overlapBottom = overlap.y + overlap.h;
    return append(result, Bounds{source.x, source.y, source.w,
                                 (int16_t)(overlap.y - source.y)})
        && append(result, Bounds{source.x, overlapBottom, source.w,
                                 (int16_t)(sourceBottom - overlapBottom)})
        && append(result, Bounds{source.x, overlap.y,
                                 (int16_t)(overlap.x - source.x), overlap.h})
        && append(result, Bounds{overlapRight, overlap.y,
                                 (int16_t)(sourceRight - overlapRight), overlap.h});
}

inline bool subtract(Regions& regions, const Bounds& cut) {
    Regions next{{}, 0};
    for (uint8_t i = 0; i < regions.count; ++i)
        if (!subtractOne(regions.items[i], cut, next)) return false;
    regions = next;
    return true;
}

inline Regions visibleRegions(const Bounds& source, const Bounds* exclusions,
                              size_t exclusionCount) {
    Regions result{{}, 0};
    append(result, source);
    for (size_t i = 0; i < exclusionCount; ++i)
        if (!subtract(result, exclusions[i])) return Regions{{}, 0};
    return result;
}

inline uint32_t pixelCount(const Regions& regions) {
    uint32_t result = 0;
    for (uint8_t i = 0; i < regions.count; ++i)
        result += (uint32_t)regions.items[i].w * regions.items[i].h;
    return result;
}

} // namespace watchfeedbackpatch

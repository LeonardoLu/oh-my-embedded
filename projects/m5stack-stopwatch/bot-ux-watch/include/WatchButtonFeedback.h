#pragma once

#include <math.h>
#include <stdint.h>

namespace watchbuttons {

enum Button : uint8_t {
    A = 1 << 0,
    B = 1 << 1,
    Power = 1 << 2,
};

struct Blob {
    int16_t centerDegrees;
    int16_t halfDegrees;
    int16_t depth;
    uint16_t color;
};

struct Bounds {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
};

constexpr int16_t DisplayCenter = 233;
constexpr int16_t DisplayRadius = 233;

// Angles use screen coordinates: 0 degrees points right and 90 degrees down.
// The locations and colors match the StopWatch enclosure: yellow A at the
// upper-left edge, blue B at the upper-right edge, and red power at lower-left.
constexpr Blob restingBlob(Button button) {
    return button == A ? Blob{220, 28, 25, 0xFFE0}
         : button == B ? Blob{320, 28, 25, 0x34BF}
                       : Blob{135, 24, 21, 0xF800};
}

inline Blob expandedBlob(Button button, uint8_t step) {
    static const uint8_t sideHalf[] = {3, 8, 14, 20, 25, 30, 29, 28, 28};
    static const uint8_t sideDepth[] = {8, 13, 18, 22, 25, 28, 26, 25, 25};
    static const uint8_t powerHalf[] = {3, 7, 12, 17, 21, 26, 25, 24, 24};
    static const uint8_t powerDepth[] = {7, 11, 15, 18, 21, 24, 22, 21, 21};
    if (step > 8) step = 8;
    Blob result = restingBlob(button);
    bool power = button == Power;
    result.halfDegrees = power ? powerHalf[step] : sideHalf[step];
    result.depth = power ? powerDepth[step] : sideDepth[step];
    return result;
}

// Fixed maximum dirty rectangles cover every animation step plus one AA pixel.
// Keeping these areas stable lets the renderer restore and submit only the edge
// pixels touched by a button instead of flashing a full framebuffer.
constexpr Bounds dirtyBounds(Button button) {
    return button == A ? Bounds{0, 10, 160, 187}
         : button == B ? Bounds{306, 10, 160, 187}
                       : Bounds{8, 305, 155, 155};
}

inline float angularDelta(float degrees, float center) {
    float result = degrees - center;
    while (result > 180.0f) result -= 360.0f;
    while (result < -180.0f) result += 360.0f;
    return result;
}

inline float blobInset(const Blob& blob, float degrees) {
    float u = (degrees - blob.centerDegrees) / blob.halfDegrees;
    if (u <= -1.0f || u >= 1.0f) return 0.0f;
    float base = 1.0f - u * u;
    return blob.depth * base * sqrtf(base);
}

// Analytic single-pixel coverage. Coordinates are pixel centers. Both the
// physical outer circle and the liquid inner contour receive a one-pixel blend
// ramp, so the color is mixed with the actual page pixel rather than black.
inline uint8_t blobCoverage(const Blob& blob, float x, float y) {
    float dx = x - DisplayCenter, dy = y - DisplayCenter;
    float radius = sqrtf(dx * dx + dy * dy);
    if (radius > DisplayRadius + 0.5f
        || radius < DisplayRadius - blob.depth - 0.5f) return 0;
    constexpr float kDegrees = 57.295779513082320876f;
    float degrees = atan2f(dy, dx) * kDegrees;
    float delta = angularDelta(degrees, blob.centerDegrees);
    if (delta <= -blob.halfDegrees || delta >= blob.halfDegrees) return 0;
    float inset = blobInset(blob, blob.centerDegrees + delta);
    float outerCoverage = DisplayRadius + 0.5f - radius;
    float innerCoverage = radius - (DisplayRadius - inset) + 0.5f;
    float coverage = fminf(outerCoverage, innerCoverage);
    if (coverage <= 0.0f) return 0;
    if (coverage >= 1.0f) return 255;
    return (uint8_t)(coverage * 255.0f + 0.5f);
}

class Feedback {
public:
    static constexpr uint8_t ExpandSteps = 8;
    static constexpr uint32_t ExpandMs = 160;

    bool sample(bool aPressed, bool bPressed, bool powerValid, bool powerPressed,
                uint32_t nowMs) {
        uint8_t next = (aPressed ? A : 0) | (bPressed ? B : 0)
            | (powerValid && powerPressed ? Power : 0);
        bool changed = next != _held;
        uint8_t pressed = next & ~_held;
        if (pressed & A) _pressedAt[0] = nowMs;
        if (pressed & B) _pressedAt[1] = nowMs;
        if (pressed & Power) _pressedAt[2] = nowMs;
        _held = next;
        if (changed) updateSteps(nowMs);
        return changed;
    }

    bool advance(uint32_t nowMs) {
        uint8_t prior[3] = {_steps[0], _steps[1], _steps[2]};
        updateSteps(nowMs);
        return prior[0] != _steps[0] || prior[1] != _steps[1] || prior[2] != _steps[2];
    }

    uint8_t held() const { return _held; }
    bool isHeld(Button button) const { return (_held & button) != 0; }
    uint8_t expandStep(Button button) const { return _steps[index(button)]; }

private:
    static uint8_t index(Button button) { return button == A ? 0 : button == B ? 1 : 2; }

    void updateSteps(uint32_t nowMs) {
        const Button buttons[3] = {A, B, Power};
        for (uint8_t i = 0; i < 3; ++i) {
            if (!isHeld(buttons[i])) {
                _steps[i] = 0;
                continue;
            }
            uint32_t elapsed = nowMs - _pressedAt[i];
            uint32_t step = elapsed * ExpandSteps / ExpandMs;
            _steps[i] = step > ExpandSteps ? ExpandSteps : (uint8_t)step;
        }
    }

    uint8_t _held = 0;
    uint8_t _steps[3] = {0, 0, 0};
    uint32_t _pressedAt[3] = {0, 0, 0};
};

} // namespace watchbuttons

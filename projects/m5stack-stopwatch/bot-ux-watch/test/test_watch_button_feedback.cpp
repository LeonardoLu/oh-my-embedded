#include "WatchButtonFeedback.h"

#include <assert.h>

int main() {
    using namespace watchbuttons;

    Feedback feedback;
    assert(feedback.held() == 0);
    assert(!feedback.sample(false, false, true, false, 90));

    assert(feedback.sample(true, false, true, false, 100));
    assert(feedback.isHeld(A));
    assert(!feedback.isHeld(B));
    assert(!feedback.isHeld(Power));
    assert(feedback.expandStep(A) == 0);
    assert(!feedback.sample(true, false, true, false, 100));
    assert(feedback.advance(160));
    assert(feedback.expandStep(A) == 3);
    assert(feedback.advance(260));
    assert(feedback.expandStep(A) == Feedback::ExpandSteps);
    assert(!feedback.advance(270));

    assert(feedback.sample(true, true, true, true, 280));
    assert(feedback.held() == (A | B | Power));
    assert(feedback.expandStep(A) == Feedback::ExpandSteps);
    assert(feedback.expandStep(B) == 0);
    assert(feedback.expandStep(Power) == 0);
    assert(feedback.advance(340));
    assert(feedback.expandStep(A) == Feedback::ExpandSteps);
    assert(feedback.expandStep(B) == 3);
    assert(feedback.expandStep(Power) == 3);

    // A failed PMIC read means unknown, so power feedback disappears instead
    // of getting stuck while the independently sampled GPIO buttons remain.
    assert(feedback.sample(true, true, false, true, 340));
    assert(feedback.held() == (A | B));
    assert(!feedback.isHeld(Power));

    assert(feedback.sample(false, false, true, false, 341));
    assert(feedback.held() == 0);

    Blob a = restingBlob(A), b = restingBlob(B), power = restingBlob(Power);
    assert(a.centerDegrees == 220 && a.halfDegrees == 28 && a.depth == 25 && a.color == 0xFFE0);
    assert(b.centerDegrees == 320 && b.halfDegrees == 28 && b.depth == 25 && b.color == 0x34BF);
    assert(power.centerDegrees == 135 && power.halfDegrees == 24 && power.depth == 21 && power.color == 0xF800);
    assert(blobInset(a, a.centerDegrees - a.halfDegrees) == 0.0f);
    assert(blobInset(a, a.centerDegrees + a.halfDegrees) == 0.0f);
    assert(blobInset(a, a.centerDegrees) == a.depth);
    assert(blobInset(a, a.centerDegrees - 7) == blobInset(a, a.centerDegrees + 7));
    assert(expandedBlob(A, 0).halfDegrees == 3 && expandedBlob(A, 0).depth == 8);
    assert(expandedBlob(A, 5).halfDegrees > a.halfDegrees);
    assert(expandedBlob(A, 5).depth > a.depth);
    assert(expandedBlob(A, Feedback::ExpandSteps).halfDegrees == a.halfDegrees);
    assert(expandedBlob(Power, Feedback::ExpandSteps).depth == power.depth);
    const Button buttons[] = {A, B, Power};
    for (Button button : buttons) {
        for (uint8_t step = 0; step <= Feedback::ExpandSteps; ++step) {
            Blob blob = expandedBlob(button, step);
            float prior = 0.0f;
            for (int16_t delta = -blob.halfDegrees; delta <= 0; ++delta) {
                float inset = blobInset(blob, blob.centerDegrees + delta);
                assert(inset >= prior && inset <= blob.depth);
                assert(inset == blobInset(blob, blob.centerDegrees - delta));
                prior = inset;
            }
        }
        Bounds bounds = dirtyBounds(button);
        assert(bounds.x >= 0 && bounds.y >= 0);
        assert(bounds.x + bounds.w <= 466 && bounds.y + bounds.h <= 466);
        for (uint8_t step = 0; step <= Feedback::ExpandSteps; ++step) {
            Blob blob = expandedBlob(button, step);
            for (int y = 0; y < 466; ++y) for (int x = 0; x < 466; ++x) {
                uint8_t coverage = blobCoverage(blob, x + 0.5f, y + 0.5f);
                bool inBounds = x >= bounds.x && y >= bounds.y
                    && x < bounds.x + bounds.w && y < bounds.y + bounds.h;
                assert(!coverage || inBounds);
                // The maximum liquid pixels stop short of the animated bot
                // sprite, so its opaque push cannot cut through the overlay.
                bool inBot = x >= 90 && x < 376 && y >= 90 && y < 376;
                assert(!coverage || !inBot);
            }
        }
    }
    // The analytic edge is opaque at the middle of the liquid, blended at its
    // contours, symmetric, and absent on either side of the pointed tips.
    assert(blobCoverage(a, 233 + cosf(220 * 3.14159265f / 180) * 220.5f,
                           233 + sinf(220 * 3.14159265f / 180) * 220.5f) == 255);
    assert(blobCoverage(a, 233 + cosf(220 * 3.14159265f / 180) * 207.5f,
                           233 + sinf(220 * 3.14159265f / 180) * 207.5f) < 255);
    assert(blobCoverage(a, 233 + cosf(192 * 3.14159265f / 180) * 232.0f,
                           233 + sinf(192 * 3.14159265f / 180) * 232.0f) == 0);
    return 0;
}

#include <BotUx.h>

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static uint32_t gNow = 1000;
uint32_t millis() { return gNow; }

using Bot = botux::BotUx;

struct Raster {
    uint64_t hash;
    int ink;
    int components;
    int left, top, right, bottom;
};

static uint16_t bgColor() { return botux::rgb565(0x0A, 0x0E, 0x14); }

static Raster inspect(const M5Canvas& canvas, int size) {
    Raster result = {1469598103934665603ULL, 0, 0, size, size, -1, -1};
    std::vector<uint8_t> seen((size_t)size * size, 0);
    std::vector<int> queue((size_t)size * size);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            uint16_t pixel = canvas.readPixel(x, y);
            result.hash ^= pixel;
            result.hash *= 1099511628211ULL;
            if (pixel == bgColor()) continue;
            ++result.ink;
            if (x < result.left) result.left = x;
            if (x > result.right) result.right = x;
            if (y < result.top) result.top = y;
            if (y > result.bottom) result.bottom = y;
        }
    }
    for (int start = 0; start < size * size; ++start) {
        int sx = start % size, sy = start / size;
        if (seen[start] || canvas.readPixel(sx, sy) == bgColor()) continue;
        ++result.components;
        int head = 0, tail = 0;
        queue[tail++] = start;
        seen[start] = 1;
        while (head < tail) {
            int pos = queue[head++], x = pos % size, y = pos / size;
            static const int dx[] = {-1, 1, 0, 0};
            static const int dy[] = {0, 0, -1, 1};
            for (int d = 0; d < 4; ++d) {
                int nx = x + dx[d], ny = y + dy[d];
                if (nx < 0 || ny < 0 || nx >= size || ny >= size) continue;
                int next = ny * size + nx;
                if (seen[next] || canvas.readPixel(nx, ny) == bgColor()) continue;
                seen[next] = 1;
                queue[tail++] = next;
            }
        }
    }
    return result;
}

static Raster render(Bot::Mood mood, uint32_t offsetMs, float speed = 1.0f,
                     float amount = 1.0f, bool reduced = false) {
    const int size = 200;
    gNow = 1000;
    M5Canvas canvas(size, size);
    canvas.setRecording(false);
    Bot bot;
    Bot::Style style;
    style.eyeColor = botux::rgb565(0xF8, 0x10, 0xB0);
    bot.begin(&canvas);
    bot.setStyle(style);
    bot.setMood(mood, 0);
    bot.setAnimationSpeed(speed);
    bot.setMotionAmount(amount);
    bot.setReducedMotion(reduced);
    gNow += offsetMs;
    bot.update(gNow);
    bot.draw();
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            assert(canvas.readPixel(x, y) != style.eyeColor);
    return inspect(canvas, size);
}

static void renderAtSize(M5Canvas& canvas, Bot::Mood mood, uint32_t offsetMs,
                         int size, float amount = 1.0f,
                         Bot::Animation animation = Bot::Animation::Auto,
                         Bot::Expression expression = Bot::Expression::Auto,
                         float speed = 1.0f, bool reduced = false) {
    gNow = 1000;
    canvas.setRecording(false);
    Bot bot;
    Bot::Style style;
    style.eyeColor = botux::rgb565(0xF8, 0x10, 0xB0);
    bot.begin(&canvas);
    bot.setStyle(style);
    bot.setMood(mood, 0);
    bot.setExpression(expression, 0);
    bot.setAnimation(animation);
    bot.setMotionAmount(amount);
    bot.setAnimationSpeed(speed);
    bot.setReducedMotion(reduced);
    bot.update(1000 + offsetMs);
    bot.draw();
    if (canvas.outOfBounds())
        std::cerr << "out of bounds mood=" << (int)mood << " size=" << size
                  << " time=" << offsetMs << " amount=" << amount
                  << " animation=" << (int)animation << " reduced=" << reduced << "\n";
    assert(!canvas.outOfBounds());
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            assert(canvas.readPixel(x, y) != style.eyeColor);
}

static void renderIdleAtSize(M5Canvas& canvas, int size) {
    gNow = 1000;
    canvas.setRecording(false);
    Bot bot;
    bot.begin(&canvas);
    bot.setMood(Bot::Mood::Idle, 0);
    bot.setAnimation(Bot::Animation::Calm);
    bot.setMotionAmount(0.0f);
    bot.update(gNow);
    bot.draw();
    assert(!canvas.outOfBounds());
    (void)size;
}

static void renderAdvancedAtSize(M5Canvas& canvas, Bot::Mood mood,
                                 uint32_t offsetMs, int size, float amount,
                                 Bot::Animation animation, bool reduced) {
    gNow = 1000;
    canvas.setRecording(false);
    Bot bot;
    bot.begin(&canvas);
    bot.setMood(mood, 0);
    bot.setAnimation(animation);
    bot.setMotionAmount(amount);
    bot.setReducedMotion(reduced);
    uint32_t time = 0;
    while (time + 16u < offsetMs) {
        time += 16u;
        bot.update(1000 + time);
    }
    bot.update(1000 + offsetMs);
    bot.draw();
    if (canvas.outOfBounds())
        std::cerr << "advanced out of bounds mood=" << (int)mood << " size=" << size
                  << " time=" << offsetMs << " amount=" << amount
                  << " animation=" << (int)animation << " reduced=" << reduced << "\n";
    assert(!canvas.outOfBounds());
    (void)size;
}

static int changedAtSize(Bot::Mood mood, int size, uint32_t firstMs,
                         uint32_t secondMs, float amount = 1.0f) {
    M5Canvas first(size, size), second(size, size);
    renderAtSize(first, mood, firstMs, size, amount);
    renderAtSize(second, mood, secondMs, size, amount);
    int changed = 0;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            changed += first.readPixel(x, y) != second.readPixel(x, y);
    return changed;
}

static void checkLoop(Bot::Mood mood, uint32_t period, int size) {
    M5Canvas start(size, size), end(size, size);
    renderAtSize(start, mood, 0, size);
    renderAtSize(end, mood, period, size);
    assert(inspect(start, size).hash == inspect(end, size).hash);

    int ordinaryMax = 0;
    for (int sample = 0; sample < 24; ++sample) {
        uint32_t time = (uint32_t)((uint64_t)period * sample / 24u);
        int changed = changedAtSize(mood, size, time, time + 16u);
        if (changed > ordinaryMax) ordinaryMax = changed;
    }
    int wrap = changedAtSize(mood, size, period - 8u, period + 8u);
    assert(ordinaryMax > 0);
    assert(wrap <= ordinaryMax * 5 / 4 + 8);

    // Vortex points fade at both poles, so re-entry stays populated without
    // a frame-wide flash or an empty shell around the exact phase wrap.
    if (mood == Bot::Mood::Working) {
        M5Canvas before(size, size), after(size, size);
        renderAtSize(before, mood, period - 32u, size);
        renderAtSize(after, mood, period + 32u, size);
        Raster beforeStats = inspect(before, size);
        Raster afterStats = inspect(after, size);
        assert(beforeStats.ink > size / 2 && afterStats.ink > size / 2);
    }
}

static void writeCanvasPpm(const std::string& path, const M5Canvas& canvas,
                           int size) {
    std::ofstream out(path.c_str(), std::ios::binary);
    assert(out);
    out << "P6\n" << size << " " << size << "\n255\n";
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            uint16_t c = canvas.readPixel(x, y);
            unsigned char rgb[3] = {
                (unsigned char)(((c >> 11) & 31u) * 255u / 31u),
                (unsigned char)(((c >> 5) & 63u) * 255u / 63u),
                (unsigned char)((c & 31u) * 255u / 31u)
            };
            out.write(reinterpret_cast<const char*>(rgb), 3);
        }
    }
}

static void writePpm(const std::string& path, Bot::Mood mood, uint32_t time,
                     int size = 200, float amount = 1.0f, float speed = 1.0f) {
    M5Canvas canvas(size, size);
    renderAtSize(canvas, mood, time, size, amount, Bot::Animation::Auto,
                 Bot::Expression::Auto, speed);
    writeCanvasPpm(path, canvas, size);
}

static void writeIdlePpm(const std::string& path, int size) {
    M5Canvas canvas(size, size);
    renderIdleAtSize(canvas, size);
    writeCanvasPpm(path, canvas, size);
}

static int changedPixels(Bot::Mood mood, uint32_t firstMs, uint32_t secondMs,
                         float amount, bool reduced, float speed = 1.0f) {
    const int size = 200;
    M5Canvas first(size, size), second(size, size);
    first.setRecording(false); second.setRecording(false);
    Bot a, b;
    gNow = 1000; a.begin(&first); b.begin(&second);
    a.setMood(mood, 0); b.setMood(mood, 0);
    a.setMotionAmount(amount); b.setMotionAmount(amount);
    a.setReducedMotion(reduced); b.setReducedMotion(reduced);
    a.setAnimationSpeed(speed); b.setAnimationSpeed(speed);
    a.update(1000 + firstMs); b.update(1000 + secondMs);
    a.draw(); b.draw();
    int changed = 0;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            changed += first.readPixel(x, y) != second.readPixel(x, y);
    return changed;
}

static void writeWatchLoop(const std::string& directory, const char* stem,
                           Bot::Mood mood, uint32_t basePeriodMs) {
    static const int kFrames = 16;
    static const float kWatchMotionAmount = 0.55f;
    static const float kWatchAnimationSpeed = 0.73f;
    float wallPeriod = basePeriodMs /
                       (kWatchMotionAmount * kWatchAnimationSpeed);
    for (int frame = 0; frame < kFrames; ++frame) {
        char suffix[32];
        snprintf(suffix, sizeof(suffix), "-%02d.ppm", frame);
        uint32_t time = (uint32_t)(wallPeriod * frame / kFrames + 0.5f);
        writePpm(directory + "/" + stem + suffix, mood, time, 120,
                 kWatchMotionAmount, kWatchAnimationSpeed);
    }
}

struct FootprintRange {
    int idleWidth, idleHeight;
    int minWidth, maxWidth, minHeight, maxHeight;
};

static FootprintRange measureWatchFootprint(Bot::Mood mood, int size,
                                            uint32_t basePeriodMs) {
    M5Canvas idleCanvas(size, size);
    renderIdleAtSize(idleCanvas, size);
    Raster idle = inspect(idleCanvas, size);
    FootprintRange range = {
        idle.right - idle.left + 1, idle.bottom - idle.top + 1,
        size, 0, size, 0
    };
    const float watchPhaseScale = 0.73f * 0.55f;
    for (int sample = 0; sample < 16; ++sample) {
        uint32_t time = (uint32_t)(basePeriodMs * sample /
                                   (16.0f * watchPhaseScale));
        M5Canvas frame(size, size);
        renderAtSize(frame, mood, time, size, 0.55f, Bot::Animation::Auto,
                     Bot::Expression::Auto, 0.73f);
        Raster stats = inspect(frame, size);
        int width = stats.right - stats.left + 1;
        int height = stats.bottom - stats.top + 1;
        if (width < range.minWidth) range.minWidth = width;
        if (width > range.maxWidth) range.maxWidth = width;
        if (height < range.minHeight) range.minHeight = height;
        if (height > range.maxHeight) range.maxHeight = height;
    }
    return range;
}

static void writeRasterSignatures(const std::string& path) {
    struct Profile {
        const char* name;
        float amount;
        float speed;
        bool reduced;
    };
    static const Profile profiles[] = {
        {"standard", 1.0f, 1.0f, false},
        {"watch", 0.55f, 0.73f, false},
        {"reduced", 1.0f, 1.0f, true},
        {"maximum", 2.0f, 1.0f, false}
    };
    static const int sizes[] = {40, 72, 120, 178, 286};
    static const Bot::Mood moods[] = {Bot::Mood::Thinking, Bot::Mood::Working};
    std::ofstream out(path.c_str());
    assert(out);
    for (size_t m = 0; m < sizeof(moods) / sizeof(moods[0]); ++m) {
        float basePeriod = moods[m] == Bot::Mood::Thinking ? 1600.0f : 1400.0f;
        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); ++s) {
            for (size_t p = 0; p < sizeof(profiles) / sizeof(profiles[0]); ++p) {
                float reducedScale = profiles[p].reduced ? 0.20f : 1.0f;
                float phaseScale = profiles[p].amount < 1.0f
                                 ? profiles[p].amount : 1.0f;
                float wallPeriod = basePeriod /
                                   (profiles[p].speed * phaseScale * reducedScale);
                for (int sample = 0; sample < 8; ++sample) {
                    uint32_t time = (uint32_t)(wallPeriod * sample / 8.0f + 0.5f);
                    M5Canvas frame(sizes[s], sizes[s]);
                    renderAtSize(frame, moods[m], time, sizes[s], profiles[p].amount,
                                 Bot::Animation::Auto, Bot::Expression::Auto,
                                 profiles[p].speed, profiles[p].reduced);
                    Raster stats = inspect(frame, sizes[s]);
                    out << (int)moods[m] << " " << sizes[s] << " "
                        << profiles[p].name << " " << sample << " " << stats.hash
                        << " " << stats.ink << " " << stats.components << " "
                        << stats.left << " " << stats.top << " " << stats.right
                        << " " << stats.bottom << "\n";
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    Raster thinking = render(Bot::Mood::Thinking, 0);
    Raster working = render(Bot::Mood::Working, 0);
    assert(thinking.ink > 1800 && thinking.ink < 6000);
    assert(working.ink > 1600 && working.ink < 6000);
    assert(thinking.components > 45);
    assert(working.components > 40);
    assert(thinking.right - thinking.left > 130 && thinking.bottom - thinking.top > 130);
    assert(working.right - working.left > 130 && working.bottom - working.top > 130);
    assert(thinking.hash != working.hash);
    assert(thinking.ink / thinking.components > 20);
    assert(working.ink / working.components > 20);
    int workingBoxArea = (working.right - working.left + 1) *
                         (working.bottom - working.top + 1);
    assert(working.ink * 5 < workingBoxArea);

    // Speed remains an exact scalar of phase for both reference loops.
    assert(render(Bot::Mood::Thinking, 400, 1.0f).hash ==
           render(Bot::Mood::Thinking, 200, 2.0f).hash);
    assert(render(Bot::Mood::Working, 350, 1.0f).hash ==
           render(Bot::Mood::Working, 175, 2.0f).hash);

    // Zero freezes every point. Small amounts and reduced motion both calm the
    // entire choreography instead of leaving a full-speed shell rotation.
    assert(changedPixels(Bot::Mood::Thinking, 0, 1237, 0.0f, false) == 0);
    assert(changedPixels(Bot::Mood::Working, 0, 1237, 0.0f, false) == 0);
    int thinkingFull = changedPixels(Bot::Mood::Thinking, 0, 40, 1.0f, false);
    int thinkingLow = changedPixels(Bot::Mood::Thinking, 0, 40, 0.10f, false);
    int thinkingReduced = changedPixels(Bot::Mood::Thinking, 0, 40, 1.0f, true);
    int workingFull = changedPixels(Bot::Mood::Working, 0, 40, 1.0f, false);
    int workingLow = changedPixels(Bot::Mood::Working, 0, 40, 0.10f, false);
    int workingReduced = changedPixels(Bot::Mood::Working, 0, 40, 1.0f, true);
    assert(thinkingFull > thinkingReduced && thinkingReduced > thinkingLow);
    assert(workingFull > workingReduced && workingReduced > workingLow);
    assert(thinkingLow > 0 && workingLow > 0);

    // The Watch's default controls (speed 0.73, amount 0.55) remain visibly
    // active while progressing more calmly than the renderer's 1.0 profile.
    int thinkingWatch = changedPixels(Bot::Mood::Thinking, 0, 40,
                                      0.55f, false, 0.73f);
    int workingWatch = changedPixels(Bot::Mood::Working, 0, 40,
                                     0.55f, false, 0.73f);
    assert(thinkingWatch > 0 && thinkingWatch < thinkingFull);
    assert(workingWatch > 0 && workingWatch < workingFull);

    M5Canvas thinkingWatchCanvas(286, 286), workingWatchCanvas(286, 286);
    renderAtSize(thinkingWatchCanvas, Bot::Mood::Thinking, 1000, 286, 0.55f,
                 Bot::Animation::Auto, Bot::Expression::Auto, 0.73f);
    renderAtSize(workingWatchCanvas, Bot::Mood::Working, 1000, 286, 0.55f,
                 Bot::Animation::Auto, Bot::Expression::Auto, 0.73f);
    Raster thinkingWatch286 = inspect(thinkingWatchCanvas, 286);
    Raster workingWatch286 = inspect(workingWatchCanvas, 286);
    assert(thinkingWatch286.components > 0);
    assert(workingWatch286.components > 0);
    // Fewer components than emitted discs confirms the dense frames exercise
    // touching/overlapping compositing, not only flat-background interiors.
    assert(thinkingWatch286.components < 96);
    assert(workingWatch286.components < 96);
    assert(thinkingWatch286.ink / thinkingWatch286.components > 40);
    assert(workingWatch286.ink / workingWatch286.components > 40);
    int workingWatchBoxArea = (workingWatch286.right - workingWatch286.left + 1) *
                              (workingWatch286.bottom - workingWatch286.top + 1);
    assert(workingWatch286.ink * 100 > workingWatchBoxArea * 9);
    assert(workingWatch286.ink * 5 < workingWatchBoxArea);

    M5Canvas idleCanvas(286, 286);
    renderIdleAtSize(idleCanvas, 286);
    Raster idle286 = inspect(idleCanvas, 286);
    int idleWidth = idle286.right - idle286.left + 1;
    int idleHeight = idle286.bottom - idle286.top + 1;
    int thinkingMinWidth = 286, thinkingMaxWidth = 0;
    int thinkingMinHeight = 286, thinkingMaxHeight = 0;
    int workingMinWidth = 286, workingMaxWidth = 0;
    int workingMinHeight = 286, workingMaxHeight = 0;
    for (int sample = 0; sample < 16; ++sample) {
        const float watchPhaseScale = 0.73f * 0.55f;
        uint32_t thinkingTime = (uint32_t)(1600.0f * sample /
                                           (16.0f * watchPhaseScale));
        uint32_t workingTime = (uint32_t)(1400.0f * sample /
                                          (16.0f * watchPhaseScale));
        M5Canvas thinkingFrame(286, 286), workingFrame(286, 286);
        renderAtSize(thinkingFrame, Bot::Mood::Thinking, thinkingTime, 286,
                     0.55f, Bot::Animation::Auto, Bot::Expression::Auto, 0.73f);
        renderAtSize(workingFrame, Bot::Mood::Working, workingTime, 286,
                     0.55f, Bot::Animation::Auto, Bot::Expression::Auto, 0.73f);
        Raster thinkingFrameStats = inspect(thinkingFrame, 286);
        Raster workingFrameStats = inspect(workingFrame, 286);
        int tw = thinkingFrameStats.right - thinkingFrameStats.left + 1;
        int th = thinkingFrameStats.bottom - thinkingFrameStats.top + 1;
        int ww = workingFrameStats.right - workingFrameStats.left + 1;
        int wh = workingFrameStats.bottom - workingFrameStats.top + 1;
        if (tw < thinkingMinWidth) thinkingMinWidth = tw;
        if (tw > thinkingMaxWidth) thinkingMaxWidth = tw;
        if (th < thinkingMinHeight) thinkingMinHeight = th;
        if (th > thinkingMaxHeight) thinkingMaxHeight = th;
        if (ww < workingMinWidth) workingMinWidth = ww;
        if (ww > workingMaxWidth) workingMaxWidth = ww;
        if (wh < workingMinHeight) workingMinHeight = wh;
        if (wh > workingMaxHeight) workingMaxHeight = wh;
        assert(tw * 100 >= idleWidth * 90 && tw * 100 <= idleWidth * 110);
        assert(th * 100 >= idleHeight * 90 && th * 100 <= idleHeight * 110);
        assert(ww * 100 >= idleWidth * 90 && ww * 100 <= idleWidth * 110);
        assert(wh * 100 >= idleHeight * 90 && wh * 100 <= idleHeight * 110);
    }
    FootprintRange thinking40 = measureWatchFootprint(Bot::Mood::Thinking, 40, 1600);
    FootprintRange working40 = measureWatchFootprint(Bot::Mood::Working, 40, 1400);
    FootprintRange thinking72 = measureWatchFootprint(Bot::Mood::Thinking, 72, 1600);
    FootprintRange working72 = measureWatchFootprint(Bot::Mood::Working, 72, 1400);
    assert(thinking40.minWidth * 100 >= thinking40.idleWidth * 75);
    assert(thinking40.maxWidth * 100 <= thinking40.idleWidth * 115);
    assert(working40.minWidth * 100 >= working40.idleWidth * 75);
    assert(working40.maxWidth * 100 <= working40.idleWidth * 115);
    assert(thinking72.minWidth * 100 >= thinking72.idleWidth * 75);
    assert(thinking72.maxWidth * 100 <= thinking72.idleWidth * 115);
    assert(working72.minWidth * 100 >= working72.idleWidth * 75);
    assert(working72.maxWidth * 100 <= working72.idleWidth * 115);
    assert(thinking40.minHeight * 100 >= thinking40.idleHeight * 75);
    assert(thinking40.maxHeight * 100 <= thinking40.idleHeight * 115);
    assert(working40.minHeight * 100 >= working40.idleHeight * 75);
    assert(working40.maxHeight * 100 <= working40.idleHeight * 115);
    assert(thinking72.minHeight * 100 >= thinking72.idleHeight * 75);
    assert(thinking72.maxHeight * 100 <= thinking72.idleHeight * 115);
    assert(working72.minHeight * 100 >= working72.idleHeight * 75);
    assert(working72.maxHeight * 100 <= working72.idleHeight * 115);
    static const int sizes[] = {40, 72, 178, 286};
    static const Bot::Animation animations[] = {
        Bot::Animation::Auto, Bot::Animation::Calm,
        Bot::Animation::Curious, Bot::Animation::Orbit,
        Bot::Animation::Bounce, Bot::Animation::Glitch,
        Bot::Animation::Wave, Bot::Animation::Sparkle
    };
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        checkLoop(Bot::Mood::Thinking, 1600, sizes[i]);
        checkLoop(Bot::Mood::Working, 1400, sizes[i]);
        M5Canvas thinkingZero(sizes[i], sizes[i]);
        M5Canvas workingZero(sizes[i], sizes[i]);
        renderAdvancedAtSize(thinkingZero, Bot::Mood::Thinking, 1237,
                             sizes[i], 0.0f, Bot::Animation::Auto, false);
        renderAdvancedAtSize(workingZero, Bot::Mood::Working, 1237,
                             sizes[i], 0.0f, Bot::Animation::Auto, false);
        for (int sample = 0; sample < 12; ++sample) {
            M5Canvas thinkingReducedCanvas(sizes[i], sizes[i]);
            M5Canvas workingReducedCanvas(sizes[i], sizes[i]);
            uint32_t thinkingTime = 1600u * sample / 12u;
            uint32_t workingTime = 1400u * sample / 12u;
            renderAdvancedAtSize(thinkingReducedCanvas, Bot::Mood::Thinking,
                                 thinkingTime, sizes[i], 1.0f,
                                 Bot::Animation::Auto, true);
            renderAdvancedAtSize(workingReducedCanvas, Bot::Mood::Working,
                                 workingTime, sizes[i], 1.0f,
                                 Bot::Animation::Auto, true);
            for (size_t a = 0; a < sizeof(animations) / sizeof(animations[0]); ++a) {
                M5Canvas thinkingMax(sizes[i], sizes[i]);
                M5Canvas workingMax(sizes[i], sizes[i]);
                renderAdvancedAtSize(thinkingMax, Bot::Mood::Thinking,
                                     thinkingTime, sizes[i], 2.0f,
                                     animations[a], false);
                renderAdvancedAtSize(workingMax, Bot::Mood::Working,
                                     workingTime, sizes[i], 2.0f,
                                     animations[a], false);
            }
        }
    }

    // Explicit expressions cannot restore eyes over either shell. Explicit
    // animations retain their documented whole-character translation layer.
    M5Canvas thinkingAuto(178, 178), thinkingAlarmed(178, 178);
    renderAtSize(thinkingAuto, Bot::Mood::Thinking, 700, 178);
    renderAtSize(thinkingAlarmed, Bot::Mood::Thinking, 700, 178, 1.0f,
                 Bot::Animation::Auto, Bot::Expression::Alarmed);
    assert(inspect(thinkingAuto, 178).hash == inspect(thinkingAlarmed, 178).hash);
    M5Canvas workingAuto(178, 178), workingCurious(178, 178);
    renderAtSize(workingAuto, Bot::Mood::Working, 700, 178);
    renderAtSize(workingCurious, Bot::Mood::Working, 700, 178, 1.0f,
                 Bot::Animation::Curious);
    assert(inspect(workingAuto, 178).hash != inspect(workingCurious, 178).hash);

    if (argc > 1) {
        std::string directory = argv[1];
        writePpm(directory + "/thinking-orbs.ppm", Bot::Mood::Thinking, 700);
        writePpm(directory + "/working-vortex.ppm", Bot::Mood::Working, 700);
        writePpm(directory + "/thinking-orbs-286.ppm", Bot::Mood::Thinking, 700, 286);
        writePpm(directory + "/working-vortex-286.ppm", Bot::Mood::Working, 700, 286);
        writePpm(directory + "/watch-thinking-120.ppm", Bot::Mood::Thinking,
                 1000, 120, 0.55f, 0.73f);
        writePpm(directory + "/watch-working-120.ppm", Bot::Mood::Working,
                 1000, 120, 0.55f, 0.73f);
        writePpm(directory + "/watch-thinking-286.ppm", Bot::Mood::Thinking,
                 1000, 286, 0.55f, 0.73f);
        writePpm(directory + "/watch-working-286.ppm", Bot::Mood::Working,
                 1000, 286, 0.55f, 0.73f);
        writeIdlePpm(directory + "/idle-286.ppm", 286);
        writeWatchLoop(directory, "watch-thinking-120", Bot::Mood::Thinking, 1600);
        writeWatchLoop(directory, "watch-working-120", Bot::Mood::Working, 1400);
        writeRasterSignatures(directory + "/orb-raster-signatures.txt");
    }

    std::cout << "Orb motion: Thinking " << thinking.ink << " px/"
              << thinking.components << " clusters, Working " << working.ink
              << " px/" << working.components << " clusters; 40ms deltas full/reduced/0.1 = "
              << thinkingFull << "/" << thinkingReduced << "/" << thinkingLow << " and "
              << workingFull << "/" << workingReduced << "/" << workingLow
              << "; Watch-default deltas " << thinkingWatch << "/"
              << workingWatch << "; 286px Watch ink/clusters "
              << thinkingWatch286.ink << "/" << thinkingWatch286.components
              << " and " << workingWatch286.ink << "/"
              << workingWatch286.components << "; footprint ranges T "
              << thinkingMinWidth << "-" << thinkingMaxWidth << "x"
              << thinkingMinHeight << "-" << thinkingMaxHeight << ", W "
              << workingMinWidth << "-" << workingMaxWidth << "x"
              << workingMinHeight << "-" << workingMaxHeight << ", Idle "
              << idleWidth << "x" << idleHeight << "; 40px T/W widths "
              << thinking40.minWidth << "-" << thinking40.maxWidth << "x"
              << thinking40.minHeight << "-" << thinking40.maxHeight << "/"
              << working40.minWidth << "-" << working40.maxWidth << "x"
              << working40.minHeight << "-" << working40.maxHeight << " vs Idle "
              << thinking40.idleWidth << "x" << thinking40.idleHeight
              << "; 72px T/W widths " << thinking72.minWidth << "-"
              << thinking72.maxWidth << "x" << thinking72.minHeight << "-"
              << thinking72.maxHeight << "/" << working72.minWidth << "-"
              << working72.maxWidth << "x" << working72.minHeight << "-"
              << working72.maxHeight << " vs Idle " << thinking72.idleWidth
              << "x" << thinking72.idleHeight << "\n";
}

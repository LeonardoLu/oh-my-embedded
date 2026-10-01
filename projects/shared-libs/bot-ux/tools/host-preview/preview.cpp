#include <BotUx.h>

#include <assert.h>
#include <cstring>
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

static uint32_t gNow = 1000;
uint32_t millis() { return gNow; }

struct MoodCase {
    botux::BotUx::Mood mood;
    const char* label;
    bool talking;
};

static const MoodCase kMoods[] = {
    {botux::BotUx::Mood::Idle, "Idle", false},
    {botux::BotUx::Mood::Listening, "Listening", false},
    {botux::BotUx::Mood::Thinking, "Thinking", false},
    {botux::BotUx::Mood::Speaking, "Speaking", true},
    {botux::BotUx::Mood::Happy, "Happy", false},
    {botux::BotUx::Mood::Sad, "Sad", false},
    {botux::BotUx::Mood::Sleepy, "Sleepy", false},
    {botux::BotUx::Mood::Surprised, "Surprised", false},
    {botux::BotUx::Mood::Working, "Working", false},
    {botux::BotUx::Mood::Waiting, "Waiting", false},
    {botux::BotUx::Mood::Blocked, "Blocked", false},
    {botux::BotUx::Mood::Done, "Done", false},
    {botux::BotUx::Mood::Asleep, "Asleep", false},
    {botux::BotUx::Mood::LookingAround, "Looking around", false},
};

struct ExpressionCase {
    botux::BotUx::Expression expression;
    const char* label;
};

static const ExpressionCase kExpressions[] = {
    {botux::BotUx::Expression::Neutral, "Neutral"},
    {botux::BotUx::Expression::Curious, "Curious"},
    {botux::BotUx::Expression::Focused, "Focused"},
    {botux::BotUx::Expression::Joy, "Joy"},
    {botux::BotUx::Expression::Skeptical, "Skeptical"},
    {botux::BotUx::Expression::Bashful, "Bashful"},
    {botux::BotUx::Expression::Wink, "Wink"},
    {botux::BotUx::Expression::Dizzy, "Dizzy"},
    {botux::BotUx::Expression::Alarmed, "Alarmed"},
};

struct AnimationCase {
    botux::BotUx::Animation animation;
    const char* label;
};

static const AnimationCase kAnimations[] = {
    {botux::BotUx::Animation::Calm, "Calm"},
    {botux::BotUx::Animation::Curious, "Curious"},
    {botux::BotUx::Animation::Orbit, "Orbit"},
    {botux::BotUx::Animation::Bounce, "Bounce"},
    {botux::BotUx::Animation::Glitch, "Glitch"},
    {botux::BotUx::Animation::Wave, "Wave"},
    {botux::BotUx::Animation::Sparkle, "Sparkle"},
};

static botux::BotUx::Style watchThemeStyle(bool mono) {
    botux::BotUx::Style style;
    if (!mono) {
        style.bgColor = botux::rgb565(0x08,0x0B,0x10);
        style.bodyColor = botux::rgb565(0xF2,0xF0,0xE8);
        style.accentColor = botux::rgb565(0x55,0x8D,0xFF);
        style.eyeColor = style.pupilColor = style.mouthColor = botux::rgb565(0x19,0x22,0x32);
        style.blushColor = botux::rgb565(0xFA,0x9A,0xA8);
    } else {
        style.bgColor = botux::rgb565(0x0C,0x0D,0x10);
        style.bodyColor = botux::rgb565(0xEC,0xEC,0xE8);
        style.accentColor = botux::rgb565(0xC7,0xCF,0xDC);
        style.eyeColor = style.pupilColor = style.mouthColor = botux::rgb565(0x19,0x1B,0x20);
        style.blushColor = botux::rgb565(0x9A,0x9A,0x9A);
    }
    style.blinkMinMs = style.blinkMaxMs = 600000;
    return style;
}

static uint16_t mix565ForTest(uint16_t a, uint16_t b, uint8_t amount) {
    uint16_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    uint16_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    uint16_t inv = 255u - amount;
    return (uint16_t)((((ar * inv + br * amount) / 255u) << 11) |
                      (((ag * inv + bg * amount) / 255u) << 5) |
                       ((ab * inv + bb * amount) / 255u));
}

static size_t countToken(const std::string& value, const char* token) {
    size_t count = 0, pos = 0;
    while ((pos = value.find(token, pos)) != std::string::npos) {
        ++count;
        pos += 1;
    }
    return count;
}

static std::string render(const MoodCase& item, int size, int index) {
    gNow = 1000;
    M5Canvas canvas((int16_t)size, (int16_t)size);
    botux::BotUx bot;
    bot.seedBlink(0x123400u + (uint32_t)index);
    bot.begin(&canvas);
    bot.setMood(item.mood);
    bot.setTalking(item.talking);

    // Exercise the same time-based update path used on device and let pose easing settle.
    for (int frame = 0; frame < 100; ++frame) {
        gNow = 1000u + (uint32_t)frame * 16u;
        bot.update(gNow);
    }
    bot.draw();
    assert(bot.metrics().cx == size / 2);
    assert(bot.metrics().bodyR > 0);
    std::string body = canvas.svgBody();
    assert(!body.empty());
    return body;
}

static bool writeSheet(const char* path, int size) {
    const int columns = (size <= 72) ? 6 : 4;
    const int rows = ((int)(sizeof(kMoods) / sizeof(kMoods[0])) + columns - 1) / columns;
    const int labelH = (size <= 72) ? 16 : 22;
    std::ofstream out(path);
    if (!out) return false;
    out << "<svg xmlns='http://www.w3.org/2000/svg' width='" << columns * size
        << "' height='" << rows * (size + labelH) << "' viewBox='0 0 " << columns * size
        << " " << rows * (size + labelH) << "'>\n";
    out << "<rect width='100%' height='100%' fill='#11151D'/>\n";
    for (int i = 0; i < (int)(sizeof(kMoods) / sizeof(kMoods[0])); ++i) {
        int x = (i % columns) * size;
        int y = (i / columns) * (size + labelH);
        out << "<g transform='translate(" << x << " " << y << ")'>\n"
            << render(kMoods[i], size, i) << "</g>\n";
        out << "<text x='" << x + size / 2 << "' y='" << y + size + labelH * 2 / 3
            << "' text-anchor='middle' font-family='sans-serif' font-size='"
            << ((size <= 72) ? 9 : 13) << "' fill='#D7DFEA'>" << kMoods[i].label << "</text>\n";
    }
    out << "</svg>\n";
    return true;
}

static std::string renderExpression(const ExpressionCase& item, int size) {
    gNow = 1000;
    M5Canvas canvas((int16_t)size, (int16_t)size);
    botux::BotUx bot;
    bot.begin(&canvas);
    bot.setAnimation(botux::BotUx::Animation::Calm);
    bot.setMotionAmount(0.0f);
    bot.setExpression(item.expression, 0);
    bot.update(gNow);
    bot.draw();
    return canvas.svgBody();
}

static std::string renderAnimation(const AnimationCase& item, int size,
                                   bool reduced = false) {
    gNow = 1000;
    M5Canvas canvas((int16_t)size, (int16_t)size);
    botux::BotUx bot;
    bot.begin(&canvas);
    bot.setExpression(botux::BotUx::Expression::Neutral, 0);
    bot.setAnimation(item.animation);
    bot.setAnimationSpeed(1.25f);
    bot.setReducedMotion(reduced);
    bot.update(2473);
    bot.draw();
    return canvas.svgBody();
}

template <typename Case, size_t N, typename RenderFn>
static bool writePickerSheet(const char* path, const Case (&items)[N], RenderFn renderItem, int size = 120) {
    const int columns = 3, labelH = 20;
    const int cellWidth = size < 90 ? 90 : size;
    const int rows = ((int)N + columns - 1) / columns;
    std::ofstream out(path);
    if (!out) return false;
    out << "<svg xmlns='http://www.w3.org/2000/svg' width='" << columns * cellWidth
        << "' height='" << rows * (size + labelH) << "'>\n"
        << "<rect width='100%' height='100%' fill='#11151D'/>\n";
    for (int i = 0; i < (int)N; ++i) {
        int x = (i % columns) * cellWidth + (cellWidth - size) / 2;
        int y = (i / columns) * (size + labelH);
        out << "<g transform='translate(" << x << " " << y << ")'>\n"
            << renderItem(items[i], size) << "</g>\n"
            << "<text x='" << x + size / 2 << "' y='" << y + size + 14
            << "' text-anchor='middle' font-family='sans-serif' font-size='12' fill='#D7DFEA'>"
            << items[i].label << "</text>\n";
    }
    out << "</svg>\n";
    return true;
}

static void writeJsString(std::ofstream& out, const std::string& value) {
    out << '"';
    for (char c : value) {
        switch (c) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': break;
            default: out << c; break;
        }
    }
    out << '"';
}

static bool writeAnimationPlayer(const char* path) {
    std::ofstream out(path);
    if (!out) return false;
    out << "<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width'>"
        << "<title>BotUx source-rendered animation preview</title>"
        << "<style>body{margin:0;background:#11151d;color:#d7dfea;font:16px system-ui;display:grid;"
        << "place-items:center;min-height:100vh}.card{width:min(88vw,520px);display:grid;gap:14px}"
        << "#stage{aspect-ratio:1;border:1px solid #29313d;border-radius:20px;overflow:hidden}"
        << "#stage svg{width:100%;height:100%;display:block}.row{display:flex;gap:10px;align-items:center}"
        << "select,button,input{font:inherit}select{flex:1;padding:8px}input{width:100%}</style>"
        << "<main class='card'><h1>BotUx actual-source preview</h1><div id='stage'></div>"
        << "<div class='row'><select id='mode'></select><button id='play'>Pause</button></div>"
        << "<input id='scrub' type='range' min='0' max='59' value='0'>"
        << "<small>60 frames per mode at 15 fps, generated by BotUx.cpp. "
        << "The browser only plays the captured M5GFX primitive calls.</small></main><script>\n"
        << "const modes=[\n";

    const int modeCount = 10;
    for (int mode = 0; mode < modeCount; ++mode) {
        const char* name = (mode == 0) ? "Auto lifecycle"
                         : (mode <= 7) ? kAnimations[mode - 1].label
                         : (mode == 8) ? "Expression transitions" : "IMU tilt + shake";
        out << "{name:";
        writeJsString(out, name);
        out << ",frames:[\n";

        gNow = 1000;
        M5Canvas canvas(200, 200);
        botux::BotUx bot;
        bot.begin(&canvas);
        bot.setAnimationSpeed(1.25f);
        if (mode == 0) {
            bot.setAnimation(botux::BotUx::Animation::Auto);
        } else if (mode <= 7) {
            bot.setExpression(botux::BotUx::Expression::Neutral, 0);
            bot.setAnimation(kAnimations[mode - 1].animation);
        } else {
            bot.setAnimation(botux::BotUx::Animation::Calm);
        }

        std::set<std::string> uniqueFrames;
        for (int frame = 0; frame < 60; ++frame) {
            gNow = 1000u + (uint32_t)frame * 67u;
            if (mode == 0 && frame % 10 == 0) {
                static const botux::BotUx::Mood lifecycle[] = {
                    botux::BotUx::Mood::Idle, botux::BotUx::Mood::Listening,
                    botux::BotUx::Mood::Working, botux::BotUx::Mood::Waiting,
                    botux::BotUx::Mood::Blocked, botux::BotUx::Mood::Done,
                };
                bot.setMood(lifecycle[frame / 10], 180);
            } else if (mode == 8) {
                if (frame == 0) bot.setExpression(botux::BotUx::Expression::Neutral, 0);
                else if (frame == 15) bot.setExpression(botux::BotUx::Expression::Skeptical, 500);
                else if (frame == 30) bot.setExpression(botux::BotUx::Expression::Wink, 500);
                else if (frame == 45) bot.setExpression(botux::BotUx::Expression::Joy, 500);
            } else if (mode == 9) {
                float tiltX = sinf(frame * 0.14f);
                float tiltY = cosf(frame * 0.11f) * 0.75f;
                float shake = (frame >= 26 && frame <= 33) ? 0.85f : 0.0f;
                bot.setMotion(tiltX, tiltY, shake);
            }
            canvas.clear();
            bot.update(gNow);
            bot.draw();
            std::string frameSvg = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 200 200'>\n";
            frameSvg += canvas.svgBody();
            frameSvg += "</svg>";
            uniqueFrames.insert(frameSvg);
            writeJsString(out, frameSvg);
            out << ((frame == 59) ? "\n" : ",\n");
        }
        assert(uniqueFrames.size() >= 8);
        out << "]}" << ((mode == modeCount - 1) ? "\n" : ",\n");
    }

    out << "];const mode=document.querySelector('#mode'),stage=document.querySelector('#stage'),"
        << "scrub=document.querySelector('#scrub'),play=document.querySelector('#play');"
        << "modes.forEach((m,i)=>mode.add(new Option(m.name,i)));let frame=0,running=true;"
        << "function draw(){stage.innerHTML=modes[+mode.value].frames[frame];scrub.value=frame}"
        << "mode.onchange=()=>{frame=0;draw()};scrub.oninput=()=>{frame=+scrub.value;draw()};"
        << "play.onclick=()=>{running=!running;play.textContent=running?'Pause':'Play'};"
        << "setInterval(()=>{if(running){frame=(frame+1)%60;draw()}},67);draw();</script>\n";
    return true;
}

static void checkExpressionAndAnimationRange() {
    std::set<std::string> expressions;
    for (const auto& item : kExpressions) expressions.insert(renderExpression(item, 200));
    assert(expressions.size() == sizeof(kExpressions) / sizeof(kExpressions[0]));

    std::set<std::string> animations;
    for (const auto& item : kAnimations) {
        const std::string svg = renderAnimation(item, 200);
        animations.insert(svg);
        assert(countToken(svg, "<") <= 2200); // perimeter coverage stays bounded
    }
    assert(animations.size() == sizeof(kAnimations) / sizeof(kAnimations[0]));
}

static void checkTransitionsMotionAndSparseFrames() {
    M5Canvas canvas(200, 200);
    botux::BotUx bot;
    gNow = 1000;
    bot.begin(&canvas);
    bot.setAnimation(botux::BotUx::Animation::Calm);
    bot.setAnimationSpeed(99.0f);
    assert(bot.animationSpeed() == 3.0f);
    bot.setAnimationSpeed(-1.0f);
    assert(bot.animationSpeed() == 0.25f);
    bot.setAnimationSpeed(1.0f);
    bot.setMotionAmount(99.0f);
    assert(bot.motionAmount() == 2.0f);
    bot.setMotionAmount(1.0f);
    bot.setExpression(botux::BotUx::Expression::Neutral, 0);
    bot.update(gNow);
    bot.draw();
    const std::string neutral = canvas.svgBody();

    canvas.clear();
    bot.setExpression(botux::BotUx::Expression::Skeptical, 600);
    bot.update(1016);
    bot.draw();
    const std::string transition = canvas.svgBody();
    canvas.clear();
    bot.update(3016);
    bot.draw();
    const std::string settled = canvas.svgBody();
    assert(neutral != transition && transition != settled && neutral != settled);

    canvas.clear();
    bot.setMotion(1.0f, -0.8f, 0.7f);
    bot.update(3316); // 3 fps sensor/render loop still converges in real time.
    bot.draw();
    const std::string moved = canvas.svgBody();
    assert(moved != settled);
    assert(moved.find("nan") == std::string::npos);

    const AnimationCase bounce = {botux::BotUx::Animation::Bounce, "Bounce"};
    assert(renderAnimation(bounce, 200, false) != renderAnimation(bounce, 200, true));

    // A very late frame must finish an overdue blink rather than holding a
    // stale intermediate phase until several future frames arrive.
    canvas.clear();
    bot.setExpression(botux::BotUx::Expression::Neutral, 0);
    bot.setMotion(0.0f, 0.0f, 0.0f);
    bot.update(10000);
    bot.update(11000);
    bot.draw();
    assert(countToken(canvas.svgBody(), "<rect") > 30);
}

static void checkMotionStaysInCanvas() {
    for (int size : {40, 72, 200, 310}) {
        for (const auto& item : kAnimations) {
            gNow = 1000;
            M5Canvas canvas((int16_t)size, (int16_t)size);
            botux::BotUx bot;
            bot.begin(&canvas);
            bot.setExpression(botux::BotUx::Expression::Curious, 0);
            bot.setAnimation(item.animation);
            for (int frame = 0; frame < 60; ++frame) {
                float direction = (frame < 30) ? 1.0f : -1.0f;
                bot.setMotion(direction, -direction, 1.0f);
                gNow = 1000u + (uint32_t)frame * 67u;
                canvas.clear();
                bot.update(gNow);
                bot.draw();
                assert(!canvas.outOfBounds());
            }
        }
    }
}

static void checkBodylessStateGlyphs() {
    gNow = 1000;
    M5Canvas canvas(72, 72);
    botux::BotUx::Style style;
    style.bodyStyle = botux::BotUx::BodyStyle::None;
    botux::BotUx bot;
    bot.setStyle(style);
    bot.begin(&canvas);

    bot.setMood(botux::BotUx::Mood::Thinking);
    bot.update(3000);
    bot.draw();
    std::set<uint16_t> coverageColors;
    int lit = 0;
    for (int y = 0; y < 72; ++y) for (int x = 0; x < 72; ++x) {
        uint16_t c = canvas.readPixel(x,y);
        if (c != style.bgColor) { ++lit; coverageColors.insert(c); }
    }
    assert(lit > 150 && lit < 500);
    assert(coverageColors.size() > 12); // three opaque dot colors plus native AA fringes
    assert(canvas.svgBody().find("<circle") == std::string::npos);

    canvas.clear();
    bot.setMood(botux::BotUx::Mood::Blocked);
    bot.update(3200);
    bot.draw();
    const std::string blocked = canvas.svgBody();
    assert(blocked.find("<circle") == std::string::npos);
    assert(canvas.readPixel(36, 36) != style.bgColor);
    assert(blocked.find("rx='") != std::string::npos);
}

static void checkLateUptimeHasNoPhantomReaction() {
    gNow = 1000;
    M5Canvas canvas(200, 200);
    botux::BotUx bot;
    bot.begin(&canvas);
    bot.update(0x80001000u);
    bot.draw();
    // Idle pill eyes use capsule triangles; a phantom Surprised state uses circles.
    assert(countToken(canvas.svgBody(), "<rect") > 30);
}

static void checkWaitingAndSleepyStayDistinct() {
    const MoodCase sleepy = {botux::BotUx::Mood::Sleepy, "Sleepy", false};
    const MoodCase waiting = {botux::BotUx::Mood::Waiting, "Waiting", false};
    for (int size : {40, 72}) {
        const std::string sleepySvg = render(sleepy, size, 0);
        const std::string waitingSvg = render(waiting, size, 0);
        const size_t sleepyBody = sleepySvg.find("<rect", 1);
        const size_t waitingBody = waitingSvg.find("<rect", 1);
        const size_t sleepyBodyEnd = sleepySvg.find("/>", sleepyBody);
        const size_t waitingBodyEnd = waitingSvg.find("/>", waitingBody);
        assert(sleepyBodyEnd != std::string::npos && waitingBodyEnd != std::string::npos);
        assert(sleepySvg.substr(sleepyBodyEnd) != waitingSvg.substr(waitingBodyEnd));
    }
}

static void checkCoverageAndFacePlacement() {
    for (int size : {40, 72, 200, 310}) {
        for (const auto& item : kExpressions) {
            gNow = 1000;
            M5Canvas canvas(size, size);
            botux::BotUx bot;
            bot.begin(&canvas);
            bot.setAnimation(botux::BotUx::Animation::Calm);
            bot.setMotionAmount(0.0f);
            bot.setExpression(item.expression, 0);
            bot.update(gNow);
            bot.draw();
            float r = bot.metrics().bodyR;
            int minX = size, maxX = 0, minY = size, maxY = 0;
            std::set<uint16_t> colors;
            for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
                uint16_t pixel = canvas.readPixel(x, y);
                colors.insert(pixel);
                if (((pixel >> 11) & 31u) < 16u && fabsf(x - size * 0.5f) < r * 0.70f &&
                    fabsf(y - size * 0.5f) < r * 0.65f) {
                    minX = std::min(minX, x); maxX = std::max(maxX, x);
                    minY = std::min(minY, y); maxY = std::max(maxY, y);
                }
            }
            // Actual RGB565 boundary coverage, not browser smoothing of a path.
            assert(colors.size() > 12);
            assert(!canvas.outOfBounds());
            // Every face retains Neutral's upper-right placement and separated
            // pair scale; no expression throws the eyes to the lower hemisphere.
            assert(minX < maxX && minY <= maxY);
            assert((minX + maxX) * 0.5f > size * 0.5f);
            assert((minY + maxY) * 0.5f < size * 0.5f - r * 0.16f);
            assert(maxX - minX < r * 0.95f);
            assert(maxY - minY < r * 0.65f);
        }
    }
}

static void checkExplicitNeutralOverridesRestingMood() {
    for (int size : {40, 72, 200}) {
        for (auto mood : {botux::BotUx::Mood::Sleepy, botux::BotUx::Mood::Waiting, botux::BotUx::Mood::Asleep}) {
            int heights[2] = {};
            for (int selected = 0; selected < 2; ++selected) {
                gNow = 1000;
                M5Canvas canvas(size, size);
                botux::BotUx bot;
                bot.begin(&canvas);
                bot.setMotionAmount(0.0f);
                bot.setMood(mood, 0);
                if (selected) bot.setExpression(botux::BotUx::Expression::Neutral, 0);
                bot.update(gNow); bot.draw();
                int minY = size, maxY = -1;
                for (int y = 0; y < size; ++y)
                    for (int x = 0; x < size; ++x)
                        if (canvas.readPixel(x, y) == bot.style().eyeColor &&
                            (x-size/2)*(x-size/2)+(y-size/2)*(y-size/2) < bot.metrics().bodyR*bot.metrics().bodyR*.70f) {
                            minY = std::min(minY, y); maxY = std::max(maxY, y);
                        }
                heights[selected] = maxY - minY + 1;
            }
            // Auto retains the resting marks; explicit Neutral reopens the face,
            // including the tiny toolbar renderer used by Core2 settings.
            assert(heights[1] >= heights[0] + (size <= 72 ? 1 : 2));
        }
    }
}

static bool writeTransitionSheet(const char* path) {
    std::ofstream out(path);
    if (!out) return false;
    out << "<svg xmlns='http://www.w3.org/2000/svg' width='960' height='280'>";
    for (int row = 0; row < 2; ++row) {
        gNow = 1000;
        M5Canvas canvas(120, 120);
        botux::BotUx bot;
        bot.begin(&canvas);
        bot.setMotionAmount(0.0f);
        bot.setExpression(botux::BotUx::Expression::Neutral, 0);
        bot.update(gNow);
        bot.setExpression(row == 0 ? botux::BotUx::Expression::Joy :
                                    botux::BotUx::Expression::Wink, 650);
        for (int frame = 0; frame < 8; ++frame) {
            gNow += 100;
            bot.update(gNow);
            canvas.clear(); bot.draw();
            out << "<g transform='translate(" << frame * 120 << " " << row * 140 << ")'>"
                << canvas.svgBody() << "</g>";
        }
    }
    out << "</svg>";
    return true;
}

static void checkCompanionSemantics() {
    using Bot = botux::BotUx;
    Bot bot;
    assert(std::strcmp(bot.name(), "Milo") == 0);
    bot.setName("  Ava<script>你好 !  ");
    assert(std::strcmp(bot.name(), "Avascript") == 0);
    bot.setName("abcdefghijklmnopqrst"); assert(std::strlen(bot.name()) == Bot::kNameMax);
    bot.setName(nullptr); assert(std::strcmp(bot.name(), "Milo") == 0);
    for (uint8_t i = 0; i < Bot::presetCount(); ++i) {
        bot.applyPreset(i); const auto p = Bot::preset(i);
        assert(bot.mood() == p.mood && bot.expression() == p.expression && bot.animation() == p.animation);
        assert(bot.randomPreset() != i);
    }
    bot.resetToIdle(); assert(bot.mood() == Bot::Mood::Idle && bot.expression() == Bot::Expression::Auto);
    assert(bot.nextPreset() == 1);
    char desc[96]; bot.describe(desc, sizeof(desc), Bot::Language::Chinese);
    assert(std::strstr(desc, "Milo ") == desc);
    for (size_t capacity = 1; capacity < 30; ++capacity) {
        std::memset(desc, 0x7f, sizeof(desc));
        bot.describe(desc, capacity, Bot::Language::Chinese);
        assert(std::strlen(desc) < capacity && desc[capacity] == 0x7f);
        // Our corpus consists of ASCII and three-byte UTF-8 codepoints.
        for (size_t i = 0; desc[i];) {
            if ((unsigned char)desc[i] < 128) ++i;
            else { assert(desc[i + 1] && desc[i + 2]); i += 3; }
        }
    }
    assert(Bot::moodCount() * Bot::expressionCount() * Bot::animationCount() == 1120);
    M5Canvas canvas(72, 72); bot.begin(&canvas);
    bot.resetToIdle(); gNow += 16; bot.update(gNow);
    bot.describe(desc, sizeof(desc)); assert(std::strcmp(desc, "Milo is resting") == 0);
    bot.setExpression(Bot::Expression::Dizzy, 0); gNow += 16; bot.update(gNow);
    bot.describe(desc, sizeof(desc)); assert(std::strcmp(desc, "Milo feels dizzy") == 0);
    bot.describe(desc, sizeof(desc), Bot::Language::Chinese);
    assert(std::strcmp(desc, "Milo 感到眩晕") == 0);
    bot.setMood(Bot::Mood::Thinking, 0); gNow += 16; bot.update(gNow);
    bot.describe(desc, sizeof(desc)); assert(std::strcmp(desc, "Milo is thinking") == 0);
    bot.describe(desc, sizeof(desc), Bot::Language::Chinese);
    assert(std::strcmp(desc, "Milo 正在思考") == 0);
    bot.setName("Ava"); bot.setMood(Bot::Mood::Happy, 0); gNow += 16; bot.update(gNow);
    bot.describe(desc, sizeof(desc)); assert(std::strcmp(desc, "Ava is happy") == 0);
    bot.describe(desc, sizeof(desc), Bot::Language::Chinese);
    assert(std::strcmp(desc, "Ava 很高兴") == 0);
    bot.setMood(Bot::Mood::Done, 0); gNow += 16; bot.update(gNow);
    bot.describe(desc, sizeof(desc)); assert(std::strcmp(desc, "Ava has finished") == 0);
    bot.describe(desc, sizeof(desc), Bot::Language::Chinese);
    assert(std::strcmp(desc, "Ava 已经完成") == 0);
    bot.setMood(Bot::Mood::LookingAround, 0); gNow += 16; bot.update(gNow);
    bot.describe(desc, sizeof(desc)); assert(std::strcmp(desc, "Ava is looking around") == 0);
    bot.describe(desc, sizeof(desc), Bot::Language::Chinese);
    assert(std::strcmp(desc, "Ava 正在到处看看") == 0);
    for (uint8_t m = 0; m < Bot::moodCount(); ++m)
        for (uint8_t e = 0; e < Bot::expressionCount(); ++e)
            for (uint8_t a = 0; a < Bot::animationCount(); ++a) {
                assert(*Bot::moodName((Bot::Mood)m, Bot::Language::Chinese));
                assert(*Bot::expressionName((Bot::Expression)e, Bot::Language::Chinese));
                assert(*Bot::animationName((Bot::Animation)a, Bot::Language::Chinese));
                bot.setMood((Bot::Mood)m, 0); bot.setExpression((Bot::Expression)e, 0);
                bot.setAnimation((Bot::Animation)a); gNow += 16; bot.update(gNow);
                canvas.clear(); bot.draw(); assert(!canvas.outOfBounds());
            }
}

static float eyeGroupCenterX(M5Canvas& canvas) {
    int left = canvas.width(), right = -1;
    for (int y = 0; y < canvas.height(); ++y) for (int x = 0; x < canvas.width(); ++x)
        if (canvas.readPixel(x, y) == botux::rgb565(32, 36, 41)) {
            left = std::min(left, x); right = std::max(right, x);
        }
    assert(right >= left); return (left + right) * 0.5f;
}
static void checkTemporaryGaze() {
    using Bot = botux::BotUx;
    M5Canvas canvas(200, 200); Bot bot; bot.begin(&canvas);
    auto style = bot.style(); style.blinkMinMs = style.blinkMaxMs = 60000; bot.setStyle(style);
    bot.setExpression(Bot::Expression::Neutral, 0); bot.setMotionAmount(0);
    gNow += 1000; bot.update(gNow); canvas.clear(); bot.draw();
    float baseline = eyeGroupCenterX(canvas);
    bot.setExpression(Bot::Expression::Neutral, 0); bot.gazeAt(-1, 0, 1800); gNow += 240; bot.update(gNow); canvas.clear(); bot.draw();
    float left = eyeGroupCenterX(canvas);
    bot.setExpression(Bot::Expression::Neutral, 0); bot.gazeAt(1, 0, 1800);
    gNow += 240; bot.update(gNow); canvas.clear(); bot.draw();
    assert(eyeGroupCenterX(canvas) > left + 20);
    gNow += 1900; bot.setExpression(Bot::Expression::Neutral, 0); bot.update(gNow); canvas.clear(); bot.draw();
    assert(std::fabs(eyeGroupCenterX(canvas) - baseline) <= 1.5f);

    // A temporary target has precedence over a persistent direction, then
    // returns to that direction when its hold expires.
    bot.setGazeDirection(Bot::GazeDirection::Left);
    gNow += 200; bot.update(gNow); canvas.clear(); bot.draw();
    float persistentLeft = eyeGroupCenterX(canvas);
    bot.gazeAt(0.70710678f, -0.70710678f, 600);
    gNow += 400; bot.update(gNow); canvas.clear(); bot.draw();
    assert(eyeGroupCenterX(canvas) > baseline - 2);
    gNow += 500; bot.update(gNow); canvas.clear(); bot.draw();
    assert(std::fabs(eyeGroupCenterX(canvas) - persistentLeft) <= 1.5f);
}

static void checkConnectedEyeMorphs() {
    using Bot = botux::BotUx;
    for (int size : {40, 72, 200}) for (int styleIndex = 0; styleIndex < 4; ++styleIndex) {
        M5Canvas canvas(size, size); Bot bot; bot.begin(&canvas);
        auto style = bot.style(); style.bodyStyle = Bot::BodyStyle::None;
        style.eyeStyle = (Bot::EyeStyle)styleIndex;
        style.bgColor = botux::rgb565(255,255,255);
        style.eyeColor = style.pupilColor = botux::rgb565(0,0,0);
        style.blinkMinMs = style.blinkMaxMs = 60000; bot.setStyle(style);
        bot.setMotionAmount(0); bot.setExpression(Bot::Expression::Neutral, 0);
        gNow += 16; bot.update(gNow); bot.setExpression(Bot::Expression::Joy, 600);
        for (int frame = 0; frame < 40; ++frame) {
            gNow += 25; bot.update(gNow); canvas.clear(); bot.draw();
            std::vector<bool> seen(size * size, false); int components = 0;
            for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
                if (seen[y * size + x] || ((canvas.readPixel(x,y) >> 11) & 31) >= 24) continue;
                ++components; std::vector<int> queue(1, y * size + x); seen[y * size + x] = true;
                for (size_t k = 0; k < queue.size(); ++k) {
                    int px = queue[k] % size, py = queue[k] / size;
                    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                        int nx = px + dx, ny = py + dy;
                        if (nx < 0 || ny < 0 || nx >= size || ny >= size) continue;
                        int id = ny * size + nx;
                        if (!seen[id] && ((canvas.readPixel(nx,ny) >> 11) & 31) < 24) {
                            seen[id] = true; queue.push_back(id);
                        }
                    }
                }
            }
            assert(components == 2);
        }
    }
}

static uint32_t rasterHash(M5Canvas& canvas) {
    uint32_t hash = 2166136261u;
    for (int y = 0; y < canvas.height(); ++y) for (int x = 0; x < canvas.width(); ++x)
        hash = (hash ^ canvas.readPixel(x, y)) * 16777619u;
    return hash;
}

struct PixelRegion {
    int x0 = 10000, x1 = -1, y0 = 10000, y1 = -1, pixels = 0;
    void add(int x, int y) {
        x0 = std::min(x0, x); x1 = std::max(x1, x);
        y0 = std::min(y0, y); y1 = std::max(y1, y); ++pixels;
    }
    int width() const { return x1 - x0 + 1; }
    int height() const { return y1 - y0 + 1; }
};

static std::vector<PixelRegion> exactColorRegions(const M5Canvas& canvas,
                                                   uint16_t color) {
    int width = canvas.width(), height = canvas.height();
    std::vector<bool> seen(width * height, false);
    std::vector<PixelRegion> regions;
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        int start = y * width + x;
        if (seen[start] || canvas.readPixel(x, y) != color) continue;
        PixelRegion region;
        std::vector<int> queue(1, start); seen[start] = true;
        for (size_t q = 0; q < queue.size(); ++q) {
            int px = queue[q] % width, py = queue[q] / width;
            region.add(px, py);
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                int nx = px + dx, ny = py + dy;
                if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
                int id = ny * width + nx;
                if (!seen[id] && canvas.readPixel(nx, ny) == color) {
                    seen[id] = true; queue.push_back(id);
                }
            }
        }
        regions.push_back(region);
    }
    return regions;
}

static PixelRegion exactColorBounds(const M5Canvas& canvas, uint16_t color) {
    PixelRegion bounds;
    for (int y = 0; y < canvas.height(); ++y) for (int x = 0; x < canvas.width(); ++x)
        if (canvas.readPixel(x, y) == color) bounds.add(x, y);
    return bounds;
}

static bool isBlushPixel(uint16_t color) {
    int red = (color >> 11) & 31, green = (color >> 5) & 63, blue = color & 31;
    return red >= 28 && green >= 25 && green < 57 && blue < 27;
}

static void checkHappyDoneSemantics() {
    using Bot = botux::BotUx;
    Bot::Style themes[] = {watchThemeStyle(false), watchThemeStyle(true)};
    for (int theme = 0; theme < 2; ++theme) for (int size : {40, 72, 120, 286}) {
        uint32_t hashes[2] = {};
        for (int moodIndex = 0; moodIndex < 2; ++moodIndex) {
            Bot::Mood mood = moodIndex ? Bot::Mood::Done : Bot::Mood::Happy;
            for (int motionMode = 0; motionMode < 3; ++motionMode) {
                gNow = 1000; M5Canvas canvas(size, size); canvas.setRecording(false);
                Bot bot; bot.begin(&canvas); bot.setStyle(themes[theme]); bot.seedBlink(1234);
                bot.setMood(mood, 0); bot.setAnimationSpeed(0.73f);
                bot.setMotionAmount(motionMode == 2 ? 0.0f : 0.55f);
                bot.setReducedMotion(motionMode == 1);
                gNow += 713; bot.update(gNow); bot.draw();
                assert(!canvas.outOfBounds());
                std::vector<PixelRegion> accents = exactColorRegions(
                    canvas, themes[theme].accentColor);
                assert(accents.size() == 2);
                for (const auto& star : accents) {
                    assert(star.y1 < bot.metrics().cy);
                    assert(star.height() >= std::max(5, (int)(bot.metrics().bodyR * 0.30f)));
                    assert(star.height() > star.width());
                    assert(star.pixels * 4 < star.width() * star.height() * 3);
                }
                if (mood == Bot::Mood::Done) {
                    uint16_t checkColor = mix565ForTest(
                        themes[theme].accentColor, themes[theme].eyeColor, 96);
                    std::vector<PixelRegion> checks = exactColorRegions(canvas, checkColor);
                    assert(checks.size() == 1);
                    const PixelRegion& check = checks[0];
                    assert(check.y0 > bot.metrics().cy + bot.metrics().bodyR * 0.20f);
                    assert(check.y1 > bot.metrics().cy + bot.metrics().bodyR * 0.54f);
                    assert(check.x0 < bot.metrics().cx - bot.metrics().bodyR * 0.34f);
                    assert(check.x1 > bot.metrics().cx);
                    int bodyLuma = ((themes[theme].bodyColor >> 11) & 31) * 2
                                 + ((themes[theme].bodyColor >> 5) & 63) * 3
                                 + (themes[theme].bodyColor & 31);
                    int checkLuma = ((checkColor >> 11) & 31) * 2
                                  + ((checkColor >> 5) & 63) * 3
                                  + (checkColor & 31);
                    assert(std::abs(bodyLuma - checkLuma) >= 90);
                }
                if (motionMode == 0) hashes[moodIndex] = rasterHash(canvas);
            }
        }
        assert(hashes[0] != hashes[1]);
    }

    // Every explicit expression remains selectable. Mood identity decorations
    // track the directed eye group and stay vertically separate at all sizes.
    for (int size : {40, 72, 286})
        for (uint8_t expression = 1; expression < Bot::expressionCount(); ++expression)
            for (uint8_t gaze = 1; gaze < Bot::gazeDirectionCount(); ++gaze)
                for (int moodIndex = 0; moodIndex < 2; ++moodIndex) {
                    Bot::Mood mood = moodIndex ? Bot::Mood::Done : Bot::Mood::Happy;
                    gNow = 1000; M5Canvas canvas(size, size); canvas.setRecording(false);
                    Bot bot; bot.begin(&canvas); Bot::Style style = bot.style();
                    style.bgColor = botux::rgb565(0, 0, 0);
                    style.bodyColor = botux::rgb565(255, 255, 255);
                    style.eyeColor = style.pupilColor = botux::rgb565(0, 255, 0);
                    style.accentColor = botux::rgb565(70, 130, 255);
                    style.blushColor = botux::rgb565(255, 80, 110);
                    style.blinkMinMs = style.blinkMaxMs = 600000; bot.setStyle(style);
                    bot.seedBlink(1234); bot.setMood(mood, 0);
                    bot.setExpression((Bot::Expression)expression, 0);
                    bot.setAnimation(Bot::Animation::Calm); bot.setMotionAmount(0);
                    bot.setGazeDirection((Bot::GazeDirection)gaze); bot.update(gNow); bot.draw();
                    assert(bot.effectiveExpression() == (Bot::Expression)expression);
                    PixelRegion eyes = exactColorBounds(canvas, style.eyeColor);
                    assert(eyes.pixels > 0);
                    if (mood == Bot::Mood::Done) {
                        uint16_t checkColor = mix565ForTest(style.accentColor, style.eyeColor, 96);
                        std::vector<PixelRegion> checks = exactColorRegions(canvas, checkColor);
                        assert(checks.size() == 1);
                        assert(eyes.y1 < checks[0].y0);
                    } else {
                        PixelRegion blush;
                        for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x)
                            if (isBlushPixel(canvas.readPixel(x, y))) blush.add(x, y);
                        assert(blush.pixels > 0);
                        assert(eyes.y1 < blush.y0);
                    }
                    assert(!canvas.outOfBounds());
                }

    // poke() keeps authored transient faces, then restores the selected face.
    gNow = 1000; M5Canvas canvas(72, 72); Bot bot; bot.begin(&canvas);
    bot.setExpression(Bot::Expression::Skeptical, 0); bot.update(gNow); bot.poke();
    bot.update(1100); assert(bot.effectiveExpression() == Bot::Expression::Alarmed);
    bot.update(1400); assert(bot.effectiveExpression() == Bot::Expression::Joy);
    bot.update(2300); assert(bot.effectiveExpression() == Bot::Expression::Skeptical);
}

static void checkPersistentAnimationWindows() {
    using Bot = botux::BotUx;
    M5Canvas canvas(72, 72); canvas.setRecording(false);
    for (uint8_t mood = 0; mood < Bot::moodCount(); ++mood)
        for (uint8_t expr = 0; expr < Bot::expressionCount(); ++expr)
            for (uint8_t anim = 0; anim < Bot::animationCount(); ++anim) {
                gNow = 1000; Bot bot; bot.begin(&canvas);
                auto style = bot.style(); style.blinkMinMs = style.blinkMaxMs = 60000; bot.setStyle(style); bot.seedBlink(1234);
                bot.setMood((Bot::Mood)mood, 0); bot.setExpression((Bot::Expression)expr, 0);
                bot.setAnimation((Bot::Animation)anim); bot.setTalking(mood == (uint8_t)Bot::Mood::Speaking);
                // Re-sample late windows too: changes must not depend on entry morph/blink.
                for (uint32_t window : {10000u, 30000u, 50000u}) {
                    std::set<uint32_t> frames;
                    for (int frame = 0; frame < 12; ++frame) {
                        gNow = window + frame * 250; bot.update(gNow); canvas.clear(); bot.draw();
                        assert(!canvas.outOfBounds()); frames.insert(rasterHash(canvas));
                    }
                    if (frames.size() < 4) std::cerr << "frozen " << (int)mood << "/" << (int)expr << "/" << (int)anim << " at " << window << " unique " << frames.size() << "\n";
                    assert(frames.size() >= 4);
                }
            }
    for (uint8_t preset = 0; preset < Bot::presetCount(); ++preset) {
        gNow = 1000; Bot bot; bot.begin(&canvas); bot.applyPreset(preset); bot.setMotionAmount(0.55f);
        for (uint32_t window : {10000u, 40000u, 80000u}) {
            std::set<uint32_t> frames;
            for (int frame = 0; frame < 16; ++frame) {
                gNow = window + frame * 250; bot.update(gNow); canvas.clear(); bot.draw();
                frames.insert(rasterHash(canvas));
            }
            assert(frames.size() >= 8);
        }
    }
}

static void checkGazeDirectionsAndMotionZero() {
    using Bot = botux::BotUx;
    M5Canvas canvas(200, 200); canvas.setRecording(false);
    gNow = 1000; Bot bot; bot.begin(&canvas);
    auto style = bot.style(); style.blinkMinMs = style.blinkMaxMs = 600000; bot.setStyle(style);
    bot.setExpression(Bot::Expression::Neutral, 0); bot.setMotionAmount(0);
    bot.setGazeDirection(Bot::GazeDirection::Left); bot.update(gNow); canvas.clear(); bot.draw();
    float left = eyeGroupCenterX(canvas);
    bot.setGazeDirection(Bot::GazeDirection::Right); bot.setExpression(Bot::Expression::Neutral, 0);
    gNow += 1000; bot.update(gNow); canvas.clear(); bot.draw();
    assert(eyeGroupCenterX(canvas) > left + 25);
    bot.gazeAt(-1, 0); gNow += 200; bot.update(gNow); canvas.clear(); bot.draw();
    assert(eyeGroupCenterX(canvas) < left + 5);
    gNow += 2000; bot.update(gNow); canvas.clear(); bot.draw();
    assert(eyeGroupCenterX(canvas) > left + 25);
    assert(bot.gazeDirection() == Bot::GazeDirection::Right);
    for (uint8_t mood = 0; mood < Bot::moodCount(); ++mood) {
        bot.setMood((Bot::Mood)mood, 0); bot.setExpression(Bot::Expression::Dizzy, 0);
        bot.setAnimation(Bot::Animation::Orbit); bot.setGazeDirection(Bot::GazeDirection::Auto);
        for (int i = 0; i < 10; ++i) { gNow += 1000; bot.update(gNow); }
        canvas.clear(); bot.draw(); uint32_t first = rasterHash(canvas);
        for (int i = 0; i < 5; ++i) {
            gNow += 300; bot.update(gNow); canvas.clear(); bot.draw();
            assert(rasterHash(canvas) == first);
        }
    }
}

static void checkDirectionsDominateEveryFace() {
    using Bot = botux::BotUx;
    M5Canvas canvas(96, 96); canvas.setRecording(false);
    for (uint8_t mood = 0; mood < Bot::moodCount(); ++mood) {
        if (mood == (uint8_t)Bot::Mood::Thinking || mood == (uint8_t)Bot::Mood::Working ||
            mood == (uint8_t)Bot::Mood::Blocked) continue;
        for (uint8_t expr = 0; expr < Bot::expressionCount(); ++expr)
            for (uint8_t eyeStyle = 0; eyeStyle < 4; ++eyeStyle)
                for (auto direction : {Bot::GazeDirection::Left, Bot::GazeDirection::Right,
                                       Bot::GazeDirection::Up, Bot::GazeDirection::Down,
                                       Bot::GazeDirection::UpLeft, Bot::GazeDirection::UpRight,
                                       Bot::GazeDirection::DownLeft, Bot::GazeDirection::DownRight}) {
                    gNow = 1000; Bot bot; bot.begin(&canvas);
                    auto style = bot.style(); style.eyeStyle = (Bot::EyeStyle)eyeStyle;
                    style.blinkMinMs = style.blinkMaxMs = 600000;
                    style.bodyColor = botux::rgb565(255,255,255);
                    style.accentColor = style.blushColor = style.bodyColor;
                    style.eyeColor = style.pupilColor = 0;
                    bot.setStyle(style); bot.seedBlink(1234);
                    bot.setMood((Bot::Mood)mood); bot.setExpression((Bot::Expression)expr);
                    bot.setAnimation(Bot::Animation::Calm); bot.setMotionAmount(0.55f);
                    // Begin on the expression's natural placement, then choose a direction.
                    for (int i = 0; i < 30; ++i) { gNow += 33; bot.update(gNow); }
                    bot.setGazeDirection(direction);
                    for (int i = 0; i < 45; ++i) { gNow += 33; bot.update(gNow); }
                    canvas.clear(); bot.draw(); assert(!canvas.outOfBounds());
                    float bodyX = 0, bodyY = 0;
                    int eyeMinX = 96, eyeMaxX = 0, eyeMinY = 96, eyeMaxY = 0;
                    unsigned bodyPixels = 0, eyePixels = 0;
                    for (int y = 0; y < 96; ++y) for (int x = 0; x < 96; ++x) {
                        uint16_t c = canvas.readPixel(x,y);
                        if (c != style.bgColor) { bodyX += x; bodyY += y; ++bodyPixels; }
                        // Thin closed eyes can have only partial-coverage pixels.
                        // Limit detection to the upper orb interior to exclude
                        // its AA edge and the lower Done check.
                        float ox=x-bot.metrics().cx, oy=y-bot.metrics().cy;
                        if (c != style.bgColor && ((c>>11)&31)<24 &&
                            oy < bot.metrics().bodyR * 0.30f &&
                            ox*ox+oy*oy < bot.metrics().bodyR*bot.metrics().bodyR*.64f) {
                            eyeMinX = std::min(eyeMinX, x); eyeMaxX = std::max(eyeMaxX, x);
                            eyeMinY = std::min(eyeMinY, y); eyeMaxY = std::max(eyeMaxY, y); ++eyePixels;
                        }
                    }
                    assert(bodyPixels && eyePixels);
                    // Use geometry extent, not ink mass: Wink's open eye has much
                    // more ink than its closed eye without moving the eye pair.
                    float dx = (eyeMinX + eyeMaxX) * 0.5f - bodyX / bodyPixels;
                    float dy = (eyeMinY + eyeMaxY) * 0.5f - bodyY / bodyPixels;
                    float r = bot.metrics().bodyR;
                    int index = (int)direction;
                    static const int8_t sx[] = {0,0,-1,1,0,0,-1,1,-1,1};
                    static const int8_t sy[] = {0,0,0,0,-1,1,-1,-1,1,1};
                    if (sx[index] && dx*sx[index]<r*.12f) std::cerr<<"direction "<<(int)mood<<"/"<<(int)expr<<"/"<<(int)eyeStyle<<" d="<<index<<" dx="<<dx<<"\n";
                    if (sx[index]) assert(dx * sx[index] >= r * 0.12f);
                    // Downward pitch is deliberately nearer center than upward pitch.
                    if (sy[index]) assert(dy * sy[index] >= r * 0.06f);
                }
    }
}

static void checkReducedMotionAmplitude() {
    using Bot = botux::BotUx;
    unsigned changes[2] = {};
    for (int reduced = 0; reduced < 2; ++reduced) {
        gNow = 1000; M5Canvas canvas(120, 120); canvas.setRecording(false);
        Bot bot; bot.begin(&canvas); bot.setReducedMotion(reduced);
        bot.setGazeDirection(Bot::GazeDirection::Center);
        bot.setExpression(Bot::Expression::Neutral, 0); bot.setAnimation(Bot::Animation::Calm);
        auto style = bot.style(); style.blinkMinMs = style.blinkMaxMs = 600000; bot.setStyle(style);
        std::vector<uint16_t> previous(120 * 120);
        for (int frame = 0; frame < 40; ++frame) {
            gNow = 10000 + frame * 100; bot.update(gNow); canvas.clear(); bot.draw();
            for (int y = 0; y < 120; ++y) for (int x = 0; x < 120; ++x) {
                uint16_t pixel = canvas.readPixel(x, y);
                if (frame) {
                    uint16_t prior = previous[y * 120 + x];
                    changes[reduced] += 2 * std::abs((int)(pixel >> 11) - (int)(prior >> 11))
                        + std::abs((int)((pixel >> 5) & 63) - (int)((prior >> 5) & 63))
                        + 2 * std::abs((int)(pixel & 31) - (int)(prior & 31));
                }
                previous[y * 120 + x] = pixel;
            }
        }
    }
    assert(changes[0] > changes[1] * 2);
    assert(changes[1] > 0);
    std::cout << "Calm RGB565 temporal difference full/reduced: " << changes[0] << "/" << changes[1] << "\n";
}

struct EyeBox {
    int x0 = 200, x1 = -1, y0 = 200, y1 = -1, pixels = 0;
    void add(int x, int y) { x0 = std::min(x0,x); x1 = std::max(x1,x); y0 = std::min(y0,y); y1 = std::max(y1,y); ++pixels; }
    float cx() const { return (x0+x1)*0.5f; }
    float cy() const { return (y0+y1)*0.5f; }
    int height() const { return y1-y0+1; }
};
struct DirectionRaster { std::vector<uint16_t> pixels; EyeBox eyes[2]; };
static DirectionRaster captureDirectionRaster(const M5Canvas& canvas, uint16_t eyeColor) {
    DirectionRaster result; result.pixels.resize(40000); EyeBox group;
    for (int y=0;y<200;++y) for(int x=0;x<200;++x) {
        uint16_t c=canvas.readPixel(x,y); result.pixels[y*200+x]=c;
        if(c==eyeColor) group.add(x,y);
    }
    for (int y=0;y<200;++y) for(int x=0;x<200;++x)
        if(result.pixels[y*200+x]==eyeColor) result.eyes[x<group.cx()?0:1].add(x,y);
    assert(result.eyes[0].pixels && result.eyes[1].pixels);
    return result;
}
static DirectionRaster directionRaster(botux::BotUx::GazeDirection direction, bool tap = false, bool autoFace = false) {
    using Bot = botux::BotUx;
    gNow = 1000; M5Canvas canvas(200,200); canvas.setRecording(false); Bot bot; bot.begin(&canvas);
    auto style = bot.style(); style.blinkMinMs = style.blinkMaxMs = 600000; bot.setStyle(style); bot.seedBlink(1234);
    bot.setMotionAmount(0); bot.setExpression(autoFace ? Bot::Expression::Auto : Bot::Expression::Neutral);
    for (int i=0;i<100;++i) { gNow+=16; bot.update(gNow); }
    if (tap) bot.gazeAt(-0.70710678f,0.70710678f,5000); else bot.setGazeDirection(direction);
    for (int i=0;i<100;++i) { gNow+=16; bot.update(gNow); }
    bot.draw(); return captureDirectionRaster(canvas, style.eyeColor);
}
static void checkNineDirectionPerspective() {
    using Bot=botux::BotUx; using G=Bot::GazeDirection;
    assert(Bot::gazeDirectionCount()==10 && Bot::explicitGazeDirectionCount()==9);
    std::set<int> directions;
    for(uint8_t i=0;i<9;++i) directions.insert((int)Bot::explicitGazeDirection(i));
    assert(directions.size()==9 && !directions.count(0));
    auto front=directionRaster(G::Center), left=directionRaster(G::Left), right=directionRaster(G::Right);
    auto up=directionRaster(G::Up), down=directionRaster(G::Down);
    assert(left.eyes[0].pixels > left.eyes[1].pixels);
    assert(right.eyes[1].pixels > right.eyes[0].pixels);
    assert(left.eyes[0].height() <= left.eyes[1].height()*1.15f);
    assert(right.eyes[1].height() <= right.eyes[0].height()*1.15f);
    assert(std::abs(front.eyes[0].height()-front.eyes[1].height())<=1);
    assert(std::fabs((front.eyes[0].cx()+front.eyes[1].cx())*.5f-100)<1);
    assert(up.eyes[0].cy()<88 && down.eyes[0].cy()>110 && down.eyes[0].cy()<114);
    assert(up.eyes[0].height()>down.eyes[0].height());
    unsigned mirrorMismatch=0, eyeUnion=0;
    const uint16_t ink=botux::rgb565(32,36,41);
    for(int y=0;y<200;++y) for(int x=1;x<200;++x) {
        bool a=left.pixels[y*200+x]==ink, b=right.pixels[y*200+200-x]==ink;
        if(a||b) ++eyeUnion; if(a!=b) ++mirrorMismatch;
    }
    assert(mirrorMismatch < eyeUnion*0.12f);
    for(auto g : {G::UpLeft,G::UpRight,G::DownLeft,G::DownRight}) {
        auto frame=directionRaster(g);
        float x=(frame.eyes[0].cx()+frame.eyes[1].cx())*.5f;
        float y=(frame.eyes[0].cy()+frame.eyes[1].cy())*.5f;
        assert((g==G::UpLeft||g==G::DownLeft)?x<88:x>112);
        assert((g==G::UpLeft||g==G::UpRight)?y<88:y>109);
    }
    auto diagonal=directionRaster(G::DownLeft), tapped=directionRaster(G::Auto,true);
    assert(diagonal.pixels==tapped.pixels); // same continuous vector, exactly the same pose/raster
}

static void checkIdleAndUpperRightProportions() {
    using G = botux::BotUx::GazeDirection;
    const auto idle = directionRaster(G::Auto, false, true);
    const auto upperRight = directionRaster(G::UpRight, false, true);
    const auto upperLeft = directionRaster(G::UpLeft, false, true);
    const auto tappedUpperRight = [] {
        using Bot = botux::BotUx;
        gNow = 1000; M5Canvas canvas(200,200); canvas.setRecording(false); Bot bot; bot.begin(&canvas);
        auto style = bot.style(); style.blinkMinMs = style.blinkMaxMs = 600000; bot.setStyle(style); bot.seedBlink(1234);
        bot.setMotionAmount(0); bot.gazeAt(0.70710678f,-0.70710678f,5000);
        for (int i=0;i<200;++i) { gNow+=16; bot.update(gNow); }
        bot.draw(); return captureDirectionRaster(canvas, style.eyeColor);
    }();
    assert(idle.pixels == upperRight.pixels);
    assert(idle.pixels == tappedUpperRight.pixels);
    assert(idle.eyes[1].pixels > idle.eyes[0].pixels);
    assert(upperRight.eyes[1].pixels > upperRight.eyes[0].pixels);
    assert(idle.eyes[1].height() > idle.eyes[0].height());
    assert(upperRight.eyes[1].height() > upperRight.eyes[0].height());
    assert(upperLeft.eyes[0].pixels > upperLeft.eyes[1].pixels);
    for (int i = 0; i < 2; ++i) {
        float idleHeight = idle.eyes[i].height(), directedHeight = upperRight.eyes[i].height();
        float idleWidth = idle.eyes[i].x1-idle.eyes[i].x0+1;
        float directedWidth = upperRight.eyes[i].x1-upperRight.eyes[i].x0+1;
        assert(std::fabs(directedHeight / idleHeight - 1) < 0.12f);
        assert(std::fabs(directedWidth / directedHeight - idleWidth / idleHeight) < 0.08f);
        std::cout << "Idle/UpRight eye " << i << " width " << idleWidth << "/" << directedWidth
                  << " height " << idleHeight << "/" << directedHeight
                  << " ink " << idle.eyes[i].pixels << "/" << upperRight.eyes[i].pixels << "\n";
    }
    float idleRatio = (float)idle.eyes[1].pixels / idle.eyes[0].pixels;
    float directedRatio = (float)upperRight.eyes[1].pixels / upperRight.eyes[0].pixels;
    assert(std::fabs(idleRatio-directedRatio) < 0.12f);
    float idleSpacing = idle.eyes[1].cx()-idle.eyes[0].cx();
    float directedSpacing = upperRight.eyes[1].cx()-upperRight.eyes[0].cx();
    assert(std::fabs(directedSpacing/idleSpacing-1) < 0.12f);
}

static DirectionRaster expressionSideRaster(botux::BotUx::Expression expression,
                                             botux::BotUx::EyeStyle eyeStyle,
                                             botux::BotUx::GazeDirection direction,
                                             botux::BotUx::FaceSide faceSide = botux::BotUx::FaceSide::Auto) {
    using Bot = botux::BotUx;
    gNow=1000; M5Canvas canvas(200,200); canvas.setRecording(false); Bot bot; bot.begin(&canvas);
    auto style=bot.style(); style.bodyStyle=Bot::BodyStyle::None; style.eyeStyle=eyeStyle;
    style.blinkMinMs=style.blinkMaxMs=600000; bot.setStyle(style); bot.seedBlink(1234);
    bot.setMotionAmount(0); bot.setAnimation(Bot::Animation::Calm);
    bot.setExpression(expression,0); bot.setGazeDirection(direction); bot.setFaceSide(faceSide);
    bot.update(gNow); canvas.clear(); bot.draw();
    return captureDirectionRaster(canvas,style.eyeColor);
}

static DirectionRaster moodSideRaster(botux::BotUx::Mood mood,
                                      botux::BotUx::EyeStyle eyeStyle,
                                      botux::BotUx::GazeDirection direction) {
    using Bot = botux::BotUx;
    gNow=1000; M5Canvas canvas(200,200); canvas.setRecording(false); Bot bot; bot.begin(&canvas);
    auto style=bot.style(); style.bodyStyle=Bot::BodyStyle::None; style.eyeStyle=eyeStyle;
    style.blinkMinMs=style.blinkMaxMs=600000; bot.setStyle(style); bot.seedBlink(1234);
    bot.setMotionAmount(0); bot.setAnimation(Bot::Animation::Calm);
    bot.setMood(mood,0); bot.setExpression(Bot::Expression::Auto,0); bot.setGazeDirection(direction);
    for(int i=0;i<100;++i) { gNow+=16; bot.update(gNow); }
    canvas.clear(); bot.draw();
    return captureDirectionRaster(canvas,style.eyeColor);
}

static float onePixelMirrorMissRatio(const DirectionRaster& right,
                                     const DirectionRaster& left,
                                     uint16_t ink) {
    unsigned misses=0,total=0;
    for(int pass=0;pass<2;++pass) for(int y=0;y<200;++y) for(int x=0;x<200;++x) {
        const auto& source=pass?left:right;
        const auto& target=pass?right:left;
        if(source.pixels[y*200+x]!=ink) continue;
        ++total; bool found=false;
        for(int dy=-1;dy<=1&&!found;++dy) for(int dx=-1;dx<=1;++dx) {
            int tx=200-x+dx,ty=y+dy;
            if(tx>=0&&tx<200&&ty>=0&&ty<200&&target.pixels[ty*200+tx]==ink) {found=true;break;}
        }
        if(!found) ++misses;
    }
    return total ? (float)misses/total : 1.0f;
}

static void checkExpressionHandedness() {
    using Bot=botux::BotUx; using G=Bot::GazeDirection;
    const uint16_t ink=botux::rgb565(32,36,41);
    for(uint8_t expression=1;expression<Bot::expressionCount();++expression)
        for(uint8_t eyeStyle=0;eyeStyle<4;++eyeStyle) {
            auto right=expressionSideRaster((Bot::Expression)expression,(Bot::EyeStyle)eyeStyle,G::UpRight);
            auto left=expressionSideRaster((Bot::Expression)expression,(Bot::EyeStyle)eyeStyle,G::UpLeft);
            unsigned mismatch=0,total=0;
            for(int y=0;y<200;++y) for(int x=1;x<200;++x) {
                bool a=right.pixels[y*200+x]==ink,b=left.pixels[y*200+200-x]==ink;
                if(a||b) ++total;
                if(a!=b) ++mismatch;
            }
            assert(total && mismatch<total*.14f);
        }

    auto forcedRight=expressionSideRaster(Bot::Expression::Wink,Bot::EyeStyle::Oval,G::UpRight,Bot::FaceSide::Right);
    auto forcedLeft=expressionSideRaster(Bot::Expression::Wink,Bot::EyeStyle::Oval,G::UpRight,Bot::FaceSide::Left);
    assert(std::fabs((forcedRight.eyes[0].cx()+forcedRight.eyes[1].cx())*.5f-
                     (forcedLeft.eyes[0].cx()+forcedLeft.eyes[1].cx())*.5f)<1.0f);
    assert(forcedRight.pixels!=forcedLeft.pixels);

    const Bot::Mood faceMoods[]={Bot::Mood::Idle,Bot::Mood::Listening,Bot::Mood::Speaking,
        Bot::Mood::Happy,Bot::Mood::Sad,Bot::Mood::Sleepy,Bot::Mood::Surprised,
        Bot::Mood::Waiting,Bot::Mood::Done,Bot::Mood::Asleep,Bot::Mood::LookingAround};
    for(auto mood:faceMoods) for(uint8_t eyeStyle=0;eyeStyle<4;++eyeStyle) {
        auto right=moodSideRaster(mood,(Bot::EyeStyle)eyeStyle,G::UpRight);
        auto left=moodSideRaster(mood,(Bot::EyeStyle)eyeStyle,G::UpLeft);
        float missRatio=onePixelMirrorMissRatio(right,left,ink);
        if(missRatio>=.08f) std::cerr<<"mood mirror "<<(int)mood
            <<" style "<<(int)eyeStyle<<" one-pixel miss ratio "<<missRatio
            <<" R "<<right.eyes[0].x0<<","<<right.eyes[0].y0<<".."<<right.eyes[0].x1<<","<<right.eyes[0].y1
            <<" / "<<right.eyes[1].x0<<","<<right.eyes[1].y0<<".."<<right.eyes[1].x1<<","<<right.eyes[1].y1
            <<" L "<<left.eyes[0].x0<<","<<left.eyes[0].y0<<".."<<left.eyes[0].x1<<","<<left.eyes[0].y1
            <<" / "<<left.eyes[1].x0<<","<<left.eyes[1].y0<<".."<<left.eyes[1].x1<<","<<left.eyes[1].y1<<"\n";
        assert(missRatio<.08f);
    }
}

static void checkEyesStayInsideBody() {
    using Bot=botux::BotUx;
    for(int size:{40,72,178,286}) for(uint8_t expression=1;expression<Bot::expressionCount();++expression)
        for(uint8_t eyeStyle=0;eyeStyle<4;++eyeStyle)
            for(uint8_t gaze=1;gaze<Bot::gazeDirectionCount();++gaze) {
                gNow=1000; M5Canvas canvas(size,size); canvas.setRecording(false); Bot bot; bot.begin(&canvas);
                auto style=bot.style(); style.eyeStyle=(Bot::EyeStyle)eyeStyle;
                style.bodyColor=style.bgColor; style.blinkMinMs=style.blinkMaxMs=600000;
                bot.setStyle(style); bot.seedBlink(1234); bot.setMotionAmount(0);
                bot.setExpression((Bot::Expression)expression,0);
                bot.setGazeDirection((Bot::GazeDirection)gaze); bot.update(gNow); canvas.clear(); bot.draw();
                unsigned ink=0; float limit=bot.metrics().bodyR*.94f;
                for(int y=0;y<size;++y) for(int x=0;x<size;++x) if(canvas.readPixel(x,y)!=style.bgColor) {
                    float dx=x-bot.metrics().cx,dy=y-bot.metrics().cy;
                    assert(dx*dx+dy*dy<=limit*limit); ++ink;
                }
                assert(ink);
            }
}

static void checkLookingAroundRange() {
    using Bot=botux::BotUx;
    gNow=1000; M5Canvas canvas(200,200); canvas.setRecording(false); Bot bot; bot.begin(&canvas);
    auto style=bot.style(); style.bodyStyle=Bot::BodyStyle::None;
    style.blinkMinMs=style.blinkMaxMs=600000; bot.setStyle(style); bot.seedBlink(19);
    bot.setMood(Bot::Mood::LookingAround,0); bot.setAnimation(Bot::Animation::Calm);
    float minX=1000,maxX=-1000,minY=1000,maxY=-1000;
    for(int frame=0;frame<1500;++frame) {
        gNow+=40; bot.update(gNow); canvas.clear(); bot.draw();
        if(frame%10) continue;
        auto raster=captureDirectionRaster(canvas,style.eyeColor);
        float x=(raster.eyes[0].cx()+raster.eyes[1].cx())*.5f;
        float y=(raster.eyes[0].cy()+raster.eyes[1].cy())*.5f;
        minX=std::min(minX,x); maxX=std::max(maxX,x);
        minY=std::min(minY,y); maxY=std::max(maxY,y);
    }
    assert(minX<86 && maxX>114 && minY<78 && maxY>107);
    std::cout<<"LookingAround eye-group range x "<<minX<<".."<<maxX
             <<" y "<<minY<<".."<<maxY<<"\n";

    bot.gazeAt(0.70710678f,-0.70710678f,5000);
    for(int i=0;i<100;++i) {gNow+=16;bot.update(gNow);}
    canvas.clear();bot.draw();
    auto held=captureDirectionRaster(canvas,style.eyeColor);
    float heldX=(held.eyes[0].cx()+held.eyes[1].cx())*.5f;
    std::cout<<"LookingAround held upper-right x "<<heldX<<"\n";
    assert(heldX>114);
}

// Least-squares ink axis, measured from native pixels rather than pose scalars.
static float eyeInkSlope(const DirectionRaster& raster, int eye) {
    const auto& box = raster.eyes[eye];
    double xSum=0, ySum=0, xySum=0, yySum=0, count=0;
    const uint16_t ink=botux::rgb565(32,36,41);
    for(int y=box.y0;y<=box.y1;++y) for(int x=box.x0;x<=box.x1;++x) {
        if(raster.pixels[y*200+x]!=ink) continue;
        xSum+=x; ySum+=y; xySum+=x*y; yySum+=y*y; ++count;
    }
    return (float)((xySum-xSum*ySum/count)/(yySum-ySum*ySum/count));
}

static void checkSideArcTilt() {
    using Bot=botux::BotUx; using G=Bot::GazeDirection;
    const G right[]={G::UpRight,G::Right,G::DownRight};
    const G left[]={G::UpLeft,G::Left,G::DownLeft};
    const uint16_t ink=botux::rgb565(32,36,41);
    for(int pose=0;pose<3;++pose) {
        auto r=directionRaster(right[pose]), l=directionRaster(left[pose]);
        for(int eye=0;eye<2;++eye) {
            float rSlope=eyeInkSlope(r,eye), lSlope=eyeInkSlope(l,1-eye);
            if(pose==0) assert(rSlope>0.20f && lSlope<-.20f);
            if(pose==1) assert(std::fabs(rSlope)<0.02f && std::fabs(lSlope)<0.02f);
            if(pose==2) assert(rSlope<-.20f && lSlope>0.20f);
            assert(std::fabs(rSlope+lSlope)<0.025f);
            std::cout << "Side arc pose " << pose << " eye " << eye << " right/left slope " << rSlope << "/" << lSlope << "\n";
        }
        unsigned mismatch=0, total=0;
        for(int y=0;y<200;++y) for(int x=1;x<200;++x) {
            bool a=r.pixels[y*200+x]==ink,b=l.pixels[y*200+200-x]==ink;
            if(a||b) ++total;
            if(a!=b) ++mismatch;
        }
        assert(mismatch<total*0.12f);
    }
    for(int side:{-1,1}) {
        gNow=1000; M5Canvas canvas(200,200); canvas.setRecording(false); Bot bot; bot.begin(&canvas);
        auto style=bot.style(); style.blinkMinMs=style.blinkMaxMs=600000; bot.setStyle(style); bot.seedBlink(1234);
        bot.setMotionAmount(0); bot.setExpression(Bot::Expression::Neutral);
        bot.gazeAt(side*0.707107f,-0.707107f,10000);
        for(int i=0;i<100;++i) {gNow+=16;bot.update(gNow);}
        float previous=0, maxStep=0;
        for(int frame=0;frame<=240;++frame) {
            float angle=-0.785398f+1.570796f*std::min(frame,180)/180.0f;
            gNow+=16; bot.gazeAt(side*cosf(angle),sinf(angle),10000); bot.update(gNow); canvas.clear(); bot.draw();
            assert(!canvas.outOfBounds());
            float slope=eyeInkSlope(captureDirectionRaster(canvas,style.eyeColor),1);
            if(frame) maxStep=std::max(maxStep,std::fabs(slope-previous));
            if(frame==0) assert(slope*side>0.15f);
            if(frame==90) assert(std::fabs(slope)<0.06f);
            if(frame==240) assert(slope*side<-.15f);
            previous=slope;
        }
        std::cout << "Continuous side arc " << side << " maximum 16ms ink-axis step " << maxStep << "\n";
        assert(maxStep<0.055f);
    }
}

static void writeIdleComparison(const std::string& path) {
    using G = botux::BotUx::GazeDirection;
    const DirectionRaster frames[] = {directionRaster(G::Auto,false,true),
        directionRaster(G::UpRight,false,true), directionRaster(G::UpLeft,false,true)};
    std::ofstream out(path, std::ios::binary); assert(out);
    out << "P6\n600 200\n255\n";
    for (int y=0;y<200;++y) for (const auto& frame:frames) for (int x=0;x<200;++x) {
        uint16_t pixel=frame.pixels[y*200+x];
        char rgb[3]={(char)(((pixel>>11)&31)*255/31),(char)(((pixel>>5)&63)*255/63),(char)((pixel&31)*255/31)};
        out.write(rgb,3);
    }
}

static void writeHappyDoneThemeComparison(const std::string& path) {
    using Bot = botux::BotUx;
    const int size = 286;
    Bot::Style themes[] = {watchThemeStyle(false), watchThemeStyle(true)};
    std::vector<uint16_t> pixels(size * size * 4);
    for (int theme = 0; theme < 2; ++theme) for (int mood = 0; mood < 2; ++mood) {
        gNow = 1000; M5Canvas canvas(size, size); canvas.setRecording(false);
        Bot bot; bot.begin(&canvas); bot.setStyle(themes[theme]); bot.seedBlink(1234);
        bot.setAnimationSpeed(0.73f); bot.setMotionAmount(0.55f);
        bot.setMood(mood ? Bot::Mood::Done : Bot::Mood::Happy, 0);
        gNow += 713; bot.update(gNow); bot.draw(); assert(!canvas.outOfBounds());
        int originX = mood * size, originY = theme * size;
        for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x)
            pixels[(originY + y) * size * 2 + originX + x] = canvas.readPixel(x, y);
    }
    std::ofstream out(path, std::ios::binary); assert(out);
    out << "P6\n" << size * 2 << " " << size * 2 << "\n255\n";
    for (uint16_t pixel : pixels) {
        char rgb[3] = {(char)(((pixel >> 11) & 31) * 255 / 31),
                       (char)(((pixel >> 5) & 63) * 255 / 63),
                       (char)((pixel & 31) * 255 / 31)};
        out.write(rgb, 3);
    }
}

static void writeHappyDoneSmallComparison(const std::string& path) {
    using Bot = botux::BotUx;
    const int cell = 72, width = cell * 4, height = cell * 2;
    Bot::Style themes[] = {watchThemeStyle(false), watchThemeStyle(true)};
    std::vector<uint16_t> pixels(width * height);
    for (int theme = 0; theme < 2; ++theme) {
        for (int y = 0; y < cell; ++y) for (int x = 0; x < width; ++x)
            pixels[(theme * cell + y) * width + x] = themes[theme].bgColor;
        for (int sizeIndex = 0; sizeIndex < 2; ++sizeIndex) for (int mood = 0; mood < 2; ++mood) {
            int size = sizeIndex ? 72 : 40;
            gNow = 1000; M5Canvas canvas(size, size); canvas.setRecording(false);
            Bot bot; bot.begin(&canvas); bot.setStyle(themes[theme]); bot.seedBlink(1234);
            bot.setAnimationSpeed(0.73f); bot.setMotionAmount(0.55f);
            bot.setMood(mood ? Bot::Mood::Done : Bot::Mood::Happy, 0);
            gNow += 713; bot.update(gNow); bot.draw(); assert(!canvas.outOfBounds());
            int cellIndex = sizeIndex * 2 + mood;
            int originX = cellIndex * cell + (cell - size) / 2;
            int originY = theme * cell + (cell - size) / 2;
            for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x)
                pixels[(originY + y) * width + originX + x] = canvas.readPixel(x, y);
        }
    }
    std::ofstream out(path, std::ios::binary); assert(out);
    out << "P6\n" << width << " " << height << "\n255\n";
    for (uint16_t pixel : pixels) {
        char rgb[3] = {(char)(((pixel >> 11) & 31) * 255 / 31),
                       (char)(((pixel >> 5) & 63) * 255 / 63),
                       (char)((pixel & 31) * 255 / 31)};
        out.write(rgb, 3);
    }
}

static void checkDirectionTransitionContinuity() {
    using Bot=botux::BotUx; using G=Bot::GazeDirection;
    gNow=1000; M5Canvas canvas(200,200); canvas.setRecording(false); Bot bot; bot.begin(&canvas);
    auto style=bot.style(); style.bodyStyle=Bot::BodyStyle::None;
    style.blinkMinMs=style.blinkMaxMs=600000; bot.setStyle(style); bot.seedBlink(1234);
    bot.setMotionAmount(0); bot.setExpression(Bot::Expression::Neutral);
    float previous=100; bool first=true;
    for(auto direction : {G::Center,G::Left,G::Right,G::UpLeft,G::DownRight,G::Center}) {
        bot.setGazeDirection(direction);
        for(int i=0;i<100;++i) {
            gNow+=16; bot.update(gNow); canvas.clear(); bot.draw();
            float center=eyeGroupCenterX(canvas);
            if(!first) assert(std::fabs(center-previous)<=8.0f);
            first=false; previous=center;
        }
    }
    assert(std::fabs(previous-100)<=1.0f);
}

static void checkSleepContinuity() {
    using Bot = botux::BotUx;
    static_assert((int)Bot::Mood::Done == 11 && (int)Bot::Mood::Asleep == 12 &&
                  (int)Bot::Mood::LookingAround == 13, "append-only moods");
    double totals[3] = {};
    for (int body=0; body<4; ++body) {
    for (int mode = 0; mode < 3; ++mode) {
        gNow = 1000; M5Canvas canvas(120,120); canvas.setRecording(false); Bot bot; bot.begin(&canvas);
        auto style = bot.style(); style.bodyStyle = (Bot::BodyStyle)body;
        style.blinkMinMs = style.blinkMaxMs = 600000; bot.setStyle(style);
        bot.setMood(Bot::Mood::Asleep, 0); bot.setReducedMotion(mode == 1); bot.setMotionAmount(mode == 2 ? 0 : 1);
        std::vector<uint16_t> prior(14400); double wrapMax=0, ordinaryMax=0;
        for (int frame=0; frame<=610; ++frame) {
            gNow=1000+frame*16; bot.update(gNow); canvas.clear(); bot.draw(); assert(!canvas.outOfBounds());
            double diff=0;
            for(int y=0;y<120;++y) for(int x=0;x<120;++x) {
                uint16_t pixel=canvas.readPixel(x,y), old=prior[y*120+x];
                diff += std::abs(int((pixel>>11)&31)-int((old>>11)&31));
                diff += std::abs(int((pixel>>5)&63)-int((old>>5)&63));
                diff += std::abs(int(pixel&31)-int(old&31)); prior[y*120+x]=pixel;
            }
            if(frame>20) {
                totals[mode]+=diff;
                if(frame%100<=1) wrapMax=std::max(wrapMax,diff); else ordinaryMax=std::max(ordinaryMax,diff);
            }
        }
        std::cout << "sleep wrap/ordinary body " << body << " mode " << mode << ": " << wrapMax << "/" << ordinaryMax << "\n";
        assert(wrapMax <= ordinaryMax * 1.5 + 1);
    }
    }
    assert(totals[0]>0 && totals[1]<totals[0]*0.65 && totals[2]==0);
    std::cout << "Asleep RGB565 temporal difference full/reduced/zero: " << totals[0] << "/" << totals[1] << "/" << totals[2] << "\n";
    assert(render({Bot::Mood::Asleep, "Asleep", false}, 120, 0) != render({Bot::Mood::Sleepy, "Sleepy", false}, 120, 0));
}

static void benchmarkEyes() {
    using Bot = botux::BotUx;
    for (int size : {40, 72, 200}) for (auto expr : {Bot::Expression::Neutral, Bot::Expression::Joy}) {
        M5Canvas canvas(size, size); canvas.setRecording(false);
        Bot bot; bot.begin(&canvas); bot.setExpression(expr, 0); bot.setMotionAmount(0);
        bot.update(gNow);
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 500; ++i) bot.draw();
        double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / 500;
        std::cout << "raster host " << size << " " << Bot::expressionName(expr) << ": " << us << " us/frame\n";
    }
}

static bool writeGazeSheet(const char* path) {
    using Bot = botux::BotUx; using G=Bot::GazeDirection;
    const G directions[]={G::UpLeft,G::Up,G::UpRight,G::Left,G::Center,G::Right,G::DownLeft,G::Down,G::DownRight};
    std::ofstream out(path); if (!out) return false;
    std::vector<uint16_t> mosaic(600*600);
    out << "<svg xmlns='http://www.w3.org/2000/svg' width='600' height='660'>";
    for (uint8_t index = 0; index < Bot::explicitGazeDirectionCount(); ++index) {
        gNow = 1000; M5Canvas canvas(200, 200); Bot bot; bot.begin(&canvas);
        bot.setExpression(Bot::Expression::Neutral, 0); bot.setMotionAmount(0);
        bot.setGazeDirection(directions[index]); bot.update(gNow); bot.draw();
        out << "<g transform='translate(" << (index%3) * 200 << " " << (index/3)*220 << ")'>" << canvas.svgBody()
            << "<text x='100' y='214' text-anchor='middle' font-family='sans-serif' font-size='12'>"
            << Bot::gazeDirectionName(directions[index]) << "</text></g>";
        for(int y=0;y<200;++y) for(int x=0;x<200;++x)
            mosaic[((index/3)*200+y)*600+(index%3)*200+x]=canvas.readPixel(x,y);
    }
    out << "</svg>";
    std::ofstream ppm(std::string(path)+".ppm", std::ios::binary); if(!ppm) return false;
    ppm<<"P6\n600 600\n255\n";
    for(uint16_t pixel:mosaic) {
        char rgb[3]={(char)(((pixel>>11)&31)*255/31),(char)(((pixel>>5)&63)*255/63),(char)((pixel&31)*255/31)};
        ppm.write(rgb,3);
    }
    return true;
}

static void writeCatalogFrames(const std::string& directory) {
    using Bot = botux::BotUx;
    std::ofstream manifest(directory + "/catalog.tsv");
    const int counts[] = {Bot::moodCount(), Bot::expressionCount(), Bot::animationCount()};
    const char* groups[] = {"Mood", "Expression", "Animation"};
    for (int group = 0; group < 3; ++group) for (int index = 0; index < counts[group]; ++index) {
        std::string id = std::string(groups[group]) + "-" + std::to_string(index);
        const char* name = group == 0 ? Bot::moodName((Bot::Mood)index) : group == 1 ?
            Bot::expressionName((Bot::Expression)index) : Bot::animationName((Bot::Animation)index);
        const char* zh = group == 0 ? Bot::moodName((Bot::Mood)index, Bot::Language::Chinese) : group == 1 ?
            Bot::expressionName((Bot::Expression)index, Bot::Language::Chinese) : Bot::animationName((Bot::Animation)index, Bot::Language::Chinese);
        manifest << id << "\t" << groups[group] << "\t" << name << "\t" << zh << "\n";
        std::ofstream raw(directory + "/" + id + ".rgb", std::ios::binary);
        gNow = 1000; M5Canvas canvas(120,120); canvas.setRecording(false); Bot bot; bot.begin(&canvas);
        bot.seedBlink(1234);
        if (group == 0) { bot.setMood((Bot::Mood)index, 0); bot.setTalking(index == (int)Bot::Mood::Speaking); }
        if (group == 1) bot.setExpression((Bot::Expression)index, 0);
        if (group == 2) bot.setAnimation((Bot::Animation)index);
        for (int frame = 0; frame < 72; ++frame) {
            gNow = 1000 + frame * 100; bot.update(gNow); canvas.clear(); bot.draw();
            for(int y=0;y<120;++y) for(int x=0;x<120;++x) {
                uint16_t pixel = canvas.readPixel(x,y);
                char rgb[3] = {(char)(((pixel>>11)&31)*255/31), (char)(((pixel>>5)&63)*255/63), (char)((pixel&31)*255/31)};
                raw.write(rgb,3);
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc > 2 && std::string(argv[2]) == "--catalog") { writeCatalogFrames(argv[1]); return 0; }
    assert(botux::BotUx().style().eyeColor == botux::rgb565(32, 36, 41));
    checkCompanionSemantics();
    checkTemporaryGaze();
    checkConnectedEyeMorphs();
    checkHappyDoneSemantics();
    checkPersistentAnimationWindows();
    checkGazeDirectionsAndMotionZero();
    checkDirectionsDominateEveryFace();
    checkReducedMotionAmplitude();
    checkNineDirectionPerspective();
    checkIdleAndUpperRightProportions();
    checkExpressionHandedness();
    checkEyesStayInsideBody();
    checkLookingAroundRange();
    checkSideArcTilt();
    checkDirectionTransitionContinuity();
    checkSleepContinuity();
    benchmarkEyes();
    checkCoverageAndFacePlacement();
    checkExplicitNeutralOverridesRestingMood();
    checkBodylessStateGlyphs();
    checkLateUptimeHasNoPhantomReaction();
    checkWaitingAndSleepyStayDistinct();
    checkExpressionAndAnimationRange();
    checkTransitionsMotionAndSparseFrames();
    checkMotionStaysInCanvas();
    const char* outDir = (argc > 1) ? argv[1] : ".";
    writeIdleComparison(std::string(outDir) + "/idle-up-right.ppm");
    writeHappyDoneThemeComparison(std::string(outDir) + "/happy-done-themes.ppm");
    writeHappyDoneSmallComparison(std::string(outDir) + "/happy-done-small.ppm");
    std::string tiny = std::string(outDir) + "/moods-40.svg";
    std::string small = std::string(outDir) + "/moods-72.svg";
    std::string large = std::string(outDir) + "/moods-200.svg";
    std::string expressions = std::string(outDir) + "/expressions.svg";
    std::string animations = std::string(outDir) + "/animations.svg";
    std::string player = std::string(outDir) + "/animation-player.html";
    if (!writeGazeSheet((std::string(outDir) + "/gaze-directions.svg").c_str()) ||
        !writeSheet(tiny.c_str(), 40) || !writeSheet(small.c_str(), 72) ||
        !writeSheet(large.c_str(), 200) ||
        !writePickerSheet(expressions.c_str(), kExpressions, renderExpression) ||
        !writePickerSheet((std::string(outDir) + "/expressions-40.svg").c_str(), kExpressions, renderExpression, 40) ||
        !writePickerSheet(animations.c_str(), kAnimations,
            [](const AnimationCase& item, int size) { return renderAnimation(item, size); }) ||
        !writeAnimationPlayer(player.c_str()) ||
        !writeTransitionSheet((std::string(outDir) + "/transitions.svg").c_str())) {
        std::cerr << "could not write preview sheets\n";
        return 1;
    }
    std::cout << tiny << "\n" << small << "\n" << large << "\n"
              << expressions << "\n" << animations << "\n" << player << "\n";
    return 0;
}

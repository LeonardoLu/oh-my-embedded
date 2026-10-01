#include <M5Unified.h>

#include "AudioFeedback.h"
#include "AnalogInput.h"
#include "BatteryDoubleTap.h"
#include "BotUx.h"
#include "BottomLeds.h"
#include "CodexLink.h"
#include "FeedbackLevel.h"
#include "FreshReplyAttention.h"
#include "Settings.h"
#include "AgentSignal.h"
#include "AgentCardPalette.h"
#include <UxText.h>
#include <UxRender.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr int16_t kScreenW = 320;
constexpr int16_t kScreenH = 240;
constexpr int16_t kBotSize = 40;
constexpr uint32_t kFrameMs = 33;
constexpr uint16_t kInk = botux::rgb565(31, 34, 39);
constexpr uint16_t kWhite = botux::rgb565(255, 255, 255);
constexpr uint16_t kGreen = botux::rgb565(39, 164, 91);
constexpr uint32_t kControlColor[6] = {
    0x3C7CF0, // Fast: configurable action whose state is unknown locally.
    0x27A45B, // Approve
    0xD73535, // Decline
    0x8759D6, // Fork
    0xE8972F, // Push to talk
    0x168FA0, // Send
};

enum class Page : uint8_t { Agents, Control };
enum class TargetType : uint8_t {
    None, Battery, PageAgents, PageControl, Agent, Command, Reasoning, Joystick
};
struct Target {
    Target(TargetType targetType = TargetType::None, int8_t targetIndex = -1)
        : type(targetType), index(targetIndex) {}
    TargetType type;
    int8_t index;
};

M5Canvas canvas(&M5.Display);
M5Canvas botSprite(&M5.Display);
botux::BotUx bot;
Settings settings;
AudioFeedback audio;
BottomLeds bottomLeds;
CodexLink codexLink;
BatteryDoubleTap batteryDoubleTap;
agentsignal::Model agentSignals;
freshreply::Attention freshReplyAttention;
uint32_t agentRevision = 0;
uint32_t alertUntil[6]{};
uint8_t freshReplyMask = 0;
Page page = Page::Agents;
Target pressed;
bool touching = false;
bool screenTouchGesture = false;
bool listening = false;
bool pttSent = false;
bool uiDirty = true;
uint8_t selectedAgent = 0;
uint32_t nowMs = 0;
uint32_t lastFrameMs = 0;
uint32_t lastPowerMs = 0;
int8_t battery = -1;
bool charging = false;
Settings::Data appliedSettings{};
CodexLink::State lastLinkState = CodexLink::State::Starting;
bool lastControlReady = false;
uint32_t lastLightingRevision = 0;
float lastAx = 0.0f;
float lastAy = 0.0f;
float lastAz = 1.0f;
uint32_t statsStartedMs = 0;
uint32_t frameCount = 0;
uint32_t fullPushCount = 0;
uint32_t drawMaxUs = 0;
uint32_t pushMaxUs = 0;
char serialCommand[16]{};
uint8_t serialLength = 0;
bool joystickMoved = false;
bool joystickActive = false;
float joystickAngle = 0.0f;
uint32_t lastJoystickMs = 0;
bool captureRequested = false;

void coreText(const char* text, int x, int y, const ux::Font& font = ux::Latin14)
{
    unsigned datum = (unsigned)canvas.getTextDatum();
    int width = ux::textWidth(text, font);
    if ((datum & 3) == 1) x -= width / 2;
    else if ((datum & 3) == 2) x -= width;
    if ((datum & 12) == 4) y -= ux::lineHeight(font) / 2;
    else if ((datum & 12) == 8) y -= ux::lineHeight(font);
    uint32_t rgb = canvas.getTextStyle().fore_rgb888;
    ux::drawText(canvas, text, x, y, ((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x7E0) | ((rgb >> 3) & 31), font);
}

uint16_t contrastingInk(uint32_t rgb)
{
    float values[3] = { ((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f };
    for (auto& value : values) value = value <= 0.04045f ? value / 12.92f : powf((value + 0.055f) / 1.055f, 2.4f);
    float luminance = values[0]*0.2126f + values[1]*0.7152f + values[2]*0.0722f;
    return luminance > 0.195f ? botux::rgb565(17, 22, 29) : kWhite;
}

bool sameTarget(const Target& a, const Target& b)
{
    return a.type == b.type && a.index == b.index;
}

Target hitTarget(int16_t x, int16_t y)
{
    if (x < 0 || x >= kScreenW || y < 0 || y >= kScreenH) return {};
    if (y < 40 && x >= 262) return {TargetType::Battery, 0};
    if (y >= 200) return {x < 160 ? TargetType::PageAgents : TargetType::PageControl, 0};
    if (page == Page::Agents && y >= 44 && y < 174)
    {
        const int8_t col = x / 106;
        const int8_t row = (y - 44) / 65;
        const int8_t index = row * 3 + col;
        if (col < 3 && index < LightingState::kSlotCount
            && x >= 4 + col*106 && x < 104 + col*106
            && y >= 44 + row*65 && y < 104 + row*65) return {TargetType::Agent, index};
    }
    if (page == Page::Control)
    {
        if ((y >= 48 && y < 88) || (y >= 92 && y < 132))
        {
            const int8_t row = y >= 92 ? 1 : 0;
            return {TargetType::Command, static_cast<int8_t>(row * 3 + x / 106)};
        }
        if (y >= 140 && y < 190)
        {
            if (x < 64) return {TargetType::Reasoning, 0};
            if (x >= 256) return {TargetType::Reasoning, 1};
            return {TargetType::Joystick, 0};
        }
    }
    return {};
}

uint16_t foreground() { return Settings::themePalette(settings.data().theme).text; }
uint16_t surface() { return Settings::themePalette(settings.data().theme).surface; }
uint16_t raisedSurface() { return Settings::themePalette(settings.data().theme).surfaceRaised; }
uint16_t outline() { return Settings::themePalette(settings.data().theme).outline; }
uint16_t mutedText() { return Settings::themePalette(settings.data().theme).muted; }
uint16_t rgb24to565(uint32_t color)
{
    return botux::rgb565(static_cast<uint8_t>(color >> 16),
                         static_cast<uint8_t>(color >> 8), static_cast<uint8_t>(color));
}

uint32_t controlColor(uint8_t index)
{
    if (index == 0 && codexLink.lighting().keys.active()
        && codexLink.lighting().keys.color)
        return codexLink.lighting().keys.color;
    return kControlColor[index < 6 ? index : 0];
}

void setBotMood()
{
    auto style = settings.botStyle();
    const auto& signal = agentSignals.slot(selectedAgent);
    if (codexLink.controlReady() && codexLink.threadLightingFresh() && signal.zone.color) {
        style.bodyColor = rgb24to565(signal.zone.color);
        style.eyeColor = contrastingInk(signal.zone.color);
        style.pupilColor = style.eyeColor == kWhite ? kInk : kWhite;
    }
    bot.setStyle(style);
    bot.setName(settings.data().botName);
    using Mood = botux::BotUx::Mood;
    Mood mood = Mood::Idle;
    if (listening) mood = Mood::Listening;
    else if (!codexLink.connected()) mood = Mood::Sleepy;
    else if (!codexLink.controlReady()) mood = Mood::Waiting;
    else switch (signal.signal) {
        case agentsignal::Signal::Working: mood = Mood::Working; break;
        case agentsignal::Signal::NeedsInput: mood = Mood::Waiting; break;
        case agentsignal::Signal::NewReply: mood = Mood::Happy; break;
        case agentsignal::Signal::Error: mood = Mood::Blocked; break;
        default: break;
    }
    bot.setMood(mood, 650);
}

void syncSettings()
{
    const Settings::Data& data = settings.data();
    if (memcmp(&data, &appliedSettings, sizeof(data)) == 0) return;
    const bool volumeChanged = data.volume != appliedSettings.volume;
    settings.apply(bot);
    audio.setVolume(corefeedback::synthVolume(data.volume));
    audio.setEnabled(data.audio != 0);
    bottomLeds.setBrightness(data.ledBrightness);
    bottomLeds.setMode(data.ledMode);
    if (!data.notifications) {
        for (auto& until : alertUntil) until = 0;
        freshReplyAttention.dismiss();
        bottomLeds.notify(0, nowMs);
    }
    // Apply both gain stages and mute first, then audition the newly selected
    // nonzero level. Muted and level-zero settings remain silent.
    if (volumeChanged && data.audio && data.volume) audio.select();
    setBotMood();
    appliedSettings = data;
    uiDirty = true;
}

void dismissFreshReplies()
{
    if (!freshReplyAttention.mask(nowMs)) return;
    freshReplyAttention.dismiss();
    freshReplyMask = 0;
    uiDirty = true;
}

bool sendTap(const char* key, uint32_t feedbackColor = 0)
{
    if (!codexLink.tapKey(key, selectedAgent)) { audio.error(); return false; }
    audio.action();
    bot.poke();
    if (feedbackColor) bottomLeds.control(nowMs);
    return true;
}

void activate(const Target& target)
{
    static const char* actionKeys[] = {"ACT06", "ACT07", "ACT08", "ACT09", "ACT10", "ACT12"};
    switch (target.type)
    {
        case TargetType::Battery:
            if (batteryDoubleTap.tap(nowMs)) { settings.open(); audio.select(); }
            break;
        case TargetType::PageAgents: page = Page::Agents; audio.select(); break;
        case TargetType::PageControl: page = Page::Control; audio.select(); break;
        case TargetType::Agent:
        {
            selectedAgent = static_cast<uint8_t>(target.index);
            bottomLeds.interact(selectedAgent, nowMs);
            char key[] = "AG00";
            key[3] = static_cast<char>('0' + selectedAgent);
            sendTap(key);
            break;
        }
        case TargetType::Command:
            if (target.index == 4) { if (pttSent) audio.action(); }
            else if (target.index >= 0 && target.index < 6)
                sendTap(actionKeys[target.index], controlColor(target.index));
            break;
        case TargetType::Reasoning:
            if (target.index >= 0)
            {
                if (codexLink.turnEncoder(target.index == 0)) { audio.select(); bot.poke(); }
                else audio.error();
            }
            break;
        case TargetType::Joystick:
            if (codexLink.tapKey("ENC_PRESS", selectedAgent)) { audio.select(); bot.poke(); }
            else audio.error();
            break;
        default: break;
    }
    setBotMood();
    uiDirty = true;
}

void finishPtt()
{
    if (pttSent) codexLink.releaseKey("ACT10", selectedAgent);
    bottomLeds.hold(false, nowMs);
    pttSent = false;
    listening = false;
    setBotMood();
}

void touchBegin(int16_t x, int16_t y)
{
    touching = true;
    screenTouchGesture = x >= 0 && x < kScreenW && y >= 0 && y < kScreenH;
    if (screenTouchGesture) dismissFreshReplies();
    if (settings.isOpen())
    {
        batteryDoubleTap.cancel();
        settings.touchBegin(x, y);
        syncSettings();
        uiDirty = true;
        return;
    }
    pressed = hitTarget(x, y);
    if (pressed.type != TargetType::Battery) batteryDoubleTap.cancel();
    joystickMoved = false;
    joystickActive = false;
    if (pressed.type == TargetType::Command && pressed.index == 4)
    {
        pttSent = codexLink.pressKey("ACT10", selectedAgent);
        listening = pttSent;
        if (pttSent)
        {
            bottomLeds.hold(true, nowMs);
            audio.select();
        }
        else audio.error();
        setBotMood();
    }
    uiDirty = true;
}

void touchMove(int16_t x, int16_t y)
{
    if (!touching) return;
    if (settings.isOpen())
    {
        if (settings.touchMove(x, y))
        {
            syncSettings();
            uiDirty = true;
        }
        return;
    }
    if (pressed.type == TargetType::Joystick)
    {
        const float dx = static_cast<float>(x - 160);
        const float dy = static_cast<float>(y - 163);
        const AnalogPosition position = AnalogInput::fromOffset(dx, dy, 45.0f, 7.0f);
        if (position.active)
        {
            joystickAngle = position.angle;
            if (!joystickMoved || nowMs - lastJoystickMs >= 45)
            {
                if (codexLink.moveJoystick(joystickAngle, position.distance))
                    joystickActive = true;
                lastJoystickMs = nowMs;
            }
            joystickMoved = true;
        }
        else if (joystickActive)
        {
            codexLink.moveJoystick(joystickAngle, 0);
            joystickActive = false;
        }
        return;
    }
    if (pressed.type != TargetType::None && !sameTarget(pressed, hitTarget(x, y)))
    {
        if (pttSent) finishPtt();
        if (pressed.type == TargetType::Battery) batteryDoubleTap.cancel();
        pressed = {};
        uiDirty = true;
    }
}

void touchEnd(int16_t x, int16_t y)
{
    if (!touching) return;
    if (settings.isOpen())
    {
        settings.touchEnd(x, y);
        touching = false;
        screenTouchGesture = false;
        syncSettings();
        uiDirty = true;
        return;
    }
    const Target released = hitTarget(x, y);
    const bool valid = sameTarget(pressed, released);
    const bool wasPtt = pressed.type == TargetType::Command && pressed.index == 4;
    const bool wasJoystick = pressed.type == TargetType::Joystick;
    if (pttSent) finishPtt();
    if (wasJoystick && joystickActive)
    {
        codexLink.moveJoystick(joystickAngle, 0);
        joystickActive = false;
    }
    if (valid)
    {
        if (wasPtt) { audio.action(); bot.poke(); }
        else if (wasJoystick && joystickMoved) audio.select();
        else activate(pressed);
    }
    touching = false;
    screenTouchGesture = false;
    pressed = {};
    uiDirty = true;
}

bool handleTouch()
{
    const auto detail = M5.Touch.getDetail(0);
    if (detail.wasPressed()) { touchBegin(detail.x, detail.y); return screenTouchGesture; }
    if (detail.isPressed()) { touchMove(detail.x, detail.y); return screenTouchGesture; }
    if (detail.wasReleased())
    {
        const bool screenEvent = screenTouchGesture;
        touchEnd(detail.x, detail.y);
        return screenEvent;
    }
    return false;
}

void handleButtons(bool screenTouchEvent)
{
    if (screenTouchEvent || touching) return;
    const bool buttonA = M5.BtnA.wasClicked();
    const bool buttonB = M5.BtnB.wasClicked();
    const bool buttonC = M5.BtnC.wasClicked();
    if (buttonA || buttonB || buttonC) dismissFreshReplies();
    if (settings.isOpen())
    {
        if (buttonB)
        {
            batteryDoubleTap.cancel();
            settings.close();
            audio.select();
            uiDirty = true;
        }
        return;
    }
    if (buttonA)
    {
        batteryDoubleTap.cancel();
        selectedAgent = (selectedAgent + 5) % 6;
        setBotMood(); bottomLeds.interact(selectedAgent, nowMs);
        audio.select();
        uiDirty = true;
    }
    else if (buttonB)
    {
        batteryDoubleTap.cancel();
        page = page == Page::Agents ? Page::Control : Page::Agents;
        audio.select();
        uiDirty = true;
    }
    else if (buttonC)
    {
        batteryDoubleTap.cancel();
        selectedAgent = (selectedAgent + 1) % 6;
        setBotMood(); bottomLeds.interact(selectedAgent, nowMs);
        audio.select();
        uiDirty = true;
    }
}

void drawHeader()
{
    botSprite.pushSprite(&canvas, 0, 0);
    const auto style = Settings::themeStyle(settings.data().theme);
    const uint16_t ble = codexLink.connected() ? style.accentColor : mutedText();
    const uint16_t app = codexLink.controlReady() ? kGreen : mutedText();
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(foreground());
    // Keep the group at the right edge; long names are fitted in the remaining space.
    char name[18]; snprintf(name, sizeof(name), "%s", settings.data().botName);
    while (strlen(name) > 1 && ux::textWidth(name, ux::Latin14) > 141) name[strlen(name)-1] = 0;
    coreText(name, 46, 20);
    ux::line(canvas, 207, 9, 207, 30, 1.8f, ble);
    ux::line(canvas, 207, 9, 215, 16, 1.8f, ble);
    ux::line(canvas, 215, 16, 200, 27, 1.8f, ble);
    ux::line(canvas, 207, 30, 215, 23, 1.8f, ble);
    ux::line(canvas, 215, 23, 200, 12, 1.8f, ble);
    ux::strokeRoundRect(canvas, 229, 11, 24, 17, 3, app, 1.8f);
    ux::line(canvas, 241, 28, 241, 31, 1.8f, app);
    ux::line(canvas, 235, 32, 247, 32, 1.8f, app);
    uint16_t power = charging ? kGreen : foreground();
    if (pressed.type == TargetType::Battery) power = style.accentColor;
    ux::strokeRoundRect(canvas, 270, 9, 41, 24, 4, power, 1.5f);
    ux::roundRect(canvas, 312, 16, 3, 10, 1, power);
    char value[5];
    if (battery < 0) snprintf(value, sizeof(value), "--");
    else snprintf(value, sizeof(value), "%d", battery);
    canvas.setTextDatum(middle_center); canvas.setTextColor(power);
    coreText(value, 291, 21);
}

void drawAgentCard(uint8_t index)
{
    const int16_t x = 4 + (index % 3) * 106, y = 44 + (index / 3) * 65;
    const auto& state = agentSignals.slot(index);
    const agentcard::Colors identity = agentcard::colors(settings.data().theme, index);
    uint16_t fill = rgb24to565(identity.fill);
    const uint16_t ink = rgb24to565(identity.ink);
    const uint32_t statusRgb = state.signal == agentsignal::Signal::Unknown
        ? 0x626A76 : state.zone.color;
    const uint16_t statusColor = rgb24to565(statusRgb);
    bool down = pressed.type == TargetType::Agent && pressed.index == index;
    const bool freshReply = freshReplyMask & (1u << index);
    if (freshReply) fill = ux::blend565(fill, kWhite, 30);
    if (alertUntil[index] && (int32_t)(alertUntil[index] - nowMs) > 0 && !settings.data().reducedMotion) {
        float t = (1600 - (alertUntil[index] - nowMs)) / 1600.0f;
        float envelope = sinf(3.14159265f * t);
        fill = ux::blend565(fill, statusColor, (uint8_t)(envelope * 54));
    }
    if (down) fill = ux::blend565(fill, ink, 48);
    ux::roundRect(canvas, x, y, 100, 60, 9, fill);
    if (freshReply) ux::strokeRoundRect(canvas, x, y, 100, 60, 9, statusColor, 2.8f);
    if (index == selectedAgent) ux::strokeRoundRect(canvas, x+2, y+2, 96, 56, 7, ink, 1.7f);
    ux::circle(canvas, x+14, y+13, 8, ink);
    ux::circle(canvas, x+14, y+13, 5, statusColor);
    ux::circle(canvas, x+85, y+13, 9, ux::blend565(fill, ink, 26));
    char badge[2] = {(char)('1'+index),0};
    canvas.setTextDatum(middle_center); canvas.setTextColor(ink);
    coreText(badge, x+85, y+13);
    const char* first = agentsignal::name(state.signal);
    const char* second = nullptr;
    if (state.signal == agentsignal::Signal::NeedsInput) { first = "Needs"; second = "input"; }
    if (state.signal == agentsignal::Signal::NewReply) { first = "New"; second = "reply"; }
    if (state.signal == agentsignal::Signal::Off) { first = "Lights"; second = "off"; }
    coreText(first, x+50, y+(second ? 28 : 37));
    if (second) coreText(second, x+50, y+46);
}

void drawTabs()
{
    const uint16_t accent = Settings::themeStyle(settings.data().theme).accentColor;
    if (page == Page::Agents) canvas.fillRect(0, 201, 160, 39, raisedSurface());
    else canvas.fillRect(160, 201, 160, 39, raisedSurface());
    if (pressed.type == TargetType::PageAgents) canvas.fillRect(0, 201, 160, 39, surface());
    if (pressed.type == TargetType::PageControl) canvas.fillRect(160, 201, 160, 39, surface());
    canvas.drawFastHLine(0, 200, 320, accent);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(foreground());
    coreText("<", 34, 220);
    coreText(page == Page::Agents ? "1 / 2" : "2 / 2", 160, 220);
    coreText(">", 286, 220);
}

void drawAgentsPage()
{
    for (uint8_t i = 0; i < LightingState::kSlotCount; ++i) drawAgentCard(i);
    char detail[160];
    bot.describe(detail, sizeof(detail), settings.data().language ? botux::BotUx::Language::Chinese : botux::BotUx::Language::English);
    const ux::Font& font = settings.data().language ? ux::Cjk18 : ux::Latin14;
    // UTF-8-safe fitting; one-line caption fits between cards and pager.
    while (ux::textWidth(detail, font) > 306 && strlen(detail)) {
        size_t len = strlen(detail) - 1;
        while (len && ((unsigned char)detail[len] & 0xC0) == 0x80) --len;
        detail[len] = 0;
    }
    canvas.setTextDatum(middle_center); canvas.setTextColor(mutedText());
    coreText(detail, 160, 184, font);
    drawTabs();
}

void drawKey(int16_t x, int16_t y, int16_t w, const char* label, const char* code,
             bool down, uint32_t semanticColor)
{
    const uint16_t color = rgb24to565(semanticColor);
    const uint16_t fill = down ? color : ux::blend565(surface(), color, 92);
    const uint16_t ink = down ? contrastingInk(semanticColor) : foreground();
    ux::roundRect(canvas, x, y, w, 40, 8, fill);
    ux::strokeRoundRect(canvas, x, y, w, 40, 8, down ? color : ux::blend565(outline(), color, 96));
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(ink);
    coreText(label, x + w / 2, y + 12);
    canvas.setTextColor(down ? ink : ux::blend565(mutedText(), color, 52));
    coreText(code, x + w / 2, y + 29);
}

void drawControlPage()
{
    static const char* labels[] = {"FAST", "APPROVE", "DECLINE", "FORK", "HOLD MIC", "SEND"};
    static const char* hints[] = {"TOGGLE", "CLICK", "CLICK", "CLICK", "HOLD / MAC", "CLICK"};
    for (uint8_t i = 0; i < 6; ++i)
    {
        const int16_t x = 4 + (i % 3) * 106;
        const int16_t y = i < 3 ? 48 : 92;
        const bool down = pressed.type == TargetType::Command && pressed.index == i;
        const char* label = i == 4 && listening ? "MIC ACTIVE" : labels[i];
        const char* hint = i == 4 && listening ? "RELEASE" : hints[i];
        drawKey(x, y, 100, label, hint, down || (i == 4 && listening), controlColor(i));
    }
    const bool lessDown = pressed.type == TargetType::Reasoning && pressed.index == 0;
    const bool moreDown = pressed.type == TargetType::Reasoning && pressed.index == 1;
    drawKey(4, 143, 58, "LESS", "TURN", lessDown, 0x697386);
    ux::roundRect(canvas, 66, 143, 188, 40, 8, surface());
    ux::strokeRoundRect(canvas, 66, 143, 188, 40, 8, outline());
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(foreground());
    coreText("TAP KNOB / DRAG STICK", 160, 163);
    drawKey(258, 143, 58, "MORE", "TURN", moreDown, 0x697386);
    drawTabs();
}

void readPower()
{
    battery = M5.Power.getBatteryLevel();
    charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
}

void updateMotion()
{
    float ax = 0.0f, ay = 0.0f, az = 1.0f;
    if (!M5.Imu.getAccel(&ax, &ay, &az)) return;
    const float delta = fabsf(ax - lastAx) + fabsf(ay - lastAy) + fabsf(az - lastAz);
    float shake = (delta - 0.12f) * 2.5f;
    if (shake < 0.0f) shake = 0.0f;
    if (shake > 1.0f) shake = 1.0f;
    bot.setMotion(ay, -ax, shake);
    lastAx = ax; lastAy = ay; lastAz = az;
}

void writeLedFrame()
{
    const bool ready = codexLink.controlReady() && codexLink.threadLightingFresh();
    Serial.printf("[ledframe] ready=%u theme=%u mode=%u brightness=%u fresh=0x%02X\n",
                  ready, settings.data().theme, settings.data().ledMode,
                  settings.data().ledBrightness, freshReplyMask);
    for (uint8_t slot = 0; slot < LightingState::kSlotCount; ++slot)
    {
        const auto color = bottomLeds.output(bottomled::kAgentLed[slot]);
        const auto signal = agentsignal::decode(codexLink.lighting().slots[slot], ready);
        Serial.printf("[ledframe] slot=%u signal=%s host_brightness=%u led=%u rgb=%02X%02X%02X\n",
                      slot + 1, agentsignal::name(signal),
                      codexLink.lighting().slots[slot].brightness,
                      bottomled::kAgentLed[slot], color.r, color.g, color.b);
    }
    Serial.print("[ledframe] ambient=");
    for (uint8_t led = 3; led <= 6; ++led)
    {
        const auto color = bottomLeds.output(led);
        Serial.printf("%s%02X%02X%02X", led == 3 ? "" : ",", color.r, color.g, color.b);
    }
    Serial.println();
}

void handleSerial()
{
    while (Serial.available())
    {
        const char value = static_cast<char>(Serial.read());
        if (value == '\n' || value == '\r')
        {
            if (serialLength)
            {
                serialCommand[serialLength] = '\0';
                bool sent = false;
                int settingsX, settingsY;
                if (strcmp(serialCommand,"sound")==0) {
                    audio.confirm(); audio.update();
                    Serial.printf("[sound] preview=confirm ready=%u busy=%u failures=%lu\n",audio.available(),audio.busy(),static_cast<unsigned long>(audio.failures()));
                    serialLength=0; return;
                }
                if (strcmp(serialCommand, "leds") == 0) {
                    writeLedFrame();
                    serialLength = 0; return;
                }
                if (sscanf(serialCommand,"ui-tap %d %d",&settingsX,&settingsY)==2) {
                    if(settings.isOpen()) {
                        dismissFreshReplies();
                        settings.touchBegin(settingsX,settingsY); settings.touchEnd(settingsX,settingsY); syncSettings(); uiDirty=true;
                    }
                    Serial.printf("[settings] open=%u theme=%u volume=%u gain=%u speaker=%u audio=%u busy=%u failures=%lu\n",settings.isOpen(),settings.data().theme,settings.data().volume,audio.volume(),audio.speakerVolume(),settings.data().audio,audio.busy(),static_cast<unsigned long>(audio.failures()));
                    serialLength=0; return;
                }

                if (strcmp(serialCommand, "next") == 0) sent = codexLink.turnEncoder(false);
                else if (strcmp(serialCommand, "prev") == 0) sent = codexLink.turnEncoder(true);
                else if (strcmp(serialCommand, "c") == 0)
                {
                    captureRequested = true;
                    uiDirty = true;
                    serialLength = 0;
                    return;
                }
                else if (serialLength == 1 && serialCommand[0] >= '0' && serialCommand[0] <= '9')
                {
                    if (serialCommand[0] == '0') { settings.close(); page = Page::Agents; }
                    else if (serialCommand[0] == '1') { settings.close(); page = Page::Control; }
                    else settings.open(static_cast<uint8_t>(serialCommand[0] - '2'));
                    uiDirty = true;
                    serialLength = 0;
                    return;
                }
                Serial.printf("[hid] command=%s sent=%s ready=%u\n", serialCommand,
                              sent ? "yes" : "no", codexLink.controlReady());
                serialLength = 0;
            }
        }
        else if (serialLength + 1 < sizeof(serialCommand)) serialCommand[serialLength++] = value;
        else serialLength = 0;
    }
}

void writeFrameCapture()
{
    const uint8_t* pixels = static_cast<const uint8_t*>(canvas.getBuffer());
    Serial.printf("FRAME 320 240 RGB565BE 153600\n");
    for (size_t offset = 0; offset < 320u * 240u * 2u; offset += 2048)
    {
        const size_t remaining = 320u * 240u * 2u - offset;
        Serial.write(pixels + offset, remaining < 2048 ? remaining : 2048);
    }
}

void failSprite(const char* message)
{
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_RED);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString(message, 160, 120);
    while (true) delay(1000);
}

void reportPerformance()
{
    const uint32_t elapsed = nowMs - statsStartedMs;
    if (elapsed < 5000) return;
    const float fps = elapsed ? frameCount * 1000.0f / elapsed : 0.0f;
    Serial.printf("[perf] fps=%.2f draw_max_us=%lu push_max_us=%lu full=%lu frames=%lu "
                  "heap=%u psram=%u ble=%u ready=%u mtu=%u rpc=%lu events=%lu led=%u mode=%u sound=%u sound_fail=%lu\n",
                  fps, static_cast<unsigned long>(drawMaxUs), static_cast<unsigned long>(pushMaxUs),
                  static_cast<unsigned long>(fullPushCount), static_cast<unsigned long>(frameCount),
                  ESP.getFreeHeap(), ESP.getFreePsram(), codexLink.connected(), codexLink.controlReady(),
                  codexLink.peerMtu(), static_cast<unsigned long>(codexLink.receivedRpcCount()),
                  static_cast<unsigned long>(codexLink.sentEventCount()), bottomLeds.available(), settings.data().ledMode, audio.available(), static_cast<unsigned long>(audio.failures()));
    statsStartedMs = nowMs;
    frameCount = fullPushCount = drawMaxUs = pushMaxUs = 0;
}
}

void setup()
{
    Serial.begin(115200);
    auto config = M5.config();
    config.clear_display = true;
    config.internal_spk = true;
    config.internal_imu = true;
    M5.begin(config);
    audio.begin();
    M5.Display.setRotation(1);
    canvas.setColorDepth(16);
    if (!canvas.createSprite(kScreenW, kScreenH)) failSprite("DISPLAY BUFFER FAILED");
    botSprite.setColorDepth(16);
    if (!botSprite.createSprite(kBotSize, kBotSize)) failSprite("BOT BUFFER FAILED");
    canvas.setFont(&fonts::Font2);
    settings.begin();
    bot.begin(&botSprite);
    bot.seedBlink(static_cast<uint32_t>(micros()));
    bot.setBatteryVisible(false);
    bot.setSignal(-1);
    settings.apply(bot);
    audio.setVolume(corefeedback::synthVolume(settings.data().volume));
    audio.setEnabled(settings.data().audio != 0);
    bottomLeds.begin();
    bottomLeds.setBrightness(settings.data().ledBrightness);
    bottomLeds.setMode(settings.data().ledMode);
    appliedSettings = settings.data();
    readPower();
    const bool started = codexLink.begin();
    Serial.printf("[hid] start=%s local_mtu=67 name=Core2 Codex Micro\n",
                  started ? "ok" : "failed");
    setBotMood();
    statsStartedMs = millis();
}

void loop()
{
    nowMs = millis();
    M5.update();
    audio.update();
    handleSerial();
    codexLink.update(battery, charging, nowMs);
    uint8_t alerts = agentSignals.update(codexLink.lighting(),
        codexLink.controlReady() && codexLink.threadLightingFresh(), nowMs);
    freshReplyAttention.retain(freshreply::current(agentSignals));
    if (agentSignals.revision() != agentRevision) {
        agentRevision = agentSignals.revision(); setBotMood(); uiDirty = true;
    }
    for (auto& until : alertUntil) if (until && (int32_t)(nowMs - until) >= 0) { until = 0; uiDirty = true; }
    if (alerts && settings.data().notifications) {
        for (uint8_t i = 0; i < 6; ++i) if (alerts & (1u << i)) alertUntil[i] = nowMs + 1600;
        freshReplyAttention.start(freshreply::fromNotifications(alerts, agentSignals), nowMs);
        bottomLeds.notify(alerts, nowMs);
        bool error = false;
        for (uint8_t i = 0; i < 6; ++i) if ((alerts & (1u << i)) && agentSignals.slot(i).signal == agentsignal::Signal::Error) error = true;
        if (error) audio.error(); else audio.confirm();
    }
    const bool screenTouchEvent = handleTouch();
    handleButtons(screenTouchEvent);
    const uint8_t activeFreshReplies = freshReplyAttention.mask(nowMs);
    if (activeFreshReplies != freshReplyMask) {
        freshReplyMask = activeFreshReplies;
        uiDirty = true;
    }
    audio.update();
    if (codexLink.state() != lastLinkState || codexLink.controlReady() != lastControlReady)
    {
        lastLinkState = codexLink.state();
        lastControlReady = codexLink.controlReady();
        setBotMood();
        uiDirty = true;
        Serial.printf("[hid] ble=%u ready=%u mtu=%u\n", codexLink.connected(), codexLink.controlReady(), codexLink.peerMtu());
    }
    if (codexLink.lightingRevision() != lastLightingRevision)
    {
        lastLightingRevision = codexLink.lightingRevision();
        uiDirty = true;
    }
    if (nowMs - lastFrameMs < kFrameMs) { delay(1); return; }
    lastFrameMs = nowMs;
    if (nowMs - lastPowerMs >= 5000) { lastPowerMs = nowMs; readPower(); }
    syncSettings();
    const bool animatePreview = settings.animate(nowMs);
    if (!settings.isOpen() && page == Page::Agents && !settings.data().reducedMotion)
    {
        for (uint8_t i = 0; i < 6; ++i) if (alertUntil[i] && (int32_t)(alertUntil[i]-nowMs)>0) uiDirty = true;
    }
    updateMotion();
    bot.update(nowMs);
    const uint32_t drawStarted = micros();
    bot.draw();
    bottomLeds.update(codexLink.lighting(), nowMs, settings.data().reducedMotion != 0,
                      codexLink.controlReady() && codexLink.threadLightingFresh(),
                      selectedAgent, freshReplyMask, settings.data().theme);
    const bool fullFrame = uiDirty;
    const bool partialPreview = !fullFrame && animatePreview;
    if (fullFrame)
    {
        canvas.fillSprite(Settings::themeStyle(settings.data().theme).bgColor);
        if (settings.isOpen()) settings.draw(canvas, botSprite);
        else { drawHeader(); if (page == Page::Agents) drawAgentsPage(); else drawControlPage(); }
    }
    else if (partialPreview)
    {
        settings.drawAnimatedPreview(canvas);
    }
    else if (!settings.isOpen())
    {
        canvas.fillRect(0, 0, kScreenW, 40, Settings::themeStyle(settings.data().theme).bgColor);
        drawHeader();
    }
    const uint32_t drawUs = micros() - drawStarted;
    if (drawUs > drawMaxUs) drawMaxUs = drawUs;
    const uint32_t pushStarted = micros();
    if (fullFrame)
    {
        canvas.pushSprite(0, 0);
        ++fullPushCount;
        uiDirty = false;
    }
    else if (partialPreview)
    {
        const auto bounds = Settings::previewRect();
        M5.Display.setClipRect(bounds.x, bounds.y, bounds.w, bounds.h);
        canvas.pushSprite(0, 0);
        M5.Display.clearClipRect();
    }
    else if (!settings.isOpen())
    {
        M5.Display.pushImage(0, 0, kScreenW, 40, static_cast<uint16_t*>(canvas.getBuffer()));
    }
    const uint32_t pushUs = micros() - pushStarted;
    if (pushUs > pushMaxUs) pushMaxUs = pushUs;
    ++frameCount;
    if (captureRequested)
    {
        captureRequested = false;
        writeFrameCapture();
    }
    reportPerformance();
}

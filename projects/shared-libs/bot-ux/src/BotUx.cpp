// BotUx — animation implementation. See BotUx.h for the contract.
#include "BotUx.h"

#include <Arduino.h> // millis()
#include <math.h>
#include <stdio.h>

namespace botux {

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr uint32_t BLINK_CLOSE_MS = 90;
constexpr uint32_t BLINK_HOLD_MS  = 50;
constexpr uint32_t BLINK_OPEN_MS  = 100;
constexpr uint32_t SURPRISE_MS    = 240;
constexpr uint32_t HAPPY_MS       = 1180;

uint32_t blinkCloseMs(BotUx::Mood mood) {
    return (mood == BotUx::Mood::Sleepy) ? 180u : BLINK_CLOSE_MS;
}

uint32_t blinkHoldMs(BotUx::Mood mood) {
    return (mood == BotUx::Mood::Sleepy) ? 360u : BLINK_HOLD_MS;
}

uint32_t blinkOpenMs(BotUx::Mood mood) {
    return (mood == BotUx::Mood::Sleepy) ? 220u : BLINK_OPEN_MS;
}

int16_t min16(int16_t a, int16_t b) { return (a < b) ? a : b; }

float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float easeAlpha(uint32_t dt, float timeConstantMs) {
    return 1.0f - expf(-(float)dt / timeConstantMs);
}

uint16_t mix565(uint16_t a, uint16_t b, uint8_t amount) {
    uint16_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    uint16_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    uint16_t inv = 255u - amount;
    uint16_t r = (uint16_t)((ar * inv + br * amount) / 255u);
    uint16_t g = (uint16_t)((ag * inv + bg * amount) / 255u);
    uint16_t bl = (uint16_t)((ab * inv + bb * amount) / 255u);
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

BotUx::Metrics computeLayout(int16_t w, int16_t h, const BotUx::Style& s) {
    BotUx::Metrics m;
    int16_t side = min16(w, h);
    int16_t nominalR = (int16_t)(side * 0.39f);
    m.bodyR = (s.bodyStyle == BotUx::BodyStyle::None) ? 0 : nominalR;
    m.eyeRadius = (int16_t)(side * 0.045f * s.eyeSize);
    if (m.eyeRadius < 2) m.eyeRadius = 2;
    m.eyeDX = (int16_t)(nominalR * 0.22f);
    m.cx = w / 2;
    m.cy = (h > (int16_t)(side * 1.30f)) ? (int16_t)(h * 0.39f) : h / 2;
    m.eyeY = m.cy - (int16_t)(nominalR * 0.38f);
    m.mouthY = m.cy + (int16_t)(nominalR * 0.24f);
    return m;
}

uint32_t blinkDelay(const BotUx::Style& s, uint32_t seed) {
    uint32_t lo = s.blinkMinMs;
    uint32_t hi = (s.blinkMaxMs < lo) ? lo : s.blinkMaxMs;
    uint32_t span = hi - lo;
    return lo + (span ? (seed % (span + 1u)) : 0u);
}
} // namespace

void BotUx::begin(M5Canvas* canvas) {
    _cv = canvas;
    if (!_cv) return;
    _w = (int16_t)canvas->width();
    _h = (int16_t)canvas->height();
    _m = computeLayout(_w, _h, _style);
    _animStart = millis();
    _lastNow = 0;
    _nextBlink = _animStart + blinkDelay(_style, _blinkSeed);
    _nextGaze = _animStart + 700u + (_gazeSeed % 800u);
}

void BotUx::setStyle(const Style& s) {
    _style = s;
    _m = computeLayout(_w, _h, _style);
}

void BotUx::setMood(Mood m) { setMood(m, 180); }

void BotUx::setMood(Mood m, uint16_t transitionMs) {
    if (_mood == m) return;
    _mood = m;
    if (m == Mood::LookingAround) _nextGaze = _lastNow;
    _transitionMs = transitionMs;
    _snapPose = transitionMs == 0;
}

void BotUx::setExpression(Expression expression, uint16_t transitionMs) {
    if (_expression == expression) return;
    _expression = expression;
    _transitionMs = transitionMs;
    _snapPose = transitionMs == 0;
}

void BotUx::setAnimation(Animation animation) { _animation = animation; }

void BotUx::setAnimationSpeed(float speed) {
    _animationSpeed = clampf(speed, 0.25f, 3.0f);
}

void BotUx::setMotionAmount(float amount) {
    _motionAmount = clampf(amount, 0.0f, 2.0f);
}

void BotUx::setReducedMotion(bool reduced) { _reducedMotion = reduced; }

void BotUx::setMotion(float tiltX, float tiltY, float shake) {
    _motionTargetX = clampf(tiltX, -1.0f, 1.0f);
    _motionTargetY = clampf(tiltY, -1.0f, 1.0f);
    _shakeTarget = clampf(shake, 0.0f, 1.0f);
}

void BotUx::seedBlink(uint32_t s) {
    _blinkSeed = s ? s : 0x1234u;
    _presetSeed = _blinkSeed ^ 0xA341316Cu;
    _gazeSeed = _blinkSeed ^ 0x9E3779B9u;
    uint32_t now = millis();
    _nextBlink = now + blinkDelay(_style, _blinkSeed);
    _nextGaze = now + 700u + (_gazeSeed % 800u);
}

void BotUx::setTalking(bool on) {
    if (on && !_talking) _talkSeed = millis();
    _talking = on;
}

void BotUx::setBattery(uint8_t pct) {
    _battery = (pct > 100) ? 100 : pct;
    _batteryVisible = true;
}

void BotUx::setBatteryVisible(bool vis) { _batteryVisible = vis; }

void BotUx::setSignal(int8_t bars) {
    if (bars < -1) bars = -1;
    if (bars > 4) bars = 4;
    _signal = bars;
}

void BotUx::setTime(uint8_t h, uint8_t m, uint8_t s, bool pm) {
    _hasTime = true;
    _hh = h % 12;
    if (_hh == 0) _hh = 12;
    _mm = m % 60;
    _ss = s % 60;
    _pm = pm;
}

void BotUx::setLabel(const char* text) { _label = text; }

void BotUx::poke() {
    uint32_t now = millis();
    _pokeUntil = now + SURPRISE_MS;
    _reactionUntil = now + HAPPY_MS;
}

float BotUx::_pulse(uint32_t ms, float period, float phase) const {
    return 0.5f + 0.5f * sinf(2.0f * kPi * ms / period + phase);
}

void BotUx::update(uint32_t nowMs) {
    _now = nowMs ? nowMs : millis();
    if (!_cv) return;
    _updateBlink(_now);
    _updateGaze(_now);
    _updateTalk(_now);
    _resolveMood(_now);
    _lastNow = _now;
}

void BotUx::_updateBlink(uint32_t now) {
    if (!_blinking) {
        if ((int32_t)(now - _nextBlink) >= 0) {
            _blinking = true;
            _blinkStart = now;
            _blinkPhase = 0;
        }
        return;
    }

    uint32_t t = now - _blinkStart;
    uint32_t closeMs = blinkCloseMs(_effMood);
    uint32_t holdMs = blinkHoldMs(_effMood);
    uint32_t openMs = blinkOpenMs(_effMood);
    if (t < closeMs) _blinkPhase = 0;
    else if (t < closeMs + holdMs) _blinkPhase = 1;
    else if (t < closeMs + holdMs + openMs) _blinkPhase = 2;
    else {
        _blinking = false;
        _blinkSeed = _blinkSeed * 1103515245u + 12345u;
        _nextBlink = now + blinkDelay(_style, _blinkSeed);
    }
}

const char* BotUx::moodName(Mood value, Language language) {
    static const char* const en[] = {"Idle", "Listening", "Thinking", "Speaking", "Happy", "Sad", "Sleepy", "Surprised", "Working", "Waiting", "Blocked", "Done", "Asleep", "Looking around"};
    static const char* const zh[] = {"空闲", "聆听", "思考", "说话", "开心", "难过", "困倦", "惊讶", "工作", "等待", "受阻", "完成", "熟睡", "到处看看"};
    return (language == Language::Chinese ? zh : en)[(uint8_t)value < moodCount() ? (uint8_t)value : 0];
}
const char* BotUx::expressionName(Expression value, Language language) {
    static const char* const en[] = {"Auto", "Neutral", "Curious", "Focused", "Joy", "Skeptical", "Bashful", "Wink", "Dizzy", "Alarmed"};
    static const char* const zh[] = {"自动", "自然", "好奇", "专注", "喜悦", "怀疑", "害羞", "眨眼", "眩晕", "警觉"};
    return (language == Language::Chinese ? zh : en)[(uint8_t)value < expressionCount() ? (uint8_t)value : 0];
}
const char* BotUx::animationName(Animation value, Language language) {
    static const char* const en[] = {"Auto", "Calm", "Curious", "Orbit", "Bounce", "Glitch", "Wave", "Sparkle"};
    static const char* const zh[] = {"自动", "平静", "好奇", "环绕", "弹跳", "闪动", "波浪", "闪耀"};
    return (language == Language::Chinese ? zh : en)[(uint8_t)value < animationCount() ? (uint8_t)value : 0];
}
void BotUx::setName(const char* name) {
    size_t n = 0;
    if (name) for (; *name && n < kNameMax; ++name) {
        const unsigned char c = (unsigned char)*name;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '\'' || (c == ' ' && n))
            _name[n++] = (char)c;
    }
    while (n && _name[n - 1] == ' ') --n;
    _name[n] = 0;
    if (!n) { _name[0] = 'M'; _name[1] = 'i'; _name[2] = 'l'; _name[3] = 'o'; _name[4] = 0; }
}
size_t BotUx::describe(char* out, size_t capacity, Language language) const {
    if (!out) capacity = 0;
    static const char* const moodEn[] = {
        "is resting", "is listening", "is thinking", "is speaking", "is happy",
        "feels sad", "feels sleepy", "feels surprised", "is working", "is waiting",
        "has hit a blocker", "has finished", "is asleep", "is looking around"
    };
    static const char* const moodZh[] = {
        "正在休息", "正在聆听", "正在思考", "正在说话", "很高兴", "有些难过",
        "有些困倦", "感到惊讶", "正在工作", "正在等待", "遇到阻碍", "已经完成", "正在熟睡", "正在到处看看"
    };
    static const char* const expressionEn[] = {
        "is resting", "looks relaxed", "feels curious", "looks focused", "is happy",
        "looks skeptical", "feels shy", "is winking", "feels dizzy", "looks alert"
    };
    static const char* const expressionZh[] = {
        "正在休息", "神态自然", "感到好奇", "神情专注", "很高兴", "有些怀疑",
        "有些害羞", "正在眨眼", "感到眩晕", "保持警觉"
    };
    // Describe the principal current state. An explicitly selected idle face
    // carries the meaning when no active mood or transient reaction supersedes it.
    bool idleExpression = _effMood == Mood::Idle && _expression != Expression::Auto;
    uint8_t index = idleExpression ? (uint8_t)_effExpression : (uint8_t)_effMood;
    const char* phrase = idleExpression
        ? (language == Language::Chinese ? expressionZh : expressionEn)[index < expressionCount() ? index : 0]
        : (language == Language::Chinese ? moodZh : moodEn)[index < moodCount() ? index : 0];
    int required = snprintf(out, capacity, "%s %s", _name, phrase);
    // Never leave an incomplete UTF-8 character in a short caller buffer.
    if (out && capacity && required >= (int)capacity) {
        size_t end = capacity - 1, start = end;
        while (start && ((unsigned char)out[start - 1] & 0xC0) == 0x80) --start;
        if (start && (unsigned char)out[start - 1] >= 0xC0) {
            --start;
            unsigned char c = (unsigned char)out[start];
            size_t width = c < 0xE0 ? 2 : (c < 0xF0 ? 3 : 4);
            if (end - start < width) out[start] = 0;
        }
    }
    return required > 0 ? (size_t)required : 0;
}
BotUx::Preset BotUx::preset(uint8_t index) {
    static const Preset presets[] = {
        {Mood::Idle, Expression::Neutral, Animation::Calm},
        {Mood::Idle, Expression::Curious, Animation::Curious},
        {Mood::Happy, Expression::Joy, Animation::Bounce},
        {Mood::Listening, Expression::Focused, Animation::Calm},
        {Mood::Idle, Expression::Wink, Animation::Wave},
        {Mood::Done, Expression::Bashful, Animation::Sparkle},
        {Mood::Surprised, Expression::Alarmed, Animation::Orbit},
        {Mood::Sleepy, Expression::Auto, Animation::Calm},
    };
    return presets[index % presetCount()];
}
void BotUx::applyPreset(uint8_t index) {
    _presetIndex = index % presetCount();
    const Preset p = preset(_presetIndex);
    _pokeUntil = _reactionUntil = 0;
    clearGaze(); setMood(p.mood); setExpression(p.expression); setAnimation(p.animation);
}
uint8_t BotUx::randomPreset() {
    _presetSeed = _presetSeed * 1664525u + 1013904223u;
    applyPreset((_presetIndex + 1 + ((_presetSeed >> 16) % (presetCount() - 1))) % presetCount());
    return _presetIndex;
}
uint8_t BotUx::nextPreset() { applyPreset(_presetIndex + 1); return _presetIndex; }
void BotUx::resetToIdle() {
    _presetIndex = 0; _pokeUntil = _reactionUntil = 0;
    clearGaze(); setMood(Mood::Idle); setExpression(Expression::Auto); setAnimation(Animation::Auto);
}
const char* BotUx::gazeDirectionName(GazeDirection value, Language language) {
    static const char* const en[] = {"Auto", "Front", "Left", "Right", "Up", "Down", "Up-left", "Up-right", "Down-left", "Down-right"};
    static const char* const zh[] = {"自动", "正前", "向左", "向右", "向上", "向下", "左上", "右上", "左下", "右下"};
    return (language == Language::Chinese ? zh : en)[(uint8_t)value < gazeDirectionCount() ? (uint8_t)value : 0];
}
void BotUx::setGazeDirection(GazeDirection direction) {
    _gazeDirection = (uint8_t)direction < gazeDirectionCount() ? direction : GazeDirection::Auto;
}

void BotUx::setFaceSide(FaceSide side) {
    _faceSide = (uint8_t)side <= (uint8_t)FaceSide::Right ? side : FaceSide::Auto;
}

void BotUx::gazeAt(float x, float y, uint16_t holdMs) {
    if (_mood != Mood::Idle && _mood != Mood::LookingAround) return;
    _gazeX = clampf(x, -1.0f, 1.0f); _gazeY = clampf(y, -1.0f, 1.0f);
    float length = sqrtf(_gazeX * _gazeX + _gazeY * _gazeY);
    if (length > 1.0f) { _gazeX /= length; _gazeY /= length; }
    _gazeUntil = millis() + holdMs; _gazeHeld = holdMs != 0;
}
void BotUx::clearGaze() { _gazeHeld = false; }

void BotUx::_updateGaze(uint32_t now) {
    if (_mood != Mood::LookingAround) return;
    if ((int32_t)(now - _nextGaze) < 0) return;
    _gazeSeed = _gazeSeed * 1664525u + 1013904223u;
    _wanderX = ((float)((_gazeSeed >> 24) & 0xFF) / 127.5f) - 1.0f;
    _gazeSeed = _gazeSeed * 1664525u + 1013904223u;
    _wanderY = ((float)((_gazeSeed >> 24) & 0xFF) / 127.5f) - 1.0f;
    float length = sqrtf(_wanderX * _wanderX + _wanderY * _wanderY);
    if (length > 1.0f) { _wanderX /= length; _wanderY /= length; }
    _gazeSeed = _gazeSeed * 1664525u + 1013904223u;
    _nextGaze = now + 900u + ((_gazeSeed >> 16) % 1901u);
}

void BotUx::_updateTalk(uint32_t now) {
    uint32_t dt = (_lastNow == 0) ? 16u : (now - _lastNow);
    if (dt > 100u) dt = 100u;
    if (!_talking) {
        float a = easeAlpha(dt, 90.0f);
        _talkAmp += (0.0f - _talkAmp) * a;
        if (_talkAmp < 0.01f) _talkAmp = 0.0f;
        return;
    }

    uint32_t t = now - _talkSeed;
    float flap = _pulse(t, 150.0f);
    float syllable = 0.35f + 0.65f * _pulse(t, 390.0f, 1.2f);
    _talkAmp = 0.18f + 0.82f * flap * syllable;
}

void BotUx::_resolveMood(uint32_t now) {
    bool surpriseReaction = _pokeUntil && (int32_t)(_pokeUntil - now) > 0;
    bool happyReaction = !surpriseReaction && _reactionUntil &&
                         (int32_t)(_reactionUntil - now) > 0;
    if (surpriseReaction) _effMood = Mood::Surprised;
    else if (happyReaction) _effMood = Mood::Happy;
    else {
        _effMood = _mood;
        _pokeUntil = 0;
        _reactionUntil = 0;
    }

    if (surpriseReaction) _effExpression = Expression::Alarmed;
    else if (happyReaction) _effExpression = Expression::Joy;
    else if (_expression != Expression::Auto) _effExpression = _expression;
    else {
        switch (_effMood) {
            case Mood::Listening: _effExpression = Expression::Curious; break;
            case Mood::Happy:     _effExpression = Expression::Joy; break;
            case Mood::Surprised: _effExpression = Expression::Alarmed; break;
            case Mood::Working:   _effExpression = Expression::Focused; break;
            case Mood::Done:      _effExpression = Expression::Bashful; break;
            default:              _effExpression = Expression::Neutral; break;
        }
    }

    float targetOpen = 1.0f, targetAsym = 0.04f;
    float targetPairX = 0.26f, targetPairY = -0.38f, targetAngle = 0.34f;
    float targetLean = 0.0f, targetTwist = 0.0f;
    constexpr float kDiagonal = 0.70710678f;
    float gazeX = kDiagonal, gazeY = -kDiagonal;
    float lift = 0.0f, stretch = 0.0f;

    switch (_effMood) {
        case Mood::Listening:
            targetOpen = 1.12f; targetAsym = 0.0f;
            targetPairX = 0.26f; targetPairY = -0.38f; targetAngle = 0.24f;
            gazeX = 0.0f; gazeY = 0.0f; lift = -0.02f;
            break;
        case Mood::Thinking:
            targetOpen = 0.84f; targetAsym = 0.25f;
            gazeX = -0.65f; gazeY = -0.45f; targetLean = -0.12f;
            break;
        case Mood::Speaking:
            targetOpen = 0.94f; targetAsym = 0.06f;
            targetPairX = 0.26f; targetPairY = -0.38f; targetAngle = 0.18f;
            gazeX = 0.0f; gazeY = 0.0f;
            break;
        case Mood::Happy:
            targetOpen = 0.66f; targetAsym = 0.0f;
            targetPairX = 0.26f; targetPairY = -0.38f;
            gazeX = 0.0f; gazeY = 0.0f; lift = -0.05f; stretch = -0.025f;
            break;
        case Mood::Sad:
            targetOpen = 0.48f; targetAsym = 0.12f;
            targetPairX = 0.26f; targetPairY = -0.38f; targetAngle = 0.25f;
            gazeX = -0.10f; gazeY = 0.48f; lift = 0.045f; stretch = -0.035f;
            break;
        case Mood::Sleepy:
            targetOpen = 0.11f; targetAsym = 0.10f;
            targetPairX = 0.26f; targetPairY = -0.38f; targetAngle = 0.28f;
            gazeX = -0.18f; gazeY = 0.30f; lift = 0.055f; stretch = -0.045f;
            break;
        case Mood::Asleep:
            targetOpen = 0.0f; targetAsym = 0.0f;
            gazeX = 0.0f; gazeY = 0.20f; lift = 0.075f; stretch = -0.055f;
            break;
        case Mood::LookingAround:
            // Use the complete touch/direction field; motion preferences only
            // scale autonomous travel back toward the canonical Idle pose.
            targetOpen = 1.0f; targetAsym = 0.04f;
            targetPairX = 0.26f; targetPairY = -0.38f; targetAngle = 0.34f;
            gazeX = _wanderX; gazeY = _wanderY;
            break;
        case Mood::Waiting:
            targetOpen = 0.72f; targetAsym = -0.08f;
            targetAngle = 0.20f; targetLean = 0.08f;
            gazeX = sinf(2.0f * kPi * (now - _animStart) * _animationSpeed / 5200.0f)
                * 0.42f * (_reducedMotion ? 0.20f : 1.0f) * _motionAmount;
            gazeY = -0.10f; lift = 0.015f; stretch = -0.015f;
            break;
        case Mood::Surprised:
            targetOpen = 1.16f; targetAsym = 0.0f;
            targetPairX = 0.26f; targetPairY = -0.38f; targetAngle = 0.05f;
            gazeX = 0.0f; gazeY = 0.0f; lift = -0.055f; stretch = 0.08f;
            break;
        case Mood::Working:
            targetOpen = 0.96f; targetAsym = 0.03f;
            targetPairX = 0.26f; targetPairY = -0.38f; targetAngle = 0.10f;
            gazeX = 0.12f; gazeY = 0.0f;
            break;
        case Mood::Blocked:
            targetOpen = 1.0f; targetAsym = 0.0f;
            gazeX = 0.0f; gazeY = 0.0f; stretch = 0.04f;
            break;
        case Mood::Done:
            targetOpen = 0.88f; targetAsym = -0.06f;
            targetPairX = 0.26f; targetPairY = -0.38f; targetAngle = 0.28f;
            gazeX = -0.22f; gazeY = 0.15f; lift = -0.025f;
            break;
        default:
            break;
    }

    // Explicit expressions replace face geometry while retaining the mood's
    // body rhythm. Keep the same eye spacing and upper-right face placement across poses.
    switch (_effExpression) {
        case Expression::Neutral:
            if (_expression == Expression::Neutral) {
                targetOpen = 1.0f; targetAsym = 0.04f;
                targetPairX = 0.26f; targetPairY = -0.38f; targetAngle = 0.34f;
                targetLean = 0.0f; targetTwist = 0.0f;
                if (_effMood == Mood::LookingAround) {
                    gazeX = _wanderX; gazeY = _wanderY;
                } else if (_effMood == Mood::Idle) {
                    gazeX = kDiagonal; gazeY = -kDiagonal;
                } else {
                    gazeX = 0.0f; gazeY = 0.0f;
                }
            }
            break;
        case Expression::Curious:
            targetOpen = 1.06f; targetAsym = 0.08f;
            targetPairX = 0.26f; targetPairY = -0.38f; targetAngle = 0.22f;
            targetLean = -0.12f; gazeX = 0.48f; gazeY = -0.22f;
            break;
        case Expression::Focused:
            targetOpen = 0.70f; targetAsym = 0.0f;
            targetPairX = 0.26f; targetPairY = -0.38f; targetAngle = 0.22f;
            gazeX = 0.0f; gazeY = 0.08f;
            break;
        case Expression::Joy:
            targetOpen = 0.66f; targetAsym = 0.0f;
            targetPairX = 0.26f; targetPairY = -0.38f;
            gazeX = 0.0f; gazeY = 0.0f;
            break;
        case Expression::Skeptical:
            targetOpen = 0.86f; targetAsym = -0.14f;
            targetPairX = 0.26f; targetPairY = -0.38f; targetAngle = 0.28f;
            targetLean = 0.16f; gazeX = -0.54f; gazeY = -0.10f;
            break;
        case Expression::Bashful:
            targetOpen = 0.86f; targetAsym = 0.12f;
            targetPairX = 0.26f; targetPairY = -0.38f; targetAngle = 0.34f;
            gazeX = -0.34f; gazeY = 0.46f; targetTwist = -0.08f;
            break;
        case Expression::Wink:
            targetOpen = 0.80f; targetAsym = 0.0f;
            targetPairX = 0.26f; targetPairY = -0.38f; targetAngle = 0.18f;
            targetLean = -0.15f; gazeX = 0.24f; gazeY = 0.0f;
            break;
        case Expression::Dizzy:
            targetOpen = 0.92f; targetAsym = 0.0f;
            targetPairX = 0.26f; targetPairY = -0.38f;
            targetTwist = sinf(2.0f * kPi * (now - _animStart) * _animationSpeed / 3600.0f)
                * 0.08f * (_reducedMotion ? 0.20f : 1.0f) * _motionAmount;
            gazeX = 0.0f; gazeY = 0.0f;
            break;
        case Expression::Alarmed:
            targetOpen = 1.16f; targetAsym = 0.0f;
            targetPairX = 0.26f; targetPairY = -0.38f; targetAngle = 0.05f;
            gazeX = 0.0f; gazeY = 0.0f;
            break;
        default:
            break;
    }

    bool gazeMood = _mood == Mood::Idle || _mood == Mood::LookingAround;
    if (_gazeHeld && ((int32_t)(now - _gazeUntil) >= 0 || !gazeMood)) clearGaze();
    bool idleCanonical = _effMood == Mood::Idle &&
                         (_expression == Expression::Auto || _expression == Expression::Neutral);
    bool directed = _gazeDirection != GazeDirection::Auto ||
                    _gazeHeld || idleCanonical || _effMood == Mood::LookingAround;

    if (_battery <= 10 && targetOpen > 0.24f) {
        targetOpen = 0.24f;
        lift += 0.03f;
    }

    uint32_t dt = (_lastNow == 0) ? 16u : (now - _lastNow);
    if (dt > 1000u) dt = 1000u;
    float timeConstant = (_transitionMs == 0) ? 1.0f : clampf(_transitionMs * 0.42f, 28.0f, 600.0f);
    float a = _snapPose ? 1.0f : easeAlpha(dt, timeConstant);
    float gazeA = _snapPose ? 1.0f : easeAlpha(dt, 120.0f);
    _directionPose += ((directed ? 1.0f : 0.0f) - _directionPose) * gazeA;
    _openBase += (targetOpen - _openBase) * a;
    _eyeAsym += (targetAsym - _eyeAsym) * a;
    _eyeSmile += ((_effExpression == Expression::Joy ? 1.0f : 0.0f) - _eyeSmile) * a;
    _eyeWink += ((_effExpression == Expression::Wink ? 1.0f : 0.0f) - _eyeWink) * a;
    _eyeRound += ((_effExpression == Expression::Alarmed ? 0.50f : 0.0f) - _eyeRound) * a;
    _eyePairX += (targetPairX - _eyePairX) * a;
    _eyePairY += (targetPairY - _eyePairY) * a;
    _eyeAngle += (targetAngle - _eyeAngle) * a;
    _bodyLean += (targetLean - _bodyLean) * a;
    _eyeTwist += (targetTwist - _eyeTwist) * a;
    float gazeMotion = (_reducedMotion ? 0.20f : 1.0f) * fminf(_motionAmount, 1.0f);
    if (_gazeHeld) {
        gazeX = _gazeX; gazeY = _gazeY;
    } else if (_gazeDirection != GazeDirection::Auto) {
        static const float x[] = {0, 0, -1, 1, 0, 0, -kDiagonal, kDiagonal, -kDiagonal, kDiagonal};
        static const float y[] = {0, 0, 0, 0, -1, 1, -kDiagonal, -kDiagonal, kDiagonal, kDiagonal};
        gazeX = x[(uint8_t)_gazeDirection];
        gazeY = y[(uint8_t)_gazeDirection];
    } else if (idleCanonical) {
        gazeX = kDiagonal; gazeY = -kDiagonal;
    } else if (_effMood == Mood::LookingAround) {
        gazeX = kDiagonal + (_wanderX - kDiagonal) * gazeMotion;
        gazeY = -kDiagonal + (_wanderY + kDiagonal) * gazeMotion;
    } else if (_effExpression == Expression::Neutral &&
               (_effMood == Mood::Idle || _expression == Expression::Neutral)) {
        gazeX *= gazeMotion; gazeY *= gazeMotion;
    } else {
        // A stable facial expression still glances around without losing its pose.
        gazeX = clampf(gazeX + _wanderX * 0.20f * gazeMotion, -1, 1);
        gazeY = clampf(gazeY + _wanderY * 0.16f * gazeMotion, -1, 1);
    }
    _pupilDX += (gazeX - _pupilDX) * gazeA;
    _pupilDY += (gazeY - _pupilDY) * gazeA;
    _snapPose = false;

    float motionA = easeAlpha(dt, _reducedMotion ? 380.0f : 240.0f);
    _motionX += (_motionTargetX - _motionX) * motionA;
    _motionY += (_motionTargetY - _motionY) * motionA;
    _shake += (_shakeTarget - _shake) * motionA;

    // Use the same blended horizontal geometry as draw(), including IMU
    // countershift, so Auto changes handedness only as the rendered pair
    // crosses the body's centre line.
    float faceSensorAmount = _reducedMotion ? _motionAmount * 0.32f : _motionAmount;
    float renderedGazeX = clampf(_pupilDX - _motionX * 0.18f * faceSensorAmount, -1.0f, 1.0f);
    float fieldX = clampf(renderedGazeX / kDiagonal, -1.0f, 1.0f);
    float naturalX = _eyePairX + renderedGazeX * 0.24f;
    float directedX = fieldX * 0.26f;
    float faceX = naturalX + (directedX - naturalX) * _directionPose;
    float faceTarget = _faceHandedness;
    if (_faceSide == FaceSide::Left) faceTarget = -1.0f;
    else if (_faceSide == FaceSide::Right) faceTarget = 1.0f;
    else if (faceX < -0.015f) faceTarget = -1.0f;
    else if (faceX > 0.015f) faceTarget = 1.0f;
    else faceTarget = 0.0f;
    _faceHandedness += (faceTarget - _faceHandedness) * gazeA;

    float open = _openBase;
    if (_blinking && _effMood != Mood::Thinking && _effMood != Mood::Blocked) {
        uint32_t t = now - _blinkStart;
        uint32_t closeMs = blinkCloseMs(_effMood);
        uint32_t holdMs = blinkHoldMs(_effMood);
        uint32_t openMs = blinkOpenMs(_effMood);
        float p;
        if (_blinkPhase == 0) p = (float)t / closeMs;
        else if (_blinkPhase == 1) p = 1.0f;
        else p = 1.0f - (float)(t - closeMs - holdMs) / openMs;
        open *= 1.0f - _smooth(clampf(p, 0.0f, 1.0f));
    }
    _open = clampf(open, 0.0f, 1.35f);

    uint32_t elapsed = now - _animStart;
    float animElapsed = elapsed * _animationSpeed;
    float period = (_effMood == Mood::Asleep) ? 8800.0f
                 : (_effMood == Mood::Sleepy) ? 7200.0f
                 : (_effMood == Mood::Waiting) ? 4600.0f : 3800.0f;
    _breath = sinf(2.0f * kPi * animElapsed / period);
    int16_t r = _m.bodyR ? _m.bodyR : (int16_t)(min16(_w, _h) * 0.39f);
    float lifeAmount = (_reducedMotion ? 0.20f : 1.0f) * _motionAmount;
    float dx = 0.0f;
    float dy = lift * r + _breath * r * 0.030f * lifeAmount;
    float sx = 1.0f + _breath * 0.022f * lifeAmount;
    float sy = 1.0f - _breath * 0.016f * lifeAmount + stretch;
    switch (_effMood) {
        case Mood::Thinking:
            dx += sinf(2.0f * kPi * animElapsed / 2300.0f) * r * 0.055f * lifeAmount;
            break;
        case Mood::Speaking:
            dy -= _talkAmp * r * 0.045f * lifeAmount;
            sx += _talkAmp * 0.035f * lifeAmount;
            sy += _talkAmp * 0.055f * lifeAmount;
            break;
        case Mood::Working:
            dx += sinf(2.0f * kPi * animElapsed / 900.0f) * r * 0.025f * lifeAmount;
            sy += _pulse((uint32_t)animElapsed, 520.0f) * 0.035f * lifeAmount;
            break;
        case Mood::Happy:
        case Mood::Done:
            dy -= _pulse((uint32_t)animElapsed, 780.0f) * r * 0.025f * lifeAmount;
            break;
        case Mood::Surprised:
            sy += _pulse((uint32_t)animElapsed, 260.0f) * 0.025f * lifeAmount;
            sx -= 0.025f;
            break;
        default:
            break;
    }

    Animation active = _animation;
    if (active == Animation::Auto) {
        switch (_effMood) {
            case Mood::Listening: active = Animation::Curious; break;
            case Mood::Thinking:
            case Mood::Working:   active = Animation::Orbit; break;
            case Mood::Speaking:
            case Mood::Surprised: active = Animation::Bounce; break;
            case Mood::Happy:     active = Animation::Bounce; break;
            case Mood::Done:      active = Animation::Sparkle; break;
            case Mood::Waiting:   active = Animation::Calm; break;
            case Mood::Blocked:   active = Animation::Glitch; break;
            default:              active = Animation::Calm; break;
        }
    }
    _activeAnimation = active;

    float amount = (_reducedMotion ? 0.12f : 0.80f) * _motionAmount;
    float phase = 2.0f * kPi * animElapsed;
    float animEyeTwist = 0.0f;
    switch (active) {
        case Animation::Calm:
            dx += sinf(phase / 4200.0f) * r * 0.045f * amount;
            dy += cosf(phase / 5400.0f) * r * 0.025f * amount;
            animEyeTwist = sinf(phase / 4600.0f) * 0.055f * amount;
            break;
        case Animation::Curious:
            dx += sinf(phase / 1900.0f) * r * 0.050f * amount;
            dy += cosf(phase / 2500.0f) * r * 0.025f * amount;
            animEyeTwist = sinf(phase / 2100.0f) * 0.12f * amount;
            break;
        case Animation::Orbit:
            dx += sinf(phase / 1300.0f) * r * 0.065f * amount;
            dy += cosf(phase / 1300.0f) * r * 0.045f * amount;
            animEyeTwist = sinf(phase / 1300.0f) * 0.18f * amount;
            break;
        case Animation::Bounce: {
            float hop = _pulse((uint32_t)animElapsed, 760.0f);
            dy -= hop * hop * r * 0.10f * amount;
            sx += (0.5f - hop) * 0.065f * amount;
            sy -= (0.5f - hop) * 0.085f * amount;
            break;
        }
        case Animation::Glitch: {
            int step = (int)(animElapsed / 95.0f);
            int jitter = ((step * 37 + 11) % 7) - 3;
            dx += jitter * r * 0.018f * amount;
            animEyeTwist = (((step * 13) % 5) - 2) * 0.055f * amount;
            break;
        }
        case Animation::Wave:
            dx += sinf(phase / 1500.0f) * r * 0.075f * amount;
            animEyeTwist = cosf(phase / 1500.0f) * 0.24f * amount;
            break;
        case Animation::Sparkle: {
            float glow = _pulse((uint32_t)animElapsed, 900.0f, 0.8f);
            dy -= _pulse((uint32_t)animElapsed, 1100.0f) * r * 0.035f * amount;
            sx += glow * 0.025f * amount;
            sy += glow * 0.025f * amount;
            break;
        }
        default:
            break;
    }
    _animEyeTwist = animEyeTwist;

    // Tilt moves the whole character while the eyes counter-shift slightly,
    // creating depth. Shake produces a brief deterministic squash/jitter.
    float sensorAmount = _reducedMotion ? _motionAmount * 0.32f : _motionAmount;
    dx += _motionX * r * 0.055f * sensorAmount;
    dy += _motionY * r * 0.042f * sensorAmount;
    if (!_reducedMotion && _shake > 0.01f) {
        float shakeWave = sinf(phase / 72.0f);
        dx += shakeWave * _shake * r * 0.025f * _motionAmount;
        sx += _shake * 0.055f * _motionAmount;
        sy -= _shake * 0.060f * _motionAmount;
    }
    _bodySX += (clampf(sx, 0.88f, 1.12f) - _bodySX) * a;
    _bodySY += (clampf(sy, 0.86f, 1.14f) - _bodySY) * a;
    // Preserve the silhouette at motion extremes. This matters most for a
    // large watch-face sprite where only a narrow transparent-looking margin
    // surrounds the orb (the sprite itself is opaque RGB565).
    float rx = r * _bodySX;
    float ry = r * _bodySY;
    dx = clampf(dx, rx - _m.cx + 2.0f, _w - _m.cx - rx - 2.0f);
    dy = clampf(dy, ry - _m.cy + 2.0f, _h - _m.cy - ry - 2.0f);
    _bodyDX += (dx - _bodyDX) * a;
    _bodyDY += (dy - _bodyDY) * a;
}

void BotUx::draw() {
    if (!_cv) return;
    _cv->fillSprite(_style.bgColor);
    _drawBody();
    _drawAnimationFx();
    _drawEyes();
    _drawOverlays();
}

void BotUx::_drawBody() {
    int16_t r = _m.bodyR ? _m.bodyR : (int16_t)(min16(_w, _h) * 0.39f);
    int16_t cx = _m.cx + (int16_t)_bodyDX;
    int16_t cy = _m.cy + (int16_t)_bodyDY;
    int16_t rx = (int16_t)(r * _bodySX);
    int16_t ry = (int16_t)(r * _bodySY);
    if (rx < 2) rx = 2;
    if (ry < 2) ry = 2;

    // Thinking and Working become volumetric dot shells. Keep Auto on the
    // mood's own choreography; an explicitly selected Animation may still
    // translate the complete shell through the ordinary animation layer.
    if (_effMood == Mood::Thinking || _effMood == Mood::Working) {
        static const int kSmallOrbDotCount = 32;
        static const int kMediumOrbDotCount = 64;
        static const int kLargeOrbDotCount = 96;
        static const float kThinkingPeriodMs = 1600.0f;
        static const float kWorkingPeriodMs = 1400.0f;
        static const float kMinOrbDotRadius = 0.90f;
        static const float kOrbDotRadiusPerBodyRadius = 0.030f;
        static const float kOrbShellScale = 0.95f;
        static const float kSmallOrbShellScale = 0.85f;
        struct OrbDot { float x, y, z, radius; uint8_t opacity; };
        OrbDot dots[kLargeOrbDotCount];
        int count = r >= 60 ? kLargeOrbDotCount
                            : (r >= 34 ? kMediumOrbDotCount : kSmallOrbDotCount);
        float amount = fminf(_motionAmount, 1.5f);
        float reducedScale = _reducedMotion ? 0.20f : 1.0f;
        float phaseScale = fminf(_motionAmount, 1.0f) * reducedScale;
        float deformationScale = amount * reducedScale;
        float elapsed = (_now - _animStart) * _animationSpeed * phaseScale;
        // Base periods remain short enough to read on the Watch after its
        // animation-speed and motion-amount controls both scale the phase.
        float period = _effMood == Mood::Thinking ? kThinkingPeriodMs
                                                  : kWorkingPeriodMs;
        float phase = elapsed / period;
        phase -= floorf(phase);
        float turn = 2.0f * kPi * phase;
        float turnCos = cosf(turn), turnSin = sinf(turn);
        float shellCos = _effMood == Mood::Thinking ? turnCos : cosf(-turn);
        float shellSin = _effMood == Mood::Thinking ? turnSin : sinf(-turn);
        const float pitch = 0.36f;
        const float pitchCos = cosf(pitch), pitchSin = sinf(pitch);
        const float goldenCos = -0.73736888f;
        const float goldenSin = 0.67549029f;
        float gx = 1.0f, gz = 0.0f;
        float dotMinX = 0.0f, dotMaxX = 0.0f;
        float dotMinY = 0.0f, dotMaxY = 0.0f;

        for (int i = 0; i < count; ++i) {
            float px, py, pz;
            float pointOpacity = 0.90f;
            if (_effMood == Mood::Thinking) {
                py = 1.0f - i * (2.0f / (count - 1));
                float ring = sqrtf(fmaxf(0.0f, 1.0f - py * py));
                px = gx * ring;
                pz = gz * ring;
                float swell = 1.0f + 0.11f * deformationScale
                            * sinf(2.0f * kPi * (phase - 0.22f * (py + 1.0f)));
                px *= swell; py *= swell; pz *= swell;

                float nextGx = gx * goldenCos - gz * goldenSin;
                gz = gx * goldenSin + gz * goldenCos;
                gx = nextGx;
            } else {
                // Each point advances bottom-to-top on a meridian and wraps.
                // A second, opposing shell turn leaves diagonal streams and
                // open lanes instead of reading as a uniformly spinning ball.
                float u = i * 0.61803399f + phase;
                u -= floorf(u);
                float polar = kPi * u;
                float ring = sinf(polar);
                float longitude = 2.0f * kPi * (i * 0.75487767f) + 2.0f * turn;
                px = cosf(longitude) * ring;
                py = cosf(polar);
                pz = sinf(longitude) * ring;
                pointOpacity = powf(fmaxf(0.0f, ring), 0.40f) * 0.90f;
            }

            // Rotate the shell and hold a fixed tilt so depth is legible.
            float tx = px * shellCos - pz * shellSin;
            float tz = px * shellSin + pz * shellCos;
            float ty = py * pitchCos - tz * pitchSin;
            tz = py * pitchSin + tz * pitchCos;

            float perspective = 3.5f / (3.5f - tz);
            float depth = clampf((tz + 1.1f) / 2.2f, 0.0f, 1.0f);
            // Small canvases reserve enough room for the minimum dot radius;
            // the visible dot edges still match the ordinary body footprint.
            float sphereR = r * (r < 34 ? kSmallOrbShellScale : kOrbShellScale);
            float dotBase = fmaxf(kMinOrbDotRadius,
                                  r * kOrbDotRadiusPerBodyRadius);
            float dotScale = _effMood == Mood::Thinking ? 0.85f : 0.80f;
            dots[i].x = tx * sphereR * perspective;
            dots[i].y = ty * sphereR * perspective;
            dots[i].z = tz;
            dots[i].radius = dotBase * (0.40f + 1.60f * depth)
                           * perspective * dotScale;
            float depthOpacity = 0.12f + 0.88f * powf(depth, 1.35f);
            dots[i].opacity = (uint8_t)(255.0f * depthOpacity * pointOpacity);
            float extent = fmaxf(dots[i].radius, 0.8f) + 0.5f;
            float left = dots[i].x - extent, right = dots[i].x + extent;
            float top = dots[i].y - extent, bottom = dots[i].y + extent;
            if (i == 0 || left < dotMinX) dotMinX = left;
            if (i == 0 || right > dotMaxX) dotMaxX = right;
            if (i == 0 || top < dotMinY) dotMinY = top;
            if (i == 0 || bottom > dotMaxY) dotMaxY = bottom;
        }

        float shellCx = _m.cx + (_animation == Animation::Auto ? 0.0f : _bodyDX);
        float shellCy = _m.cy + (_animation == Animation::Auto ? 0.0f : _bodyDY);
        // Keep an explicitly translated shell inside the same two-pixel
        // safety margin as the ordinary body. The cloud remains rigid: its
        // centre is constrained without clipping or moving individual dots.
        float minCx = 2.0f - dotMinX, maxCx = _w - 3.0f - dotMaxX;
        float minCy = 2.0f - dotMinY, maxCy = _h - 3.0f - dotMaxY;
        if (minCx <= maxCx) shellCx = clampf(shellCx, minCx, maxCx);
        if (minCy <= maxCy) shellCy = clampf(shellCy, minCy, maxCy);
        for (int pass = 0; pass < 2; ++pass) {
            for (int i = 0; i < count; ++i) {
                if ((dots[i].z >= 0.0f) != (pass != 0)) continue;
                _fillCapsule(shellCx + dots[i].x, shellCy + dots[i].y,
                             0.0f, 0.0f, dots[i].radius,
                             _style.bodyColor, dots[i].opacity);
            }
        }
        return;
    }
    if (_effMood == Mood::Blocked) {
        float stemW = fmaxf(3.0f, r * 0.28f * _bodySX);
        float stemH = r * 0.98f * _bodySY;
        float x = _m.cx + _bodyDX, y = _m.cy + _bodyDY;
        _fillCapsule(x, y - r * 0.63f + stemH * 0.5f, 0,
                     (stemH - stemW) * 0.5f, stemW * 0.5f, _style.bodyColor);
        _fillEllipseAA(x, y + r * 0.58f * _bodySY, stemW * 0.5f, stemW * 0.5f,
                       _style.bodyColor);
        return;
    }
    if (_style.bodyStyle == BodyStyle::None) return;

    switch (_style.bodyStyle) {
        case BodyStyle::Round:
            _fillEllipseAA(_m.cx + _bodyDX, _m.cy + _bodyDY,
                           r * _bodySX, r * _bodySY, _style.bodyColor);
            break;
        case BodyStyle::RoundedSquare: {
            int16_t corner = (int16_t)(r * 0.48f);
            _cv->fillRoundRect(cx - rx, cy - ry, rx * 2, ry * 2, corner, _style.bodyColor);
            break;
        }
        case BodyStyle::Hexagon: {
            int16_t px[6], py[6];
            for (int i = 0; i < 6; ++i) {
                float angle = kPi / 6.0f + i * kPi / 3.0f;
                px[i] = cx + (int16_t)(rx * cosf(angle));
                py[i] = cy + (int16_t)(ry * sinf(angle));
            }
            for (int i = 0; i < 6; ++i)
                _cv->fillTriangle(cx, cy, px[i], py[i], px[(i + 1) % 6], py[(i + 1) % 6], _style.bodyColor);
            break;
        }
        default:
            break;
    }
}

void BotUx::_drawAnimationFx() {
    if (_effMood == Mood::Asleep) {
        // Three vector z marks; fixed geometry also works without a font or orb.
        float r = _m.bodyR ? _m.bodyR : min16(_w, _h) * 0.39f;
        float amount = (_reducedMotion ? 0.20f : 1.0f) * _motionAmount;
        float phase = (_now - _animStart) * _animationSpeed / 4800.0f;
        for (int i = 0; i < 3; ++i) {
            float t = phase + i / 3.0f;
            t -= floorf(t);
            float travel = (t - 0.5f) * amount;
            float x = _m.cx + r * (0.75f + i * 0.08f + travel * 0.06f);
            float y = _m.cy - r * (0.88f + i * 0.10f + travel * 0.10f);
            float half = fmaxf(1.1f, r * (0.045f + i * 0.010f));
            float stroke = fmaxf(0.65f, r * 0.009f);
            uint8_t opacity = (uint8_t)(amount > 0 ? 255 * sinf(kPi * t) * sinf(kPi * t) : 220);
            _fillCapsule(x, y - half, half, 0, stroke, _style.accentColor, opacity);
            _fillCapsule(x, y, -half, half, stroke, _style.accentColor, opacity);
            _fillCapsule(x, y + half, half, 0, stroke, _style.accentColor, opacity);
        }
        return;
    }

    if (_m.bodyR == 0) return;
    float r = _m.bodyR;
    float cx = _m.cx + _bodyDX;
    float cy = _m.cy + _bodyDY;
    float motion = (_reducedMotion ? 0.20f : 1.0f)
                 * fminf(_motionAmount, 1.0f);
    float elapsed = (_now - _animStart) * _animationSpeed;

    if (_effMood == Mood::Happy) {
        // Warm cheek strokes make the joyful face readable even when motion is
        // reduced or an explicit non-celebratory Animation is selected.
        float sensorAmount = _reducedMotion ? _motionAmount * 0.32f : _motionAmount;
        float gazeX = clampf(_pupilDX - _motionX * 0.18f * sensorAmount, -1.0f, 1.0f);
        float gazeY = clampf(_pupilDY - _motionY * 0.14f * sensorAmount, -1.0f, 1.0f);
        constexpr float kDiagonal = 0.70710678f;
        float fieldX = clampf(gazeX / kDiagonal, -1.0f, 1.0f);
        float fieldY = clampf(gazeY / kDiagonal, -1.0f, 1.0f);
        float naturalX = _eyePairX + gazeX * 0.24f;
        float naturalY = _eyePairY + gazeY * (gazeY > 0.0f ? 0.16f : 0.20f);
        float directedX = fieldX * 0.26f;
        float directedY = fieldY * (fieldY > 0.0f ? 0.16f : 0.38f);
        float faceCx = cx + (naturalX + (directedX - naturalX) * _directionPose) * r;
        float faceCy = cy + (naturalY + (directedY - naturalY) * _directionPose) * r;
        float cheekY = faceCy + r * 0.28f;
        float cheekDx = fmaxf(0.9f, r * 0.070f);
        float cheekDy = r * 0.020f;
        float cheekRadius = fmaxf(0.8f, r * 0.030f);
        _fillCapsule(faceCx - _m.eyeDX, cheekY, cheekDx, -cheekDy,
                     cheekRadius, _style.blushColor, 210);
        _fillCapsule(faceCx + _m.eyeDX, cheekY, cheekDx, cheekDy,
                     cheekRadius, _style.blushColor, 210);
    }

    if (_effMood == Mood::Done) {
        // A large rounded check occupies the quiet lower-left portion of the
        // orb. It remains present with every expression and animation, so Done
        // never depends on tiny orbiting decoration for its meaning.
        float checkScale = 1.0f + 0.025f * motion
                         * sinf(2.0f * kPi * elapsed / 1100.0f);
        float stroke = fmaxf(1.15f, r * 0.052f) * checkScale;
        uint16_t checkColor = mix565(_style.accentColor, _style.eyeColor, 96);
        float jointX = cx - r * 0.28f;
        float jointY = cy + r * 0.72f;
        float shortX = cx - r * 0.47f;
        float shortY = cy + r * 0.57f;
        float longX = cx + r * 0.14f;
        float longY = cy + r * 0.43f;
        _fillCapsule((shortX + jointX) * 0.5f, (shortY + jointY) * 0.5f,
                     (jointX - shortX) * 0.5f, (jointY - shortY) * 0.5f,
                     stroke, checkColor);
        _fillCapsule((jointX + longX) * 0.5f, (jointY + longY) * 0.5f,
                     (longX - jointX) * 0.5f, (longY - jointY) * 0.5f,
                     stroke, checkColor);
        _fillCapsule(jointX, jointY, 0.0f, 0.0f, stroke,
                     checkColor);
    }

    // Auto Happy adds radiance around its Bounce rhythm; Sparkle remains an
    // independently selectable animation for every mood. Two tapered stars
    // stay near the upper silhouette instead of orbiting across the face/check.
    bool happyAuto = _effMood == Mood::Happy && _animation == Animation::Auto;
    if (_activeAnimation != Animation::Sparkle && !happyAuto) return;
    static const float anchorAngles[] = {-2.42f, -0.46f};
    for (int i = 0; i < 2; ++i) {
        float twinkle = sinf(2.0f * kPi * elapsed / 920.0f + i * 2.1f);
        float angle = anchorAngles[i] + 0.11f * motion
                    * sinf(2.0f * kPi * elapsed / 2600.0f + i);
        float pulse = 0.88f + 0.12f * motion * twinkle;
        float vertical = fmaxf(3.5f, r * 0.205f * pulse);
        float horizontal = vertical * 0.70f;
        float x = cx + cosf(angle) * r * 0.91f;
        float y = cy + sinf(angle) * r * 0.79f;
        x = clampf(x, vertical + 1.0f, _w - vertical - 2.0f);
        y = clampf(y, vertical + 1.0f, _h - vertical - 2.0f);
        _fillSparkle(x, y, vertical, horizontal, _style.accentColor);
    }
}

void BotUx::_fillSparkle(float cx, float cy, float verticalRadius,
                         float horizontalRadius, uint16_t color) {
    float inner = fmaxf(0.85f, fminf(verticalRadius, horizontalRadius) * 0.24f);
    int x = (int)roundf(cx), y = (int)roundf(cy);
    int top = (int)roundf(cy - verticalRadius);
    int bottom = (int)roundf(cy + verticalRadius);
    int left = (int)roundf(cx - horizontalRadius);
    int right = (int)roundf(cx + horizontalRadius);
    int ix0 = (int)floorf(cx - inner), ix1 = (int)ceilf(cx + inner);
    int iy0 = (int)floorf(cy - inner), iy1 = (int)ceilf(cy + inner);
    _cv->fillTriangle(x, top, ix0, iy0, ix1, iy0, color);
    _cv->fillTriangle(right, y, ix1, iy0, ix1, iy1, color);
    _cv->fillTriangle(x, bottom, ix1, iy1, ix0, iy1, color);
    _cv->fillTriangle(left, y, ix0, iy1, ix0, iy0, color);
    _fillCapsule(cx, cy, 0.0f, 0.0f, inner * 1.20f, color);
}

// Scanline ellipse: solid spans plus one-pixel coverage at the perimeter.
// The extra work scales with circumference, with no supersampled framebuffer.
void BotUx::_fillEllipseAA(float cx, float cy, float rx, float ry, uint16_t color) {
    for (int y = (int)floorf(cy - ry - 0.5f); y <= (int)ceilf(cy + ry + 0.5f); ++y) {
        float extent[4], widest = 0.0f, narrowest = rx;
        for (int sample = 0; sample < 4; ++sample) {
            float v = (y - cy + (sample - 1.5f) * 0.25f) / ry;
            extent[sample] = fabsf(v) >= 1.0f ? 0.0f : rx * sqrtf(1.0f - v * v);
            widest = fmaxf(widest, extent[sample]);
            narrowest = fminf(narrowest, extent[sample]);
        }
        if (widest == 0.0f) continue;
        int left = (int)ceilf(cx - narrowest + 0.5f);
        int right = (int)floorf(cx + narrowest - 0.5f);
        if (right >= left) _cv->fillRect(left, y, right - left + 1, 1, color);
        for (int side = -1; side <= 1; side += 2) {
            int begin = side < 0 ? (int)floorf(cx - widest - 0.5f) : right + 1;
            int end = side < 0 ? left - 1 : (int)ceilf(cx + widest + 0.5f);
            for (int x = begin; x <= end; ++x) {
                float coverage = 0.0f;
                for (int sample = 0; sample < 4; ++sample)
                    coverage += fmaxf(0.0f, fminf(x + 0.5f, cx + extent[sample]) -
                                               fmaxf(x - 0.5f, cx - extent[sample]));
                if (coverage > 0.0f)
                    _cv->drawPixel(x, y, mix565(_style.bgColor, color, (uint8_t)(coverage * 63.75f)));
            }
        }
    }
}

void BotUx::_fillCapsule(float cx, float cy, float halfDx, float halfDy,
                         float radius, uint16_t color, uint8_t opacity) {
    if (!opacity) return;
    radius = fmaxf(radius, 0.8f);
    if (halfDx == 0.0f && halfDy == 0.0f) {
        int minX = (int)floorf(cx - radius - 0.5f);
        int maxX = (int)ceilf(cx + radius + 0.5f);
        int minY = (int)floorf(cy - radius - 0.5f);
        int maxY = (int)ceilf(cy + radius + 0.5f);
        float inner2 = (radius - 0.5f) * (radius - 0.5f);
        float outer2 = (radius + 0.5f) * (radius + 0.5f);
        if (opacity < 255) {
            uint16_t flatInterior = mix565(_style.bgColor, color, opacity);
            for (int y = minY; y <= maxY; ++y) {
                float dy = y - cy;
                float dy2 = dy * dy;
                for (int x = minX; x <= maxX; ++x) {
                    float dx = x - cx;
                    float d2 = dx * dx + dy2;
                    if (d2 <= inner2) {
                        uint16_t under = _cv->readPixel(x, y);
                        _cv->drawPixel(x, y, under == _style.bgColor
                            ? flatInterior : mix565(under, color, opacity));
                        continue;
                    }
                    if (d2 >= outer2) continue;
                    uint8_t coverage = (uint8_t)(clampf(
                        radius + 0.5f - sqrtf(d2), 0.0f, 1.0f) * opacity);
                    _cv->drawPixel(x, y,
                        mix565(_cv->readPixel(x, y), color, coverage));
                }
            }
            return;
        }
        for (int y = minY; y <= maxY; ++y) {
            float dy = y - cy;
            float dy2 = dy * dy;
            int run = -1;
            for (int x = minX; x <= maxX; ++x) {
                float dx = x - cx;
                float d2 = dx * dx + dy2;
                if (d2 <= inner2) {
                    if (run < 0) run = x;
                    continue;
                }
                if (run >= 0) {
                    _cv->fillRect(run, y, x - run, 1, color);
                    run = -1;
                }
                if (d2 >= outer2) continue;
                uint8_t coverage = (uint8_t)(clampf(
                    radius + 0.5f - sqrtf(d2), 0.0f, 1.0f) * opacity);
                _cv->drawPixel(x, y,
                    mix565(_cv->readPixel(x, y), color, coverage));
            }
            if (run >= 0) _cv->fillRect(run, y, maxX - run + 1, 1, color);
        }
        return;
    }
    float ax = cx - halfDx, ay = cy - halfDy;
    float vx = halfDx * 2.0f, vy = halfDy * 2.0f;
    float invLen = 1.0f / fmaxf(vx * vx + vy * vy, 0.0001f);
    int minX = (int)floorf(cx - fabsf(halfDx) - radius - 0.5f);
    int maxX = (int)ceilf(cx + fabsf(halfDx) + radius + 0.5f);
    int minY = (int)floorf(cy - fabsf(halfDy) - radius - 0.5f);
    int maxY = (int)ceilf(cy + fabsf(halfDy) + radius + 0.5f);
    float inner2 = (radius - 0.5f) * (radius - 0.5f);
    float outer2 = (radius + 0.5f) * (radius + 0.5f);
    for (int y = minY; y <= maxY; ++y) {
        int run = -1;
        for (int x = minX; x <= maxX; ++x) {
            float t = clampf(((x - ax) * vx + (y - ay) * vy) * invLen, 0.0f, 1.0f);
            float dx = x - ax - t * vx, dy = y - ay - t * vy;
            float d2 = dx * dx + dy * dy;
            if (d2 <= inner2) {
                // Opaque eyes retain the fast span path. Only fading z marks
                // composite their interior against the actual underlying body.
                if (opacity < 255) {
                    _cv->drawPixel(x, y, mix565(_cv->readPixel(x, y), color, opacity));
                    continue;
                }
                if (run < 0) run = x;
                continue;
            }
            if (run >= 0) { _cv->fillRect(run, y, x - run, 1, color); run = -1; }
            if (d2 >= outer2) continue;
            uint8_t coverage = (uint8_t)(clampf(radius + 0.5f - sqrtf(d2), 0.0f, 1.0f) * opacity);
            uint16_t under = _cv->readPixel(x, y);
            _cv->drawPixel(x, y, mix565(under, color, coverage));
        }
        if (run >= 0) _cv->fillRect(run, y, maxX - run + 1, 1, color);
    }
}

// A single swept quadratic: union distance is evaluated before coverage blending.
// 12 chords bound flattening error below 0.04px even at eyeRadius=24.
// No separately rounded/alpha-blended joins, no heap, no secondary framebuffer.
void BotUx::_fillEyeCurve(float cx, float cy, float dx, float dy, float rise,
                          float radius, uint16_t color) {
    constexpr int segments = 12;
    float ax[segments], ay[segments], vx[segments], vy[segments], inv[segments];
    float minX = -fabsf(dx), maxX = fabsf(dx);
    float minY = fminf(-fabsf(dy), -2 * rise), maxY = fabsf(dy);
    for (int i = 0; i < segments; ++i) {
        float t = (float)i / segments, u = (float)(i + 1) / segments;
        ax[i] = (2 * t - 1) * dx;
        ay[i] = (2 * t - 1) * dy - 4 * rise * t * (1 - t);
        vx[i] = (2 * u - 1) * dx - ax[i];
        vy[i] = (2 * u - 1) * dy - 4 * rise * u * (1 - u) - ay[i];
        inv[i] = 1.0f / fmaxf(vx[i] * vx[i] + vy[i] * vy[i], 0.000001f);
    }
    radius = fmaxf(radius, 0.8f);
    int left = (int)floorf(cx + minX - radius - 0.5f);
    int right = (int)ceilf(cx + maxX + radius + 0.5f);
    float inner2 = (radius - 0.5f) * (radius - 0.5f);
    float outer2 = (radius + 0.5f) * (radius + 0.5f);
    for (int y = (int)floorf(cy + minY - radius - 0.5f);
         y <= (int)ceilf(cy + maxY + radius + 0.5f); ++y) {
        int run = -1;
        for (int x = left; x <= right; ++x) {
            float nearest = outer2;
            for (int i = 0; i < segments; ++i) {
                float px = x - cx - ax[i], py = y - cy - ay[i];
                float t = clampf((px * vx[i] + py * vy[i]) * inv[i], 0, 1);
                px -= t * vx[i]; py -= t * vy[i];
                nearest = fminf(nearest, px * px + py * py);
            }
            if (nearest <= inner2) { if (run < 0) run = x; continue; }
            if (run >= 0) { _cv->fillRect(run, y, x - run, 1, color); run = -1; }
            if (nearest < outer2) {
                uint8_t coverage = (uint8_t)(clampf(radius + 0.5f - sqrtf(nearest), 0, 1) * 255);
                _cv->drawPixel(x, y, mix565(_cv->readPixel(x, y), color, coverage));
            }
        }
        if (run >= 0) _cv->fillRect(run, y, right - run + 1, 1, color);
    }
}

void BotUx::_drawEyes() {
    if (_effMood == Mood::Thinking || _effMood == Mood::Working || _effMood == Mood::Blocked) return;
    int16_t side = min16(_w, _h);
    int16_t r = _m.bodyR ? _m.bodyR : (int16_t)(side * 0.39f);
    float bodyCx = _m.cx + _bodyDX;
    float bodyCy = _m.cy + _bodyDY;
    float sensorAmount = _reducedMotion ? _motionAmount * 0.32f : _motionAmount;
    float gazeX = clampf(_pupilDX - _motionX * 0.18f * sensorAmount, -1.0f, 1.0f);
    float gazeY = clampf(_pupilDY - _motionY * 0.14f * sensorAmount, -1.0f, 1.0f);
    constexpr float kDiagonal = 0.70710678f;
    float fieldX = clampf(gazeX / kDiagonal, -1.0f, 1.0f);
    float fieldY = clampf(gazeY / kDiagonal, -1.0f, 1.0f);
    float naturalX = _eyePairX + gazeX * 0.24f;
    float naturalY = _eyePairY + gazeY * (gazeY > 0.0f ? 0.16f : 0.20f);
    float directedX = fieldX * 0.26f;
    // Preserve the established, comfortable near-centre downward limit while
    // the upper half reaches the legacy Idle baseline.
    float directedY = fieldY * (fieldY > 0.0f ? 0.16f : 0.38f);
    float pairCx = bodyCx + (naturalX + (directedX - naturalX) * _directionPose) * r;
    float pairCy = bodyCy + (naturalY + (directedY - naturalY) * _directionPose) * r;
    float facingX = fieldX * _directionPose, facingY = fieldY * _directionPose;
    // At the upper diagonals this is exactly the legacy Idle spacing. Pure
    // side views retain a small, continuous yaw compression.
    float dx = _m.eyeDX * (1.0f - 0.08f * fabsf(facingX) * (1.0f - fabsf(facingY)));
    float baseEr = (float)_m.eyeRadius;
    // The bilinear direction field crosses upright at side/centre and reaches
    // the exact legacy Idle +0.34 slope at the upper-right 45-degree pose.
    float eyeAngle = _eyeAngle * _faceHandedness * (1.0f - _directionPose)
        - facingX * facingY * 0.34f;

    float twist = (_eyeTwist + _animEyeTwist) * _faceHandedness;
    float twistCos = cosf(twist), twistSin = sinf(twist);
    for (int s = -1; s <= 1; s += 2) {
        float er = baseEr;
        float asym = 1.0f + s * _eyeAsym * _faceHandedness;
        float pitchOpen = 1.0f - facingY * 0.04f * (1.0f - fabsf(facingX));
        float eyeOpen = clampf(_open * asym * pitchOpen, 0.05f, 1.35f);
        float pairX = s * dx;
        float pairY = s * _bodyLean * _faceHandedness * baseEr;
        float ex = pairCx + pairX * twistCos - pairY * twistSin;
        float ey = pairCy + pairX * twistSin + pairY * twistCos;

        float lengthScale = 1.0f, radiusScale = 0.52f;
        switch (_style.eyeStyle) {
            case EyeStyle::Round:  lengthScale = 0.88f; radiusScale = 0.62f; break;
            case EyeStyle::Oval:   lengthScale = 1.25f; radiusScale = 0.48f; break;
            case EyeStyle::Square: lengthScale = 0.86f; radiusScale = 0.72f; break;
            case EyeStyle::Googly: lengthScale = 0.12f; radiusScale = 1.02f; break;
        }

        // Morph the neutral pill into a short, rounded arch. A blink closes
        // each eye continuously. Wink handedness blends between eyes while a
        // moving gaze crosses the centre, avoiding an eye-swap discontinuity.
        float closure = clampf(eyeOpen / 0.45f, 0.0f, 1.0f);
        float winkWeight = 0.5f - 0.5f * s * _faceHandedness;
        closure *= 1.0f - _eyeWink * winkWeight;
        float smile = _eyeSmile * closure;
        float major = er * lengthScale * (0.62f + 0.38f * eyeOpen) *
                      (1.0f - _eyeRound) * closure * (1.0f - smile);
        float baseDy = major / sqrtf(1.0f + eyeAngle * eyeAngle);
        float baseDx = baseDy * eyeAngle;
        float halfDx = baseDx * twistCos - baseDy * twistSin;
        float halfDy = baseDx * twistSin + baseDy * twistCos;
        float radius = fmaxf(0.9f, er * radiusScale * eyeOpen * closure);
        radius = radius * (1.0f - smile) + er * 0.27f * smile;
        float closedWidth = er * 0.60f * (1.0f - closure);
        float horizontalSign = halfDx < 0.0f ? -1.0f : 1.0f;
        halfDx += horizontalSign * closedWidth;
        if (smile > 0.001f) {
            float curveDx = halfDx + horizontalSign * er * 0.90f * smile;
            if (side <= 48) {
                float maxCurve = fmaxf(0.5f, dx - radius - 1.0f);
                curveDx = horizontalSign * fminf(fabsf(curveDx), maxCurve);
            }
            _fillEyeCurve(ex, ey, curveDx, halfDy,
                          er * 0.46f * smile, radius, _style.eyeColor);
        } else {
            _fillCapsule(ex, ey, halfDx, halfDy, radius, _style.eyeColor);
        }

        if (_style.eyeStyle == EyeStyle::Googly && eyeOpen > 0.34f) {
            int16_t pr = (int16_t)(er * 0.34f);
            if (pr < 1) pr = 1;
            _cv->fillCircle(ex + (int16_t)(_pupilDX * er * 0.32f),
                            ey + (int16_t)(_pupilDY * er * 0.32f), pr, _style.pupilColor);
        }
    }
}

void BotUx::_drawOverlays() {
    if (_signal >= 0) {
        for (int i = 0; i < 4; ++i) {
            int16_t bh = 3 + i * 2;
            int16_t bx = 4 + i * 5;
            int16_t by = 12 - bh;
            uint16_t c = (i < _signal) ? _style.accentColor : mix565(_style.bgColor, _style.accentColor, 44);
            _cv->fillRect(bx, by, 4, bh, c);
        }
    }

    if (_batteryVisible) {
        int16_t bw = (int16_t)(_w * 0.18f);
        if (bw < 18) bw = 18;
        int16_t bh = bw / 2;
        int16_t x0 = _w - bw - 4, y0 = 4;
        _cv->drawRoundRect(x0, y0, bw, bh, 2, _style.accentColor);
        int16_t lvl = (int16_t)((bw - 4) * _battery / 100);
        uint16_t c = (_battery <= 20) ? rgb565(0xFF, 0x4D, 0x4D) : _style.accentColor;
        if (lvl > 0) _cv->fillRect(x0 + 2, y0 + 2, lvl, bh - 4, c);
        _cv->fillRect(x0 + bw, y0 + bh / 4, 2, bh / 2, _style.accentColor);
    }

    _cv->setTextDatum(middle_center);
    _cv->setTextColor(_style.accentColor);
    if (_hasTime) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%02u:%02u", _hh, _mm);
        _cv->setTextSize(1.6f);
        _cv->drawString(buf, _w / 2, _h - 18);
    }
    if (_label) {
        _cv->setTextSize(1.0f);
        _cv->setTextColor(_style.eyeColor);
        _cv->drawString(_label, _w / 2, _h - 6);
    }
}

} // namespace botux

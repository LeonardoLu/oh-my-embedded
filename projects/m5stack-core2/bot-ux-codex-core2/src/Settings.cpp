#include "Settings.h"

#include <M5Unified.h>
#include <Preferences.h>

#include <stdio.h>
#include <string.h>
#include "FeedbackLevel.h"
#include "UxText.h"
#include "UxRender.h"

namespace {
constexpr uint16_t kInk = botux::rgb565(33, 36, 42);
constexpr uint8_t kDisplayBrightness[5] = {40, 88, 144, 204, 255};
constexpr int16_t kRowY[4] = {40, 80, 120, 160};
constexpr int16_t kSliderLeft = 42;
constexpr int16_t kSliderRight = 272;

const char* onOff(uint8_t value) { return value ? "ON" : "OFF"; }

const char* ledName(uint8_t value)
{
    static const char* names[] = {"OFF", "LOW", "MED", "HIGH"};
    return names[value > 3 ? 2 : value];
}

const char* motionName(uint8_t value)
{
    static const char* names[] = {"OFF", "LOW", "MED", "HIGH", "MAX"};
    return names[value > 4 ? 2 : value];
}

const char* botStyleName(uint8_t value)
{
    static const char* names[] = {"CLASSIC", "SOFT", "FACET", "GOOGLY", "MINIMAL"};
    return names[value > 4 ? 0 : value];
}

const char* pageName(uint8_t value)
{
    static const char* names[] = {"GENERAL", "BOT", "BOT COLOR", "MOTION",
                                  "COMPANION", "PREVIEW", "LIGHTS"};
    return names[value < Settings::kPageCount ? value : 0];
}

const char* colorPartName(uint8_t value)
{
    static const char* names[] = {"BODY", "EYES", "ACCENT"};
    return names[value > 2 ? 0 : value];
}

constexpr ux::Rect kKeyboardArea{8, 66, 304, 160};

void text(M5Canvas& cv, const char* value, int x, int y, uint16_t color,
          const ux::Font& font = ux::Latin18)
{
    ux::drawText(cv, value, x, y, color, font);
}

void fittedText(M5Canvas& cv, const char* value, int x, int y, int width,
                uint16_t color, const ux::Font& font = ux::Latin14)
{
    // Keep complete UTF-8 codepoints when a long name or state needs an ellipsis.
    if (ux::textWidth(value, font) <= width) { text(cv, value, x, y, color, font); return; }
    char clipped[96]{};
    size_t length = 0;
    const char* cursor = value;
    while (*cursor && length + 8 < sizeof(clipped))
    {
        const char* begin = cursor;
        ux::nextCodepoint(cursor);
        const size_t bytes = cursor - begin;
        memcpy(clipped + length, begin, bytes);
        memcpy(clipped + length + bytes, "...", 4);
        if (ux::textWidth(clipped, font) > width) break;
        length += bytes;
    }
    memcpy(clipped + length, "...", 4);
    text(cv, clipped, x, y, color, font);
}

void drawRow(M5Canvas& cv, int16_t y, const char* label, const char* value,
             bool down, uint16_t bg, uint16_t fg, uint16_t accent, uint16_t line,
             const ux::Font& valueFont = ux::Latin18)
{
    const uint16_t fill = down ? ux::blend565(bg, accent, 72) : bg;
    ux::roundRect(cv, 6, y + 2, 308, 36, 7, fill);
    ux::strokeRoundRect(cv, 6, y + 2, 308, 36, 7, down ? accent : line);
    text(cv, label, 16, y + 10, fg, ux::Latin14);
    const int valueWidth = ux::textWidth(value, valueFont);
    text(cv, value, 304 - valueWidth, y + (40 - ux::lineHeight(valueFont)) / 2,
         accent, valueFont);
}

void drawSlider(M5Canvas& cv, int16_t y, const char* label, uint8_t value,
                uint8_t channel, uint8_t red, uint8_t green, uint8_t blue,
                uint16_t fg, uint16_t line)
{
    text(cv, label, 12, y + 8, fg);
    for (int16_t x = kSliderLeft; x < kSliderRight; ++x)
    {
        const uint8_t shade = static_cast<uint8_t>(
            (static_cast<uint32_t>(x - kSliderLeft) * 255) / (kSliderRight - kSliderLeft - 1));
        cv.fillRect(x, y + 11, 1, 18, botux::rgb565(channel == 0 ? shade : red,
            channel == 1 ? shade : green, channel == 2 ? shade : blue));
    }
    ux::strokeRoundRect(cv, kSliderLeft - 1, y + 10, kSliderRight - kSliderLeft + 2, 20, 2, line);
    const float marker = kSliderLeft +
        (static_cast<float>(value) * (kSliderRight - kSliderLeft - 1)) / 255;
    ux::roundRect(cv, marker - 2, y + 6, 5, 28, 2.5f, 0xffff);
    ux::line(cv, marker, y + 9, marker, y + 30, 1, kInk);
    char number[4];
    snprintf(number, sizeof(number), "%u", static_cast<unsigned>(value));
    text(cv, number, 314 - ux::textWidth(number, ux::Latin14), y + 10, fg, ux::Latin14);
}

void drawPager(M5Canvas& cv, uint8_t page, int8_t pressed, uint16_t bg,
               uint16_t fg, uint16_t accent)
{
    if (pressed == 5) cv.fillRect(0, 200, 160, 40, bg);
    if (pressed == 6) cv.fillRect(160, 200, 160, 40, bg);
    ux::line(cv, 0, 200.5f, 320, 200.5f, 1, accent);
    ux::line(cv, 37, 215, 31, 221, 2, fg);
    ux::line(cv, 31, 221, 37, 227, 2, fg);
    ux::line(cv, 283, 215, 289, 221, 2, fg);
    ux::line(cv, 289, 221, 283, 227, 2, fg);
    char position[8];
    snprintf(position, sizeof(position), "%u / %u", static_cast<unsigned>(page + 1),
             static_cast<unsigned>(Settings::kPageCount));
    text(cv, position, 160 - ux::textWidth(position) / 2, 209, fg);
}

}

const char* Settings::themeName(uint8_t value)
{
    static const char* names[kThemeCount] = {"PAPER", "WARM", "DARK"};
    return names[value < kThemeCount ? value : 0];
}

Settings::ThemePalette Settings::themePalette(uint8_t value)
{
    static constexpr ThemePalette palettes[kThemeCount] = {
        {botux::rgb565(234, 240, 249), botux::rgb565(255, 255, 252),
         botux::rgb565(207, 219, 238), botux::rgb565(102, 119, 147),
         botux::rgb565(12, 24, 42), botux::rgb565(64, 79, 102),
         botux::rgb565(0, 92, 232)},
        {botux::rgb565(255, 222, 177), botux::rgb565(255, 247, 229),
         botux::rgb565(242, 179, 103), botux::rgb565(151, 79, 34),
         botux::rgb565(55, 27, 16), botux::rgb565(112, 57, 31),
         botux::rgb565(211, 49, 0)},
        {botux::rgb565(7, 12, 24), botux::rgb565(22, 31, 50),
         botux::rgb565(43, 59, 91), botux::rgb565(116, 143, 187),
         botux::rgb565(255, 252, 242), botux::rgb565(190, 205, 229),
         botux::rgb565(54, 165, 255)},
    };
    return palettes[value < kThemeCount ? value : 0];
}

botux::BotUx::Style Settings::themeStyle(uint8_t value)
{
    const ThemePalette palette = themePalette(value);
    botux::BotUx::Style style;
    style.bgColor = palette.background;
    style.bodyColor = botux::rgb565(252, 252, 250);
    style.accentColor = palette.accent;
    style.eyeColor = kInk;
    style.pupilColor = botux::rgb565(12, 14, 18);
    style.mouthColor = kInk;
    style.blushColor = botux::rgb565(238, 130, 126);
    style.eyeStyle = botux::BotUx::EyeStyle::Oval;
    style.bodyStyle = botux::BotUx::BodyStyle::Round;
    return style;
}

botux::BotUx::Style Settings::botStyle() const
{
    botux::BotUx::Style style = themeStyle(_data.theme);
    style.bodyColor = botux::rgb565(_data.bodyRed, _data.bodyGreen, _data.bodyBlue);
    style.eyeColor = botux::rgb565(_data.eyeRed, _data.eyeGreen, _data.eyeBlue);
    const uint16_t eyeLuma = static_cast<uint16_t>(_data.eyeRed) * 3
                           + static_cast<uint16_t>(_data.eyeGreen) * 6
                           + _data.eyeBlue;
    style.pupilColor = eyeLuma >= 1280
        ? botux::rgb565(12, 14, 18) : botux::rgb565(252, 252, 250);
    style.mouthColor = style.eyeColor;
    style.accentColor = botux::rgb565(_data.accentRed, _data.accentGreen, _data.accentBlue);
    style.blushColor = style.accentColor;
    switch (_data.botStyle)
    {
        case 1:
            style.bodyStyle = botux::BotUx::BodyStyle::RoundedSquare;
            style.eyeStyle = botux::BotUx::EyeStyle::Round;
            break;
        case 2:
            style.bodyStyle = botux::BotUx::BodyStyle::Hexagon;
            style.eyeStyle = botux::BotUx::EyeStyle::Square;
            break;
        case 3:
            style.bodyStyle = botux::BotUx::BodyStyle::Round;
            style.eyeStyle = botux::BotUx::EyeStyle::Googly;
            break;
        case 4:
            style.bodyStyle = botux::BotUx::BodyStyle::None;
            style.eyeStyle = botux::BotUx::EyeStyle::Oval;
            style.eyeSize = 1.1f;
            break;
        default: break;
    }
    return style;
}

void Settings::begin()
{
    Preferences prefs;
    prefs.begin("codexmicro", true);
    _data.theme = prefs.getUChar("theme", 0);
    _data.audio = prefs.getUChar("audio", 1);
    _data.brightness = prefs.getUChar("display", 3);
    _data.reducedMotion = prefs.getUChar("reduce", 0);
    _data.ledBrightness = prefs.getUChar("led", 2);
    _data.animation = prefs.getUChar("anim", 0);
    _data.motion = prefs.getUChar("motion", 2);
    _data.botStyle = prefs.getUChar("botstyle", 0);
    _data.expression = prefs.getUChar("expr", 0);
    _data.bodyRed = prefs.getUChar("bodyr", 252);
    _data.bodyGreen = prefs.getUChar("bodyg", 252);
    _data.bodyBlue = prefs.getUChar("bodyb", 250);
    _data.eyeRed = prefs.getUChar("eyer", 33);
    _data.eyeGreen = prefs.getUChar("eyeg", 36);
    _data.eyeBlue = prefs.getUChar("eyeb", 42);
    _data.accentRed = prefs.getUChar("accentr", 60);
    _data.accentGreen = prefs.getUChar("accentg", 124);
    _data.accentBlue = prefs.getUChar("accentb", 232);
    prefs.getString("botname", _data.botName, sizeof(_data.botName));
    _data.language = prefs.getUChar("language", 0);
    _data.ledMode = prefs.getUChar("ledmode", 2);
    _data.notifications = prefs.getUChar("notify", 1);
    _data.gaze = prefs.getUChar("gaze", 0);
    _data.volume = prefs.getUChar("volume", corefeedback::kDefaultLevel);
    prefs.end();
    // Reuse BotUx's public sanitizer so persisted and displayed names agree.
    _preview.setName(_data.botName);
    snprintf(_data.botName, sizeof(_data.botName), "%s", _preview.name());
    if (_data.language > 1) _data.language = 0;
    if (_data.ledMode > 2) _data.ledMode = 2;
    if (_data.notifications > 1) _data.notifications = 1;
    if (_data.gaze >= botux::BotUx::gazeDirectionCount()) _data.gaze = 0;
    _data.volume = corefeedback::normalizeLevel(_data.volume);
    if (!_previewAttempted)
    {
        _previewAttempted = true;
        _previewSprite.setColorDepth(16);
        _previewReady = _previewSprite.createSprite(112, 112) != nullptr;
        if (_previewReady)
        {
            _preview.begin(&_previewSprite);
            _preview.setBatteryVisible(false);
            _preview.setSignal(-1);
        }
    }
    if (_data.theme >= kThemeCount) _data.theme = 0;
    if (_data.audio > 1) _data.audio = 1;
    if (_data.brightness < 1 || _data.brightness > 5) _data.brightness = 3;
    if (_data.reducedMotion > 1) _data.reducedMotion = 0;
    if (_data.ledBrightness > 3) _data.ledBrightness = 2;
    if (_data.animation > 7) _data.animation = 0;
    if (_data.motion > 4) _data.motion = 2;
    if (_data.botStyle > 4) _data.botStyle = 0;
    if (_data.expression > 9) _data.expression = 0;
}

void Settings::_save() const
{
    Preferences prefs;
    prefs.begin("codexmicro", false);
    prefs.putUChar("theme", _data.theme);
    prefs.putUChar("audio", _data.audio);
    prefs.putUChar("display", _data.brightness);
    prefs.putUChar("reduce", _data.reducedMotion);
    prefs.putUChar("led", _data.ledBrightness);
    prefs.putUChar("anim", _data.animation);
    prefs.putUChar("motion", _data.motion);
    prefs.putUChar("botstyle", _data.botStyle);
    prefs.putUChar("expr", _data.expression);
    prefs.putUChar("bodyr", _data.bodyRed);
    prefs.putUChar("bodyg", _data.bodyGreen);
    prefs.putUChar("bodyb", _data.bodyBlue);
    prefs.putUChar("eyer", _data.eyeRed);
    prefs.putUChar("eyeg", _data.eyeGreen);
    prefs.putUChar("eyeb", _data.eyeBlue);
    prefs.putUChar("accentr", _data.accentRed);
    prefs.putUChar("accentg", _data.accentGreen);
    prefs.putUChar("accentb", _data.accentBlue);
    prefs.putString("botname", _data.botName);
    prefs.putUChar("language", _data.language);
    prefs.putUChar("ledmode", _data.ledMode);
    prefs.putUChar("notify", _data.notifications);
    prefs.putUChar("gaze", _data.gaze);
    prefs.putUChar("volume", _data.volume);
    prefs.end();
}

void Settings::apply(botux::BotUx& bot) const
{
    bot.setName(_data.botName);
    bot.setStyle(botStyle());
    bot.setExpression(static_cast<botux::BotUx::Expression>(_data.expression), 650);
    bot.setAnimation(static_cast<botux::BotUx::Animation>(_data.animation));
    bot.setAnimationSpeed(1.0f);
    bot.setMotionAmount(_data.motion * 0.5f);
    bot.setReducedMotion(_data.reducedMotion != 0);
    bot.setGazeDirection(static_cast<botux::BotUx::GazeDirection>(_data.gaze));
    applyBrightness();
}

void Settings::applyBrightness() const
{
    M5.Display.setBrightness(kDisplayBrightness[_data.brightness - 1]);
}

void Settings::open(uint8_t page)
{
    _open = true;
    _page = page <= kNamePage ? page : 0;
    if (_page == kNamePage) _nameEditor.begin(_data.botName);
    _previewNow = 0;
    _pressed = -1;
    _colorChanged = false;
}

void Settings::close()
{
    _open = false;
    _pressed = -1;
    _colorChanged = false;
}

int8_t Settings::_hit(int16_t x, int16_t y) const
{
    if (x < 0 || x >= 320 || y < 0 || y >= 240) return -1;
    if (_page == kNamePage)
    {
        if (y < 40 && x < 80) return 50;
        if (y < 40 && x >= 240) return 51;
        const int key = ux::nameKeyAt(kKeyboardArea, x, y);
        return key < 0 ? -1 : 20 + key;
    }
    if (x >= 244 && y < 40) return 0;
    if (y >= 200) return x < 160 ? 5 : 6;
    if (_page == 5)
    {
        if (x < 124 || x >= 314) return -1;
        for (int8_t i = 0; i < 3; ++i)
            if (y >= 44 + i * 50 && y < 88 + i * 50) return i + 1;
        return -1;
    }
    const int8_t rowCount = (_page == 3 || _page == 4 || _page == 6) ? 3 : 4;
    for (int8_t i = 0; i < rowCount; ++i)
        if (y >= kRowY[i] && y < kRowY[i] + 40) return i + 1;
    return -1;
}

uint8_t& Settings::_colorChannel(uint8_t channel)
{
    uint8_t* colors[] = {
        &_data.bodyRed, &_data.bodyGreen, &_data.bodyBlue,
        &_data.eyeRed, &_data.eyeGreen, &_data.eyeBlue,
        &_data.accentRed, &_data.accentGreen, &_data.accentBlue,
    };
    return *colors[_colorPart * 3 + channel];
}

bool Settings::_setColor(uint8_t channel, int16_t x)
{
    if (x < kSliderLeft) x = kSliderLeft;
    if (x >= kSliderRight) x = kSliderRight - 1;
    const uint8_t value = static_cast<uint8_t>(
        (static_cast<uint32_t>(x - kSliderLeft) * 255) / (kSliderRight - kSliderLeft - 1));
    uint8_t& current = _colorChannel(channel);
    if (current == value) return false;
    current = value;
    return true;
}

void Settings::touchBegin(int16_t x, int16_t y)
{
    _pressed = _hit(x, y);
    _colorChanged = false;
    if (_page == 2 && _pressed >= 2 && _pressed <= 4)
    {
        _colorOriginal = _colorChannel(static_cast<uint8_t>(_pressed - 2));
        _colorChanged = _setColor(static_cast<uint8_t>(_pressed - 2), x);
    }
}

bool Settings::touchMove(int16_t x, int16_t y)
{
    if (_pressed < 0) return false;
    const int8_t target = _hit(x, y);
    if (_page == 2 && _pressed >= 2 && _pressed <= 4)
    {
        const uint8_t channel = static_cast<uint8_t>(_pressed - 2);
        if (target == _pressed)
        {
            const bool changed = _setColor(channel, x);
            _colorChanged |= changed;
            return changed;
        }
        _colorChannel(channel) = _colorOriginal;
        _colorChanged = false;
        _pressed = -1;
        return true;
    }
    if (target == _pressed) return false;
    _pressed = -1;
    return true;
}

void Settings::touchEnd(int16_t x, int16_t y)
{
    const int8_t released = _hit(x, y);
    if (_page == 2 && _pressed >= 2 && _pressed <= 4)
    {
        if (released == _pressed)
        {
            if (_colorChanged) _save();
        }
        else
        {
            _colorChannel(static_cast<uint8_t>(_pressed - 2)) = _colorOriginal;
        }
        _colorChanged = false;
        _pressed = -1;
        return;
    }
    if (_pressed >= 0 && released == _pressed) _activate(_pressed);
    _pressed = -1;
}

void Settings::_commitName()
{
    _nameEditor.press(ux::NameEditor::Done);
    snprintf(_data.botName, sizeof(_data.botName), "%s", _nameEditor.text());
    _save();
    _page = 4;
}

void Settings::_activate(int8_t target)
{
    if (_page == kNamePage)
    {
        if (target == 50) { _page = 4; return; }
        if (target == 51) { _commitName(); return; }
        if (target >= 20 && target < 50 && _nameEditor.press(target - 20)) _commitName();
        return;
    }
    if (target == 0) { close(); return; }
    if (target == 5) { _page = (_page + kPageCount - 1) % kPageCount; return; }
    if (target == 6) { _page = (_page + 1) % kPageCount; return; }
    if (_page == 0)
    {
        if (target == 1) _data.brightness = (_data.brightness % 5) + 1;
        else if (target == 2) _data.audio ^= 1;
        else if (target == 3) _data.volume = (_data.volume + 1) % corefeedback::kLevelCount;
        else if (target == 4) _data.theme = (_data.theme + 1) % kThemeCount;
    }
    else if (_page == 1)
    {
        if (target == 1) _data.botStyle = (_data.botStyle + 1) % 5;
        else if (target == 2) _data.expression = (_data.expression + 1) % botux::BotUx::expressionCount();
        else if (target == 3) _data.animation = (_data.animation + 1) % botux::BotUx::animationCount();
        else if (target == 4) { _page = 2; return; }
    }
    else if (_page == 2)
    {
        if (target == 1) { _colorPart = (_colorPart + 1) % 3; return; }
    }
    else if (_page == 3)
    {
        if (target == 1) _data.motion = (_data.motion + 1) % 5;
        else if (target == 2) _data.reducedMotion ^= 1;
        else if (target == 3) _data.gaze = (_data.gaze + 1) % botux::BotUx::gazeDirectionCount();
    }
    else if (_page == 4)
    {
        if (target == 1) { _nameEditor.begin(_data.botName); _page = kNamePage; return; }
        else if (target == 2) _data.language ^= 1;
        else if (target == 3) { _page = 5; return; }
    }
    else if (_page == 5)
    {
        if (target == 1) _previewMood = (_previewMood + 1) % botux::BotUx::moodCount();
        else if (target == 2) _previewExpression = (_previewExpression + 1) % botux::BotUx::expressionCount();
        else if (target == 3) _previewAnimation = (_previewAnimation + 1) % botux::BotUx::animationCount();
        return; // Local preview choices never persist or alter the host Bot.
    }
    else if (_page == 6)
    {
        if (target == 1) _data.ledMode = (_data.ledMode + 1) % 3;
        else if (target == 2) _data.notifications ^= 1;
        else if (target == 3) _data.ledBrightness = (_data.ledBrightness + 1) % 4;
    }
    _save();
    applyBrightness();
}

bool Settings::animate(uint32_t nowMs)
{
    if (!_open || _page != 5 || !_previewReady || nowMs - _previewNow < 33) return false;
    _previewNow = nowMs;
    return true;
}

bool Settings::drawAnimatedPreview(lgfx::LovyanGFX& target)
{
    if (!_open || _page != 5 || !_previewReady) return false;
    const auto mood = static_cast<botux::BotUx::Mood>(_previewMood);
    _preview.setStyle(botStyle());
    _preview.setName(_data.botName);
    _preview.setMood(mood);
    _preview.setExpression(static_cast<botux::BotUx::Expression>(_previewExpression));
    _preview.setAnimation(static_cast<botux::BotUx::Animation>(_previewAnimation));
    _preview.setTalking(mood == botux::BotUx::Mood::Speaking);
    _preview.setMotionAmount(_data.motion * 0.5f);
    _preview.setReducedMotion(_data.reducedMotion != 0);
    _preview.setGazeDirection(static_cast<botux::BotUx::GazeDirection>(_data.gaze));
    _preview.update(_previewNow ? _previewNow : millis());
    _preview.draw();
    const ux::Rect region = previewRect();
    _previewSprite.pushSprite(&target, region.x, region.y);
    return true;
}

void Settings::_drawPreview(M5Canvas& cv, uint16_t fg, uint16_t rowBg, uint16_t line)
{
    const auto language = static_cast<botux::BotUx::Language>(_data.language);
    const ux::Font& font = _data.language ? ux::Cjk18 : ux::Latin18;
    const auto mood = static_cast<botux::BotUx::Mood>(_previewMood);
    const auto expression = static_cast<botux::BotUx::Expression>(_previewExpression);
    const auto animation = static_cast<botux::BotUx::Animation>(_previewAnimation);
    if (!drawAnimatedPreview(cv)) text(cv, "No preview", 8, 88, fg, ux::Latin14);
    fittedText(cv, _data.botName, 8, 156, 108, fg);
    text(cv, botux::BotUx::moodName(mood, language), 8, 176, fg, font);
    const char* labels[] = {"MOOD", "EXPRESSION", "ANIMATION"};
    const char* values[] = {botux::BotUx::moodName(mood, language),
        botux::BotUx::expressionName(expression, language),
        botux::BotUx::animationName(animation, language)};
    const uint8_t indices[] = {_previewMood, _previewExpression, _previewAnimation};
    const uint8_t counts[] = {botux::BotUx::moodCount(), botux::BotUx::expressionCount(), botux::BotUx::animationCount()};
    for (int i = 0; i < 3; ++i)
    {
        const int y = 44 + i * 50;
        ux::roundRect(cv, 124, y, 190, 44, 7, _pressed == i + 1 ? ux::blend565(rowBg, fg, 32) : rowBg);
        ux::strokeRoundRect(cv, 124, y, 190, 44, 7, line);
        char label[28];
        snprintf(label, sizeof(label), "%s %u/%u", labels[i], indices[i] + 1, counts[i]);
        text(cv, label, 134, y + 2, fg, ux::Latin14);
        text(cv, values[i], 134, y + 20, fg, font);
        text(cv, "+", 292, y + 14, fg);
    }
}

void Settings::draw(M5Canvas& cv, M5Canvas& botSprite)
{
    (void)botSprite; // The local preview has its own sprite; never borrow host state.
    const botux::BotUx::Style style = themeStyle(_data.theme);
    const ThemePalette palette = themePalette(_data.theme);
    const uint16_t fg = palette.text;
    const uint16_t rowBg = palette.surface;
    // UI contrast is independent of personalized Bot body/eye colors.
    const uint16_t accent = palette.accent;
    const uint16_t line = palette.outline;
    const auto language = static_cast<botux::BotUx::Language>(_data.language);
    const ux::Font& stateFont = _data.language ? ux::Cjk18 : ux::Latin18;
    cv.fillSprite(style.bgColor);
    if (_page == kNamePage)
    {
        ux::roundRect(cv, 4, 2, 76, 36, 7, _pressed == 50 ? line : rowBg);
        ux::roundRect(cv, 240, 2, 76, 36, 7, _pressed == 51 ? line : rowBg);
        text(cv, "Cancel", 12, 8, fg);
        text(cv, "NAME", 160 - ux::textWidth("NAME") / 2, 8, fg);
        text(cv, "Done", 252, 8, accent);
        const ux::Font& nameFont = ux::textWidth(_nameEditor.text(), ux::Latin18) <= 248
            ? ux::Latin18 : ux::Latin14;
        text(cv, _nameEditor.text(), 12, 40, fg, nameFont);
        char count[8];
        snprintf(count, sizeof(count), "%u/16", static_cast<unsigned>(strlen(_nameEditor.text())));
        text(cv, count, 312 - ux::textWidth(count, ux::Latin14), 42, fg, ux::Latin14);
        ux::drawNameKeyboard(cv, _nameEditor, kKeyboardArea, rowBg, fg, ux::Latin14,
                             _pressed >= 20 && _pressed < 50 ? _pressed - 20 : -1);
        return;
    }
    text(cv, pageName(_page), 12, 8, fg);
    if (_page == 5) text(cv, "12 x 10 x 8", 116, 11, accent, ux::Latin14);
    ux::roundRect(cv, 276, 2, 40, 36, 8, _pressed == 0 ? accent : rowBg);
    const uint16_t closeColor = _pressed == 0 ? 0xffff : fg;
    ux::line(cv, 289, 13, 303, 27, 2, closeColor);
    ux::line(cv, 303, 13, 289, 27, 2, closeColor);
    if (_page == 0)
    {
        char level[2] = {static_cast<char>('0' + _data.brightness), 0};
        char volume[6];
        snprintf(volume, sizeof(volume), "%u / 5", static_cast<unsigned>(_data.volume));
        drawRow(cv, kRowY[0], "DISPLAY", level, _pressed == 1, rowBg, fg, accent, line);
        drawRow(cv, kRowY[1], "AUDIO FEEDBACK", onOff(_data.audio), _pressed == 2, rowBg, fg, accent, line);
        drawRow(cv, kRowY[2], "VOLUME", volume, _pressed == 3, rowBg, fg, accent, line);
        drawRow(cv, kRowY[3], "THEME", themeName(_data.theme), _pressed == 4, rowBg, fg, accent, line);
    }
    else if (_page == 1)
    {
        drawRow(cv, kRowY[0], "STYLE", botStyleName(_data.botStyle), _pressed == 1, rowBg, fg, accent, line);
        drawRow(cv, kRowY[1], "EXPRESSION", botux::BotUx::expressionName(static_cast<botux::BotUx::Expression>(_data.expression), language), _pressed == 2, rowBg, fg, accent, line, stateFont);
        drawRow(cv, kRowY[2], "ANIMATION", botux::BotUx::animationName(static_cast<botux::BotUx::Animation>(_data.animation), language), _pressed == 3, rowBg, fg, accent, line, stateFont);
        drawRow(cv, kRowY[3], "COLOR", "EDIT", _pressed == 4, rowBg, fg, accent, line);
    }
    else if (_page == 2)
    {
        const uint8_t red = _colorPart == 0 ? _data.bodyRed : (_colorPart == 1 ? _data.eyeRed : _data.accentRed);
        const uint8_t green = _colorPart == 0 ? _data.bodyGreen : (_colorPart == 1 ? _data.eyeGreen : _data.accentGreen);
        const uint8_t blue = _colorPart == 0 ? _data.bodyBlue : (_colorPart == 1 ? _data.eyeBlue : _data.accentBlue);
        drawRow(cv, kRowY[0], "COLOR PART", colorPartName(_colorPart), _pressed == 1, rowBg, fg, accent, line);
        ux::roundRect(cv, 190, 49, 28, 22, 5, botux::rgb565(red, green, blue));
        ux::strokeRoundRect(cv, 190, 49, 28, 22, 5, line);
        drawSlider(cv, kRowY[1], "R", red, 0, red, green, blue, fg, line);
        drawSlider(cv, kRowY[2], "G", green, 1, red, green, blue, fg, line);
        drawSlider(cv, kRowY[3], "B", blue, 2, red, green, blue, fg, line);
    }
    else if (_page == 3)
    {
        drawRow(cv, kRowY[0], "IMU MOTION", motionName(_data.motion), _pressed == 1, rowBg, fg, accent, line);
        drawRow(cv, kRowY[1], "REDUCED MOTION", onOff(_data.reducedMotion), _pressed == 2, rowBg, fg, accent, line);
        drawRow(cv, kRowY[2], "GAZE", botux::BotUx::gazeDirectionName(
            static_cast<botux::BotUx::GazeDirection>(_data.gaze), language),
            _pressed == 3, rowBg, fg, accent, line, stateFont);
        text(cv, "Tilt, shake and eye direction", 16, 174, fg, ux::Latin14);
    }
    else if (_page == 4)
    {
        drawRow(cv, kRowY[0], "NAME", _data.botName, _pressed == 1, rowBg, fg, accent, line, ux::Latin14);
        drawRow(cv, kRowY[1], "LANGUAGE", _data.language ? "中文" : "English", _pressed == 2, rowBg, fg, accent, line, stateFont);
        drawRow(cv, kRowY[2], "COMBINATIONS", "PREVIEW", _pressed == 3, rowBg, fg, accent, line);
        text(cv, _data.language ? "状态 / 表情 / 动作" : "Mood / expression / animation", 16, 174, fg,
             _data.language ? ux::Cjk18 : ux::Latin14);
    }
    else if (_page == 5) _drawPreview(cv, fg, rowBg, line);
    else if (_page == 6)
    {
        static const char* modes[] = {"OFF", "STATUS", "ALIVE"};
        drawRow(cv, kRowY[0], "LED MODE", modes[_data.ledMode], _pressed == 1, rowBg, fg, accent, line);
        drawRow(cv, kRowY[1], "NOTIFICATIONS", onOff(_data.notifications), _pressed == 2, rowBg, fg, accent, line);
        drawRow(cv, kRowY[2], "LED BRIGHTNESS", ledName(_data.ledBrightness), _pressed == 3, rowBg, fg, accent, line);
        const char* notes[] = {"Bottom lights stay off", "Agent colors + state rhythm", "State rhythm + interactions"};
        text(cv, notes[_data.ledMode], 16, 171, fg, ux::Latin14);
    }
    drawPager(cv, _page, _pressed, rowBg, fg, accent);
}

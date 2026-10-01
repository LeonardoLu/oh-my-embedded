#include "Settings.h"
#include "WatchInteraction.h"

namespace {
struct ThemeColors {
    uint16_t bg, body, accent, eye, pupil, mouth, blush;
    uint16_t ink, muted, panel, warning;
};

const ThemeColors kThemes[Settings::THEME_COUNT] = {
    { botux::rgb565(0x08,0x0B,0x10), botux::rgb565(0xF2,0xF0,0xE8), botux::rgb565(0x55,0x8D,0xFF),
      botux::rgb565(0x19,0x22,0x32), botux::rgb565(0x19,0x22,0x32), botux::rgb565(0x19,0x22,0x32),
      botux::rgb565(0xFA,0x9A,0xA8), botux::rgb565(0xF7,0xF8,0xFA), botux::rgb565(0x98,0xA2,0xB3),
      botux::rgb565(0x18,0x20,0x2D), botux::rgb565(0xFF,0x5F,0x68) },
    { botux::rgb565(0x1A,0x10,0x17), botux::rgb565(0xF5,0xEC,0xDE), botux::rgb565(0xFF,0x9B,0x62),
      botux::rgb565(0x2A,0x1D,0x27), botux::rgb565(0x2A,0x1D,0x27), botux::rgb565(0x2A,0x1D,0x27),
      botux::rgb565(0xFA,0x95,0xA0), botux::rgb565(0xFF,0xF7,0xED), botux::rgb565(0xB8,0x9E,0xA9),
      botux::rgb565(0x35,0x20,0x2B), botux::rgb565(0xFF,0x68,0x68) },
    { botux::rgb565(0x0C,0x0D,0x10), botux::rgb565(0xEC,0xEC,0xE8), botux::rgb565(0xC7,0xCF,0xDC),
      botux::rgb565(0x19,0x1B,0x20), botux::rgb565(0x19,0x1B,0x20), botux::rgb565(0x19,0x1B,0x20),
      botux::rgb565(0x9A,0x9A,0x9A), botux::rgb565(0xF4,0xF4,0xF2), botux::rgb565(0x9B,0x9E,0xA5),
      botux::rgb565(0x25,0x27,0x2C), botux::rgb565(0xFF,0x62,0x62) },
};

const char* kThemeNames[Settings::THEME_COUNT] = { "Night", "Dusk", "Mono" };
const char* kAppearanceNames[Settings::APPEARANCE_COUNT] = { "Orb", "Bean", "Pebble" };
const char* kEyeStyleNames[Settings::EYE_STYLE_COUNT] = { "Round", "Oval", "Square", "Googly" };
const char* kExpressionNames[Settings::EXPRESSION_COUNT] = {
    "Auto", "Neutral", "Curious", "Focused", "Joy",
    "Skeptical", "Bashful", "Wink", "Dizzy", "Alarmed"
};
const char* kAnimationNames[Settings::ANIMATION_COUNT] = {
    "Auto", "Calm", "Curious", "Orbit", "Bounce", "Glitch", "Wave", "Sparkle"
};
const char* kNamespace = "watch";
const char* kDimTimeoutKey = "dimTimeout";
// ESP NVS keys are limited to 15 characters; keep this distinct from the
// public Settings::Data::screenOffTimeout field without altering old keys.
const char* kScreenOffTimeoutKey = "screenOffTo";
const char* kButtonWakeOnlyKey = "buttonWake";
const char* kWakeModeKey = "wakeMode";
const char* kPowerSaveKey = "powerSave";
const char* kDimBrightnessKey = "dimBright";
const char* kChargeSaveKey = "chargeSave";
const char* kChargeAwakeKey = "chargeAwake";
const char* kForcedSleepKey = "forceSleep";
const char* kForcedStartKey = "sleepStart";
const char* kForcedEndKey = "sleepEnd";
} // namespace

void Settings::begin() {
    Preferences prefs;
    prefs.begin(kNamespace, false);
    prefs.getString("botName", "Milo").toCharArray(_data.botName, sizeof(_data.botName));
    _data.language = prefs.getUChar("language", 0) == 1 ? 1 : 0;
    _data.indicator = prefs.getBool("indicator", false);
    _data.gaze = prefs.getUChar("gaze", 0);
    if (_data.gaze >= botux::BotUx::gazeDirectionCount()) _data.gaze = 0;
    _data.showDescription = prefs.getBool("description", true);
    _data.swapLayout = prefs.getBool("swapLayout", false);
    _data.hour24      = prefs.getBool("hour24", _data.hour24);
    _data.showSeconds = prefs.getBool("seconds", _data.showSeconds);
    _data.sound       = prefs.getBool("sound", _data.sound);
    _data.startupSound = prefs.getBool("startupSound", _data.startupSound);
    _data.buttonSound = prefs.getBool("buttonSound", _data.buttonSound);
    _data.alertSound = prefs.getBool("alertSound", _data.alertSound);
    _data.buttonFeedback = prefs.getBool("buttonFx", _data.buttonFeedback);
    _data.theme       = prefs.getUChar("theme", _data.theme);
    _data.appearance  = prefs.getUChar("look", _data.appearance);
    _data.expression  = prefs.getUChar("expr", _data.expression);
    _data.animation   = prefs.getUChar("anim", _data.animation);
    _data.brightness  = prefs.getUChar("bright", _data.brightness);
    _data.powerSaveEnabled = prefs.getBool(kPowerSaveKey, _data.powerSaveEnabled);
    _data.dimBrightness = prefs.getUChar(kDimBrightnessKey, _data.dimBrightness);
    _data.dimTimeout = prefs.getUChar(kDimTimeoutKey, _data.dimTimeout);
    _data.screenOffTimeout = prefs.getUChar(kScreenOffTimeoutKey, _data.screenOffTimeout);
    const bool legacyButtonWakeOnly = prefs.getBool(kButtonWakeOnlyKey, false);
    _data.wakeMode = prefs.getUChar(kWakeModeKey,
        legacyButtonWakeOnly ? WAKE_KEYS_ONLY : WAKE_TOUCH_KEYS);
    // The earlier unpublished setting expressed the inverse choice.
    const bool legacyChargeSave = prefs.getBool(kChargeSaveKey, false);
    _data.keepAwakeWhileCharging = prefs.getBool(kChargeAwakeKey,
        !legacyChargeSave);
    _data.forcedSleepEnabled = prefs.getBool(kForcedSleepKey,
        _data.forcedSleepEnabled);
    _data.forcedSleepStartHour = prefs.getUChar(kForcedStartKey,
        _data.forcedSleepStartHour);
    _data.forcedSleepEndHour = prefs.getUChar(kForcedEndKey,
        _data.forcedSleepEndHour);
    _data.motion      = prefs.getBool("motion", _data.motion);
    _data.eyeStyle    = prefs.getUChar("eyes", _data.eyeStyle);
    _data.customColor = prefs.getBool("custom", _data.customColor);
    _data.colorHue    = prefs.getUShort("hue", _data.colorHue);
    _data.colorSat    = prefs.getUChar("sat", _data.colorSat);
    _data.colorValue  = prefs.getUChar("value", _data.colorValue);
    _data.motionAmount = prefs.getUChar("amount", _data.motionAmount);
    _data.animationSpeed = prefs.getUChar("speed", _data.animationSpeed);
    prefs.end();

    if (_data.theme >= THEME_COUNT) _data.theme = THEME_NIGHT;
    if (_data.appearance >= APPEARANCE_COUNT) _data.appearance = 0;
    if (_data.expression >= EXPRESSION_COUNT) _data.expression = 0;
    if (_data.animation >= ANIMATION_COUNT) _data.animation = 1;
    if (_data.brightness < BRIGHTNESS_MIN || _data.brightness > BRIGHTNESS_MAX)
        _data.brightness = 3;
    if (_data.dimBrightness < BRIGHTNESS_MIN || _data.dimBrightness > BRIGHTNESS_MAX)
        _data.dimBrightness = BRIGHTNESS_MIN;
    if (_data.dimBrightness > _data.brightness)
        _data.dimBrightness = _data.brightness;
    _data.dimTimeout = timeoutIndex(_data.dimTimeout, 1);
    _data.screenOffTimeout = timeoutIndex(_data.screenOffTimeout, 2);
    if (_data.wakeMode >= WAKE_MODE_COUNT) _data.wakeMode = WAKE_TOUCH_KEYS;
    if (_data.forcedSleepStartHour > 23) _data.forcedSleepStartHour = 23;
    if (_data.forcedSleepEndHour > 23) _data.forcedSleepEndHour = 7;
    if (_data.eyeStyle >= EYE_STYLE_COUNT) _data.eyeStyle = 1;
    if (_data.colorHue > 359) _data.colorHue = 42;
    if (_data.colorSat > 100) _data.colorSat = 8;
    if (_data.colorValue > 100) _data.colorValue = 95;
    if (_data.motionAmount < 1 || _data.motionAmount > 5) _data.motionAmount = 2;
    if (_data.animationSpeed < 1 || _data.animationSpeed > 5) _data.animationSpeed = 2;

    rebuildStyle();
}

void Settings::save() {
    if (_data.dimBrightness > _data.brightness)
        _data.dimBrightness = _data.brightness;
    Preferences prefs;
    prefs.begin(kNamespace, false);
    prefs.putString("botName", _data.botName);
    prefs.putUChar("language", _data.language);
    prefs.putUChar("gaze", _data.gaze);
    prefs.putBool("description", _data.showDescription);
    prefs.putBool("swapLayout", _data.swapLayout);
    prefs.putBool("hour24", _data.hour24);
    prefs.putBool("seconds", _data.showSeconds);
    prefs.putBool("sound", _data.sound);
    prefs.putBool("startupSound", _data.startupSound);
    prefs.putBool("buttonSound", _data.buttonSound);
    prefs.putBool("alertSound", _data.alertSound);
    prefs.putBool("indicator", _data.indicator);
    prefs.putBool("buttonFx", _data.buttonFeedback);
    prefs.putUChar("theme", _data.theme);
    prefs.putUChar("look", _data.appearance);
    prefs.putUChar("expr", _data.expression);
    prefs.putUChar("anim", _data.animation);
    prefs.putUChar("bright", _data.brightness);
    prefs.putBool(kPowerSaveKey, _data.powerSaveEnabled);
    prefs.putUChar(kDimBrightnessKey, _data.dimBrightness);
    prefs.putUChar(kDimTimeoutKey, _data.dimTimeout);
    prefs.putUChar(kScreenOffTimeoutKey, _data.screenOffTimeout);
    prefs.putUChar(kWakeModeKey, _data.wakeMode);
    prefs.putBool(kButtonWakeOnlyKey, _data.wakeMode == WAKE_KEYS_ONLY);
    prefs.putBool(kChargeAwakeKey, _data.keepAwakeWhileCharging);
    prefs.putBool(kChargeSaveKey, !_data.keepAwakeWhileCharging);
    prefs.putBool(kForcedSleepKey, _data.forcedSleepEnabled);
    prefs.putUChar(kForcedStartKey, _data.forcedSleepStartHour);
    prefs.putUChar(kForcedEndKey, _data.forcedSleepEndHour);
    prefs.putBool("motion", _data.motion);
    prefs.putUChar("eyes", _data.eyeStyle);
    prefs.putBool("custom", _data.customColor);
    prefs.putUShort("hue", _data.colorHue);
    prefs.putUChar("sat", _data.colorSat);
    prefs.putUChar("value", _data.colorValue);
    prefs.putUChar("amount", _data.motionAmount);
    prefs.putUChar("speed", _data.animationSpeed);
    prefs.end();
}

void Settings::rebuildStyle() {
    const ThemeColors& c = kThemes[_data.theme];
    botux::BotUx::Style s;
    s.bgColor     = c.bg;
    s.bodyColor   = _data.customColor ? hsv565(_data.colorHue, _data.colorSat, _data.colorValue) : c.body;
    s.accentColor = c.accent;
    s.eyeColor    = c.eye;
    s.pupilColor  = c.pupil;
    s.mouthColor  = c.mouth;
    s.blushColor  = c.blush;
    s.eyeStyle = (botux::BotUx::EyeStyle)_data.eyeStyle;
    static const botux::BotUx::BodyStyle bodies[APPEARANCE_COUNT] = {
        botux::BotUx::BodyStyle::Round,
        botux::BotUx::BodyStyle::RoundedSquare,
        botux::BotUx::BodyStyle::Hexagon,
    };
    s.bodyStyle = bodies[_data.appearance];
    _style = s;
}

const char* Settings::themeName(uint8_t idx) {
    return (idx < THEME_COUNT) ? kThemeNames[idx] : "?";
}

const char* Settings::appearanceName(uint8_t idx) {
    return (idx < APPEARANCE_COUNT) ? kAppearanceNames[idx] : "?";
}

const char* Settings::eyeStyleName(uint8_t idx) {
    return (idx < EYE_STYLE_COUNT) ? kEyeStyleNames[idx] : "?";
}

const char* Settings::expressionName(uint8_t idx) {
    return (idx < EXPRESSION_COUNT) ? kExpressionNames[idx] : "?";
}

const char* Settings::animationName(uint8_t idx) {
    return (idx < ANIMATION_COUNT) ? kAnimationNames[idx] : "?";
}

uint16_t Settings::ink() const     { return kThemes[_data.theme].ink; }
uint16_t Settings::muted() const   { return kThemes[_data.theme].muted; }
uint16_t Settings::panel() const   { return kThemes[_data.theme].panel; }
uint16_t Settings::warning() const { return kThemes[_data.theme].warning; }

uint16_t Settings::hsv565(uint16_t hue, uint8_t saturation, uint8_t value) {
    auto rgb = watchinteraction::hsvRgb(hue, saturation, value);
    return botux::rgb565(rgb.r, rgb.g, rgb.b);
}

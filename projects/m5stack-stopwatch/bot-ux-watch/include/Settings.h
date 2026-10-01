// NVS-backed watch preferences and the matching bot/UI palettes.
#pragma once

#include <Preferences.h>
#include <BotUx.h>
#include <stdint.h>

class Settings {
public:
    static const uint8_t THEME_NIGHT  = 0;
    static const uint8_t THEME_DUSK   = 1;
    static const uint8_t THEME_MONO   = 2;
    static const uint8_t THEME_COUNT  = 3;
    static const uint8_t APPEARANCE_COUNT = 3;
    static const uint8_t EYE_STYLE_COUNT = 4;
    static const uint8_t EXPRESSION_COUNT = 10;
    static const uint8_t ANIMATION_COUNT = 8;
    static const uint8_t TIMEOUT_COUNT = 6;
    static const uint8_t BRIGHTNESS_MIN = 1;
    static const uint8_t BRIGHTNESS_MAX = 5;
    static const uint8_t WAKE_TOUCH_KEYS = 0;
    static const uint8_t WAKE_KEYS_ONLY = 1;
    static const uint8_t WAKE_MODE_COUNT = 2;

    static constexpr uint8_t timeoutIndex(uint8_t value, uint8_t fallback) {
        return value < TIMEOUT_COUNT ? value : fallback;
    }

    struct Data {
        char    botName[17] = "Milo";
        uint8_t language = 0;
        uint8_t gaze = 0;
        bool showDescription = true;
        bool swapLayout = false;
        bool    hour24      = true;
        bool    showSeconds = false;
        bool    sound       = true;
        bool    startupSound = true;
        bool    buttonSound = true;
        bool    alertSound = true;
        bool    indicator   = false;
        bool    buttonFeedback = true;
        uint8_t theme       = THEME_NIGHT;
        uint8_t appearance  = 0;
        uint8_t expression  = 0;
        uint8_t animation   = 1;
        uint8_t brightness  = 3;     // 1..5
        bool    powerSaveEnabled = true;
        uint8_t dimBrightness = 1;   // 1..5
        uint8_t dimTimeout = 1;      // 5 s, 15 s, 1 m, 5 m, 10 m, 15 m
        uint8_t screenOffTimeout = 2;
        uint8_t wakeMode = WAKE_TOUCH_KEYS;
        bool    keepAwakeWhileCharging = true;
        bool    forcedSleepEnabled = false;
        uint8_t forcedSleepStartHour = 23;
        uint8_t forcedSleepEndHour = 7;
        bool    motion       = true;
        uint8_t eyeStyle     = 1;
        bool    customColor  = false;
        uint16_t colorHue    = 42;   // HSV: 0..359, 0..100, 0..100
        uint8_t colorSat     = 8;
        uint8_t colorValue   = 95;
        uint8_t motionAmount = 2;    // 1..5, intentionally gentle by default
        uint8_t animationSpeed = 2;  // 1..5
    };

    void begin();        // load from NVS (defaults on first boot)
    void save();         // persist data() to NVS

    Data& data() { return _data; }

    void rebuildStyle(); // recompute _style from _data
    const botux::BotUx::Style& style() const { return _style; }
    void apply(botux::BotUx& bot) const { bot.setStyle(_style); }

    static const char* themeName(uint8_t idx);
    static const char* appearanceName(uint8_t idx);
    static const char* eyeStyleName(uint8_t idx);
    static const char* expressionName(uint8_t idx);
    static const char* animationName(uint8_t idx);
    static uint16_t hsv565(uint16_t hue, uint8_t saturation, uint8_t value);

    // UI text never borrows the bot eye color: official-style eyes are dark.
    uint16_t ink() const;
    uint16_t muted() const;
    uint16_t panel() const;
    uint16_t warning() const;

private:
    Data _data;
    botux::BotUx::Style _style;
};

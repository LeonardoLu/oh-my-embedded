#pragma once

#include <M5GFX.h>
#include <stdint.h>

#include "BotUx.h"
#include "UxKeyboard.h"

class Settings {
public:
    struct ThemePalette {
        uint16_t background;
        uint16_t surface;
        uint16_t surfaceRaised;
        uint16_t outline;
        uint16_t text;
        uint16_t muted;
        uint16_t accent;
    };

    struct Data {
        uint8_t theme;
        uint8_t audio;
        uint8_t brightness;
        uint8_t reducedMotion;
        uint8_t ledBrightness;
        uint8_t animation;
        uint8_t motion;
        uint8_t botStyle;
        uint8_t expression;
        uint8_t bodyRed;
        uint8_t bodyGreen;
        uint8_t bodyBlue;
        uint8_t eyeRed;
        uint8_t eyeGreen;
        uint8_t eyeBlue;
        uint8_t accentRed;
        uint8_t accentGreen;
        uint8_t accentBlue;
        char botName[botux::BotUx::kNameMax + 1];
        uint8_t language;
        uint8_t ledMode;
        uint8_t notifications;
        uint8_t gaze;
        uint8_t volume;
    };

    static constexpr uint8_t kThemeCount = 3;
    static constexpr uint8_t kPageCount = 7;

    static constexpr uint8_t kNamePage = 7;

    void begin();
    void apply(botux::BotUx& bot) const;
    void applyBrightness() const;
    const Data& data() const { return _data; }
    botux::BotUx::Style botStyle() const;

    bool isOpen() const { return _open; }
    void open(uint8_t page = 0);
    void close();
    void touchBegin(int16_t x, int16_t y);
    bool touchMove(int16_t x, int16_t y);
    void touchEnd(int16_t x, int16_t y);
    void draw(M5Canvas& cv, M5Canvas& botSprite);
    // Call every host frame. When true and the static UI is clean, update only
    // previewRect() with drawAnimatedPreview(); interactions still use draw().
    bool animate(uint32_t nowMs);
    static constexpr ux::Rect previewRect() { return {6, 40, 112, 112}; }
    // Canvas or display destination; renders and pushes only the private Bot tile.
    bool drawAnimatedPreview(lgfx::LovyanGFX& target);

    static const char* themeName(uint8_t value);
    static ThemePalette themePalette(uint8_t value);
    static botux::BotUx::Style themeStyle(uint8_t value);

private:
    int8_t _hit(int16_t x, int16_t y) const;
    uint8_t& _colorChannel(uint8_t channel);
    bool _setColor(uint8_t channel, int16_t x);
    void _activate(int8_t target);
    void _save() const;
    void _drawPreview(M5Canvas& cv, uint16_t fg, uint16_t rowBg, uint16_t line);
    void _commitName();

    Data _data{0, 1, 3, 0, 2, 0, 2, 0, 0,
               252, 252, 250, 33, 36, 42, 60, 124, 232, "Milo", 0, 2, 1, 0, 3};
    bool _open = false;
    uint8_t _page = 0;
    uint8_t _colorPart = 0;
    uint8_t _colorOriginal = 0;
    bool _colorChanged = false;
    int8_t _pressed = -1;
    M5Canvas _previewSprite;
    botux::BotUx _preview;
    ux::NameEditor _nameEditor;
    bool _previewReady = false;
    bool _previewAttempted = false;
    uint8_t _previewMood = 0;
    uint8_t _previewExpression = 0;
    uint8_t _previewAnimation = 0;
    uint32_t _previewNow = 0;
};

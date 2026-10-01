#include "WatchFace.h"
#include "CalendarMath.h"
#include "RtcClock.h"
#include "WatchEdgeGeometry.h"
#include <UxRender.h>
#include <UxText.h>
#include <stdio.h>
#include <string.h>

namespace {
const char* const kWeekdays[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* const kWeekdaysZh[7] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};

void centered(M5Canvas& canvas, const char* text, int y, uint16_t color, const ux::Font& font) {
    ux::drawText(canvas, text, (466 - ux::textWidth(text, font)) / 2, y, color, font);
}

// Split only on UTF-8 boundaries; prefer a word boundary for Latin captions.
const char* wrapLine(const char* source, char* line, size_t capacity,
                     int maxWidth, const ux::Font& font, bool ellipsize) {
    size_t used = 0, wordEnd = 0;
    const char* cursor = source;
    while (*cursor) {
        const char* next = cursor; ux::nextCodepoint(next);
        size_t bytes = (size_t)(next - cursor);
        if (used + bytes >= capacity) break;
        memcpy(line + used, cursor, bytes); line[used + bytes] = 0;
        if (ux::textWidth(line, font) > maxWidth) { line[used] = 0; break; }
        if (*cursor == ' ' && used) wordEnd = used;
        used += bytes; cursor = next;
    }
    if (*cursor && !ellipsize && wordEnd > used / 2) { used = wordEnd; cursor = source + used; }
    line[used] = 0;
    if (*cursor && ellipsize) {
        while (used && (used + 3 >= capacity || ux::textWidth(line, font) + ux::textWidth("...", font) > maxWidth)) {
            --used;
            while (used && ((unsigned char)line[used] & 0xC0) == 0x80) --used;
            line[used] = 0;
        }
        if (used + 3 < capacity) memcpy(line + used, "...", 4);
    }
    while (*cursor == ' ') ++cursor;
    return cursor;
}
}

bool WatchFace::begin(M5Canvas* botSprite) {
    _bot.begin(botSprite);
    _hud.setColorDepth(16);
    _ready = _hud.createSprite(466, 90) != nullptr;
    _drawInvalid = true;
    return _ready;
}

void WatchFace::setLayout(bool swap, bool showDescription, uint8_t language) {
    language = language == 1 ? 1 : 0;
    if (_swap != swap || _showDescription != showDescription || _language != language) _drawInvalid = true;
    _swap = swap; _showDescription = showDescription; _language = language;
}

void WatchFace::setBattery(uint8_t pct) { _battery = pct > 100 ? 100 : pct; }

void WatchFace::update(uint32_t nowMs) {
    _now = nowMs;
    if (_lastRtcReadMs && nowMs - _lastRtcReadMs < 200) return;
    _lastRtcReadMs = nowMs;
    _bot.setBattery(_battery); _bot.setBatteryVisible(false); _bot.setSignal(-1);
    watchClock.refresh();
    if(_clockValid!=watchClock.hasTime()) _drawInvalid=true;
    _clockValid=watchClock.hasTime();
    if(!_clockValid) return;
    auto dt = watchClock.value();
    _hh = (uint8_t)dt.time.hours; _mm = (uint8_t)dt.time.minutes; _ss = (uint8_t)dt.time.seconds;
    _year = dt.date.year; _month = (uint8_t)dt.date.month; _day = (uint8_t)dt.date.date;
    _weekDay = watchcalendar::weekDay(_year, _month, _day);
}

void WatchFace::draw(lgfx::LovyanGFX* target, uint16_t bg, uint16_t ink, uint16_t muted,
                     uint16_t accent, uint16_t panel, uint16_t warning, float statusProgress) {
    if (!_ready || !target) return;
    const uint16_t colors[] = {bg, ink, muted, accent, panel, warning};
    bool invalid = _drawInvalid || target != _lastTarget || memcmp(colors, _colors, sizeof(colors));
    bool clockDirty = invalid || _drawHour != _hh || _drawMinute != _mm || _drawYear != _year
        || (_showSeconds && _drawSecond != _ss) || _drawDay != _day || _drawMonth != _month;
    char description[sizeof(_description)] = {};
    _bot.describe(description, sizeof(description), _language ? botux::BotUx::Language::Chinese : botux::BotUx::Language::English);
    bool descriptionDirty = invalid || strcmp(description, _description);
    memcpy(_description, description, sizeof(_description));
    bool panelVisible = statusProgress > 0.0f;
    for (int band = 0; band < 2; ++band) {
        bool top = band == 0, clock = top == _swap;
        bool dirty = clock ? clockDirty : (invalid || (_showDescription && descriptionDirty));
        if (top && (panelVisible || _panelWasVisible)) dirty = true;
        if (!dirty) continue;
        _hud.fillSprite(bg);
        if (!(top && panelVisible)) {
            if (clock) _drawClock(top, ink, muted);
            else if (_showDescription) _drawDescription(top, muted);
        }
        if (top && panelVisible) _drawBatteryPanel(bg, statusProgress);
        _hud.pushSprite(target, 0, top ? 0 : 376);
    }
    _drawHour = _hh; _drawMinute = _mm; _drawSecond = _ss;
    _drawDay = _day; _drawMonth = _month; _drawYear = _year;
    _panelWasVisible = panelVisible; _lastTarget = target;
    memcpy(_colors, colors, sizeof(colors)); _drawInvalid = false;
}

void WatchFace::_drawClock(bool top, uint16_t ink, uint16_t muted) {
    if(!_clockValid) {
        centered(_hud,"--:--",top?30:6,ink,ux::Latin28);
        centered(_hud,"----/--/--",top?62:57,muted,ux::Latin24);
        return;
    }
    uint8_t h = _hour24 ? _hh : (_hh % 12 ? _hh % 12 : 12);
    char time[24];
    const char* suffix = (_hour24 || _language) ? "" : (_hh >= 12 ? " PM" : " AM");
    if (_showSeconds) snprintf(time, sizeof(time), "%02u:%02u:%02u%s", h, _mm, _ss, suffix);
    else snprintf(time, sizeof(time), "%02u:%02u%s", h, _mm, suffix);
    const ux::Font* font = top ? &ux::Latin24 : &ux::Clock36;
    if (ux::textWidth(time, *font) > (top ? 240 : 300)) font = &ux::Latin24;
    if(_language && !_hour24) {
        const char* period=_hh>=12?"下午":"上午";
        int prefix=ux::textWidth(period,ux::Cjk18)+8;
        if(prefix+ux::textWidth(time,*font)>(top?240:300)) font=&ux::Latin24;
        int x=(466-prefix-ux::textWidth(time,*font))/2, y=top?30:6;
        ux::drawText(_hud,period,x,y+(ux::lineHeight(*font)-ux::lineHeight(ux::Cjk18))/2,ink,ux::Cjk18);
        ux::drawText(_hud,time,x+prefix,y,ink,*font);
    } else centered(_hud, time, top ? 30 : 6, ink, *font);
    char date[48];
    const char* weekday = _language ? kWeekdaysZh[_weekDay % 7] : kWeekdays[_weekDay % 7];
    snprintf(date, sizeof(date), "%04d/%02u/%02u %s", (int)_year, _month, _day, weekday);
    centered(_hud, date, top ? 62 : 57, muted, _language ? ux::Cjk18 : ux::Latin18);
}

void WatchFace::_drawDescription(bool top, uint16_t ink) {
    const ux::Font& font = _language ? ux::Cjk18 : ux::Latin18;
    char first[96] = {}, second[96] = {};
    int maxWidth = 250;
    const char* rest = wrapLine(_description, first, sizeof(first), maxWidth, font, false);
    wrapLine(rest, second, sizeof(second), top ? 250 : 225, font, true);
    centered(_hud, first, top ? 30 : 8, ink, font);
    if (second[0]) centered(_hud, second, top ? 56 : 34, ink, font);
}

void WatchFace::_drawBatteryPanel(uint16_t bg, float progress) {
    int16_t offset=watchedge::batteryToothOffset(progress);
    auto to565=[](uint32_t rgb) { return botux::rgb565((uint8_t)(rgb>>16),(uint8_t)(rgb>>8),(uint8_t)rgb); };
    uint16_t level=to565(watchedge::batteryBandRgb(watchedge::batteryBand(_battery)));
    const uint16_t dark=to565(watchedge::batteryInkRgb());
    for(int16_t y=0;y<watchedge::hudHeight();++y) {
        auto span=watchedge::batteryToothRowSpan(y,offset);
        if(span.width<=0) continue;
        _hud.fillRect(span.x,span.y,span.width,1,level);
    }
    uint8_t glow=watchedge::batteryChargeShade(_now);
    uint16_t charge=botux::rgb565(glow,(uint8_t)(glow+6),(uint8_t)(glow+10));
    char pct[8]; snprintf(pct, sizeof(pct), "%u%%", (unsigned)_battery);
    ux::drawText(_hud,pct,watchedge::batteryPercentX(),offset+watchedge::batteryPercentY(),dark,ux::Latin24);
    float bx=watchedge::batteryGaugeX(),by=offset+watchedge::batteryGaugeY();
    ux::strokeRoundRect(_hud, bx, by, 44, 21, 5, dark, 1.5f);
    ux::roundRect(_hud, bx + 45, by + 6, 4, 9, 2, dark);
    float fill=watchedge::batteryGaugeFill(_battery);
    if(fill>0) ux::roundRect(_hud,bx+3,by+3,fill,15,3,dark);
    if (_charging) {
        // Charging breathes independently without replacing the measured level color.
        ux::line(_hud, bx + 63, by + 2, bx + 56, by + 11, 2.5f, charge);
        ux::line(_hud, bx + 56, by + 11, bx + 62, by + 10, 2.5f, charge);
        ux::line(_hud, bx + 62, by + 10, bx + 56, by + 19, 2.5f, charge);
    }
    // Glyph and AA primitives draw rectangular bounds. Restore the background
    // outside the current animated tooth, then add one dark outline pixel.
    for(int16_t y=0;y<watchedge::hudHeight();++y) {
        auto span=watchedge::batteryToothRowSpan(y,offset);
        if(span.width<=0) { _hud.fillRect(0,y,watchedge::displaySize(),1,bg); continue; }
        if(span.x>0) _hud.fillRect(0,y,span.x,1,bg);
        int16_t end=(int16_t)(span.x+span.width);
        if(end<watchedge::displaySize())
            _hud.fillRect(end,y,watchedge::displaySize()-end,1,bg);
        _hud.drawPixel(span.x,y,dark);
        if(span.width>1) _hud.drawPixel(end-1,y,dark);
        if(y-offset==watchedge::batteryToothBottomY())
            _hud.drawFastHLine(span.x,y,span.width,dark);
    }
}

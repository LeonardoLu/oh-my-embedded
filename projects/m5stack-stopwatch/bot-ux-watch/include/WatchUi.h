#pragma once
#include <M5Unified.h>
#include <UxText.h>
#include <UxRender.h>
#include "WatchStrings.h"

inline bool watchLargeText(M5Canvas& cv) {
    return cv.getFont() == &fonts::FreeSansBold12pt7b
        || cv.getFont() == &fonts::FreeSansBold18pt7b;
}

inline const ux::Font& watchFont(M5Canvas& cv, const char* text) {
    bool cjk=false;
    for (const unsigned char* p = (const unsigned char*)text; *p; ++p)
        if (*p >= 128) { cjk=true; break; }
    if(watchLargeText(cv)) return cjk ? ux::Cjk28 : ux::Latin28;
    return cjk ? ux::Cjk24 : ux::Latin24;
}

inline const ux::Font& watchKeyboardFont(bool chinese) {
    return chinese ? ux::Cjk22 : ux::Latin24;
}

inline void watchDrawAligned(M5Canvas& cv, const char* text, int x, int y,
                             const ux::Font& font) {
    unsigned datum = (unsigned)cv.getTextDatum();
    int width = ux::textWidth(text, font);
    if ((datum & 3) == 1) x -= width / 2;
    else if ((datum & 3) == 2) x -= width;
    if ((datum & 12) == 4) y -= ux::lineHeight(font) / 2;
    else if ((datum & 12) == 8) y -= ux::lineHeight(font);
    uint32_t rgb = cv.getTextStyle().fore_rgb888;
    uint16_t ink = ((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x1F);
    ux::drawText(cv, text, x, y, ink, font);
}

// Migration adapter: retain deliberate alignment while using native AA glyphs.
inline void watchText(M5Canvas& cv, const char* text, int x, int y, bool localize=true) {
    if(localize) text=watchstrings::translate(text);
    watchDrawAligned(cv,text,x,y,watchFont(cv,text));
}

// Names are the only variable-width settings copy. Preserve UTF-8 boundaries and
// make truncation explicit instead of reducing the font below the 24 px body face.
inline void watchEllipsizedText(M5Canvas& cv, const char* text, int x, int y,
                                int maxWidth) {
    const ux::Font& font=watchFont(cv,text);
    if(ux::textWidth(text,font)<=maxWidth) {
        watchDrawAligned(cv,text,x,y,font);
        return;
    }
    char shortened[40]={};
    const char* cursor=text;
    unsigned used=0;
    int width=0, dots=ux::textWidth("...",font);
    while(*cursor&&used+4<sizeof(shortened)) {
        const char* start=cursor;
        uint32_t cp=ux::nextCodepoint(cursor);
        const ux::Glyph* glyph=ux::glyph(font,cp);
        int advance=glyph?glyph->advance:0;
        if(width+advance+dots>maxWidth) break;
        unsigned bytes=(unsigned)(cursor-start);
        if(used+bytes+3>=sizeof(shortened)) break;
        for(unsigned i=0;i<bytes;++i) shortened[used++]=start[i];
        width+=advance;
    }
    shortened[used++]='.'; shortened[used++]='.'; shortened[used++]='.';
    shortened[used]=0;
    watchDrawAligned(cv,shortened,x,y,font);
}

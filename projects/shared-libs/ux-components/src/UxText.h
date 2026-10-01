#pragma once
#include "UxRender.h"
namespace ux {
struct Glyph { uint32_t code, offset; int8_t x,y; uint8_t w,h,advance; };
struct Font { const Glyph* glyphs; const uint8_t* coverage; uint16_t count; uint8_t lineHeight; };
// Each face lives in a separate translation unit; reference only what you use.
extern const Font Latin14, Latin18, Latin24, Latin28, Clock36;
extern const Font Cjk18, Cjk22, Cjk24, Cjk28;
inline uint32_t nextCodepoint(const char*& s) {
    uint8_t b=(uint8_t)*s++; if(b<128) return b;
    int n=b>=0xF0?3:b>=0xE0?2:b>=0xC2?1:0;
    uint32_t cp=b&((1u<<(6-n))-1);
    if(!n) return '?';
    for(int i=0;i<n;++i) { uint8_t t=(uint8_t)*s; if((t&0xC0)!=0x80) return '?'; ++s; cp=(cp<<6)|(t&63); }
    return cp;
}
inline const Glyph* glyph(const Font& f,uint32_t cp) {
    int lo=0,hi=f.count;
    while(lo<hi) { int m=(lo+hi)/2; if(f.glyphs[m].code<cp) lo=m+1; else hi=m; }
    if(lo<f.count && f.glyphs[lo].code==cp) return f.glyphs+lo;
    return cp=='?'?nullptr:glyph(f,'?');
}
inline int textWidth(const char* s,const Font& f=Latin18) {
    int width=0,line=0; if(!s) return 0;
    while(*s) { uint32_t cp=nextCodepoint(s); if(cp=='\n') { if(line>width) width=line; line=0; continue; } const Glyph* g=glyph(f,cp); if(g) line+=g->advance; }
    return line>width?line:width;
}
inline int lineHeight(const Font& f=Latin18) { return f.lineHeight; }
// Coordinates are the top-left of the line box, not a baseline. No heap allocation.
template<class Canvas> void drawText(Canvas& c,const char* s,int x,int y,uint16_t color,const Font& f=Latin18) {
    if(!s) return; int origin=x;
    while(*s) {
        uint32_t cp=nextCodepoint(s); if(cp=='\n') { x=origin;y+=f.lineHeight;continue; }
        const Glyph* g=glyph(f,cp); if(!g) continue;
        for(unsigned py=0;py<g->h;++py) for(unsigned px=0;px<g->w;++px) {
            unsigned i=py*g->w+px; uint8_t packed=f.coverage[g->offset+i/2];
            uint8_t a=((i&1)?packed&15:packed>>4)*17;
            pixel(c,x+g->x+px,y+g->y+py,color,a);
        }
        x+=g->advance;
    }
}
}

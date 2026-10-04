#include "WatchStrings.h"
#include <UxText.h>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <vector>

namespace {
struct Canvas {
    std::vector<uint16_t> pixels;
    int w,h;
    Canvas(int width,int height):pixels(width*height,0x0841),w(width),h(height) {}
    int width() const { return w; }
    int height() const { return h; }
    uint16_t readPixel(int x,int y) const { return pixels[y*w+x]; }
    void drawPixel(int x,int y,uint16_t color) {
        assert(x>=0&&x<w&&y>=0&&y<h); pixels[y*w+x]=color;
    }
    void fillRect(int x,int y,int width,int height,uint16_t color) {
        for(int py=y;py<y+height;++py) for(int px=x;px<x+width;++px) drawPixel(px,py,color);
    }
};

void centered(Canvas& c,const char* text,int y,uint16_t color,const ux::Font& font) {
    ux::drawText(c,text,(c.width()-ux::textWidth(text,font))/2,y,color,font);
}
void centerAt(Canvas& c,const char* text,int x,int y,uint16_t color,const ux::Font& font) {
    ux::drawText(c,text,x-ux::textWidth(text,font)/2,y,color,font);
}
void right(Canvas& c,const char* text,int x,int y,uint16_t color,const ux::Font& font) {
    ux::drawText(c,text,x-ux::textWidth(text,font),y,color,font);
}
void assertPair(const char* label,const char* value,const ux::Font& font) {
    // Rows start at x=84 and end at x=382. Preserve a readable 16 px gap.
    assert(ux::textWidth(label,font)+ux::textWidth(value,font)<=282);
}
}

int main(int argc,char** argv) {
    using namespace ux;
    assert(Latin24.lineHeight==29&&Cjk24.lineHeight==29);
    assert(Latin28.lineHeight==33&&Cjk28.lineHeight==33);
    assert(Cjk22.lineHeight==27);
    assert(textWidth("SETTINGS",Latin24)>textWidth("SETTINGS",Latin18));
    assert(textWidth("设置",Cjk24)>textWidth("设置",Cjk18));
    assert(textWidth("SETTINGS",Latin28)>textWidth("SETTINGS",Latin24));
    assert(textWidth("设置",Cjk28)>textWidth("设置",Cjk24));

    // Every localized source glyph is present in each Watch face that can render it.
    for(const auto& entry:watchstrings::entries) {
        const char* cursor=entry.zh;
        while(*cursor) {
            uint32_t cp=nextCodepoint(cursor);
            if(!((cp>=0x4e00&&cp<=0x9fff)||cp==0xff0c||cp==0x3002)) continue;
            assert(glyph(Cjk18,cp)->code==cp);
            assert(glyph(Cjk22,cp)->code==cp);
            assert(glyph(Cjk24,cp)->code==cp);
            assert(glyph(Cjk28,cp)->code==cp);
        }
    }
    const char* localizedSources[]={"projects/shared-libs/bot-ux/src/BotUx.cpp",
        "projects/m5stack-stopwatch/bot-ux-watch/include/WatchStrings.h",
        "projects/m5stack-stopwatch/bot-ux-watch/src/main.cpp",
        "projects/m5stack-stopwatch/bot-ux-watch/src/Settings.cpp"};
    for(const char* path:localizedSources) {
        std::ifstream file(path); assert(file.good());
        std::string source((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        const char* cursor=source.c_str();
        while(*cursor) {
            uint32_t cp=nextCodepoint(cursor);
            if(!((cp>=0x4e00&&cp<=0x9fff)||cp==0xff0c||cp==0x3002)) continue;
            assert(glyph(Cjk18,cp)->code==cp);
            assert(glyph(Cjk22,cp)->code==cp);
            assert(glyph(Cjk24,cp)->code==cp);
            assert(glyph(Cjk28,cp)->code==cp);
        }
    }

    const char* titles[]={"SETTINGS","BOT PERSONALITY","SET TIME","SET DATE",
        "TIME FORMAT","APPEARANCE","EXPRESSION","MOTION","BOT COLOR",
        "TIME SETTINGS","DISPLAY","SOUND","POWER SAVING","BOT NAME","LANGUAGE",
        "WATCH LAYOUT","COMBINATIONS","GAZE"};
    for(const char* title:titles) {
        assert(textWidth(title,Latin28)<=284);
        assert(textWidth(watchstrings::translate(title,true),Cjk28)<=284);
    }

    // Fixed controls retain at least four pixels of horizontal inset per side.
    assert(textWidth("DONE",Latin24)<=150);
    assert(textWidth("完成",Cjk24)<=150);
    assert(textWidth("12 HOUR",Latin24)<=138);
    assert(textWidth("12小时制",Cjk24)<=138);
    assert(textWidth("SECONDS  OFF",Latin24)<=258);
    assert(textWidth("隐藏秒数",Cjk24)<=258);
    assert(textWidth("USE THEME",Latin24)<=132);
    assert(textWidth("使用主题",Cjk24)<=132);

    // Bounded settings values retain separation from their real paired labels.
    assertPair("EXPRESSION","Skeptical",Latin24);
    assertPair("APPEARANCE","Pebble",Latin24);
    assertPair("COMBINATIONS","1120",Latin24);
    assertPair("DIRECTION","Down-right",Latin24);
    assertPair("BRIGHTNESS","5 / 5",Latin24);
    assertPair("BUTTON FX","OFF",Latin24);
    assertPair("STARTUP SOUND","OFF",Latin24);
    assertPair("BUTTON SOUND","OFF",Latin24);
    assertPair("ALERT SOUND","OFF",Latin24);
    assertPair("POWER SAVE","OFF",Latin24);
    assertPair("DIM LEVEL","5 / 5",Latin24);
    assertPair("CHARGE AWAKE","OFF",Latin24);
    assertPair("FORCED OFF","OFF",Latin24);
    assertPair("FROM","23:00",Latin24);
    assertPair("UNTIL","07:00",Latin24);
    assertPair("DIM TIMEOUT","15 MIN",Latin24);
    assertPair("SCREEN OFF","15 MIN",Latin24);
    assertPair("WAKE","TOUCH + KEYS",Latin24);
    assertPair("WAKE","KEYS ONLY",Latin24);
    assertPair("按下效果","关闭",Cjk24);
    assertPair("调暗延时","15 MIN",Cjk24);
    assertPair("息屏延时","15 MIN",Cjk24);
    assertPair("唤醒方式","触摸 + 按键",Cjk24);
    assertPair("唤醒方式","仅按键",Cjk24);
    assertPair("充电时常亮","关闭",Cjk24);
    assertPair("强制息屏","关闭",Cjk24);
    assertPair("开始时间","23:00",Cjk24);
    assertPair("结束时间","07:00",Cjk24);
    assertPair("伙伴描述","显示",Cjk24);
    assertPair("方向","右下",Cjk24);
    assertPair("表情","怀疑",Cjk24);
    const char* expressions[]={"Auto","Neutral","Curious","Focused","Joy",
        "Skeptical","Bashful","Wink","Dizzy","Alarmed"};
    const char* animations[]={"Auto","Calm","Curious","Orbit","Bounce",
        "Glitch","Wave","Sparkle"};
    const char* appearances[]={"Orb","Bean","Pebble"};
    const char* directions[]={"Auto","Front","Left","Right","Up","Down",
        "Up-left","Up-right","Down-left","Down-right"};
    for(const char* value:expressions) assertPair("EXPRESSION",value,Latin24);
    for(const char* value:animations) assertPair("ACTION",value,Latin24);
    for(const char* value:appearances) assertPair("APPEARANCE",value,Latin24);
    for(const char* value:directions) assertPair("GAZE",value,Latin24);

    // Center cards keep their 28 px names inside the 136 px surface.
    for(const char* label:{"TIME","BOT","DISPLAY","SOUND","POWER"})
        assert(textWidth(label,Latin28)<=124);
    for(const char* label:{"时间","伙伴","显示","声音","省电"})
        assert(textWidth(label,Cjk28)<=124);

    assert(textWidth("MONTH",Latin24)<=86);
    assert(textWidth("2026",Latin28)<=86);
    assert(textWidth("Show seconds on the clock",Latin24)<=424);
    assert(textWidth("选择是否在时钟上显示秒数",Cjk24)<=424);
    assert(textWidth("Tap arrows; tap the bot to react",Latin24)<=424);
    assert(textWidth("轻点箭头切换，轻点伙伴互动",Cjk24)<=424);

    // Keyboard interiors are 47 px at their narrowest in the 310 px layout.
    for(const char* label:{"Del","_","Aa","OK"}) assert(textWidth(label,Latin24)<=47);
    for(const char* label:{"删除","空格","Aa","确定"}) assert(textWidth(label,Cjk22)<=47);
    assert(Latin24.lineHeight<=37&&Cjk22.lineHeight<=37);

    Canvas canvas(466,466);
    centered(canvas,"BOT PERSONALITY",18,0xffff,Latin28);
    centered(canvas,"伙伴个性",58,0xffff,Cjk28);
    drawText(canvas,"EXPRESSION",84,112,0xffff,Latin24);
    right(canvas,"Skeptical",382,112,0x9e7f,Latin24);
    drawText(canvas,"表情",84,152,0xffff,Cjk24);
    right(canvas,"怀疑",382,152,0x9e7f,Cjk24);
    centered(canvas,"DONE / 完成",202,0xffff,Cjk24);
    const char* enKeys[]={"Del","_","Aa","OK"};
    const char* zhKeys[]={"删除","空格","Aa","确定"};
    for(int i=0;i<4;++i) {
        int x=77+i*104;
        centerAt(canvas,enKeys[i],x,252,0xffff,Latin24);
        centerAt(canvas,zhKeys[i],x,302,0xffff,Cjk22);
    }
    centered(canvas,"Show seconds on the clock",356,0x9e7f,Latin24);
    centered(canvas,"选择是否在时钟上显示秒数",396,0x9e7f,Cjk24);
    unsigned coverage=0;
    for(uint16_t pixel:canvas.pixels) if(pixel!=0x0841&&pixel!=0xffff&&pixel!=0x9e7f) ++coverage;
    assert(coverage>300);
    if(argc>1) {
        std::ofstream out(argv[1],std::ios::binary); out<<"P6\n466 466\n255\n";
        for(uint16_t p:canvas.pixels) {
            char rgb[3]={(char)((p>>11)*255/31),(char)(((p>>5)&63)*255/63),(char)((p&31)*255/31)};
            out.write(rgb,3);
        }
    }
    std::cout<<"Watch native 22/24/28 px font coverage and layout bounds passed\n";
}

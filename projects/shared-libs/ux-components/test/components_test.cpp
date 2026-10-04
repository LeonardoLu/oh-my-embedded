#include "UxKeyboard.h"
#include "UxTokens.h"
#include <assert.h>
#include <vector>
#include <fstream>
#include <iostream>
struct Canvas {
    std::vector<uint16_t> p; int w,h; unsigned spans=0,pixels=0;
    Canvas(int width,int height):p(width*height,0x0841),w(width),h(height){}
    void fillRect(int x,int y,int width,int height,uint16_t color) { ++spans; for(int py=y;py<y+height;++py) for(int px=x;px<x+width;++px) { assert(px>=0&&px<w&&py>=0&&py<h);p[py*w+px]=color; } }
    int width() const { return w; } int height() const { return h; }
    uint16_t readPixel(int x,int y) const { assert(x>=0&&x<w&&y>=0&&y<h);return p[y*w+x]; }
    void drawPixel(int x,int y,uint16_t c) { ++pixels; assert(x>=0&&x<w&&y>=0&&y<h);p[y*w+x]=c; }
};
int main(int argc,char** argv) {
    using namespace ux;
    assert(blend565(0,0xffff,0)==0); assert(blend565(0,0xffff,255)==0xffff);
    assert(blend565(0x1234,0x1234,127)==0x1234);
    Canvas c(320,240);
    roundRect(c,7.25f,5.5f,90,36,10,0xffff);
    int partial=0; for(auto p:c.p) if(p!=0x0841&&p!=0xffff) ++partial; assert(partial>20);
    roundRect(c,-20,-20,24,24,12,0x07e0); line(c,-10,45,90,50,3,0x07ff);
    // Compare optimized span filling with the unoptimized signed-distance oracle.
    for(int n=0;n<80;++n) {
        Canvas actual(80,60), expected(80,60);
        float x=-3+(n%7)*0.23f,y=2+(n%5)*0.19f,w=50+(n%9)*0.3f,h=35+(n%3)*0.2f,r=n%18;
        roundRect(actual,x,y,w,h,r,0xfedf);
        for(int py=0;py<60;++py) for(int px=0;px<80;++px) {
            if(px<floorf(x)||px>=ceilf(x+w)||py<floorf(y)||py>=ceilf(y+h)) continue;
            float qx=fabsf(px+0.5f-x-w/2)-w/2+r,qy=fabsf(py+0.5f-y-h/2)-h/2+r;
            float a=fmaxf(qx,0),b=fmaxf(qy,0);
            float d=sqrtf(a*a+b*b)+fminf(fmaxf(qx,qy),0)-r;
            pixel(expected,px,py,0xfedf,(uint8_t)(clamp(0.5f-d,0,1)*255+0.5f));
        }
        assert(actual.p==expected.p);
    }
    Canvas large(400,80); roundRect(large,10,10,350,58,14,0xffff); assert(large.spans==58); assert(large.pixels<200);
    Canvas outline(60,40); strokeRoundRect(outline,1,1,58,38,8,0xffff);
    assert(outline.readPixel(30,20)==0x0841);assert(outline.readPixel(30,1)==0xffff);
    ScrollModel scroll; scroll.setBounds(1000,200); scroll.begin(100,0); scroll.move(96,10); assert(scroll.offset()==0); scroll.move(85,20); assert(scroll.offset()==15); scroll.move(82,30); assert(scroll.offset()==18); assert(!scroll.end(30)); assert(scroll.update(16)); assert(scroll.offset()>18);
    scroll.setOffset(800); scroll.begin(10,100); scroll.move(-100,110); assert(scroll.offset()==800); scroll.end(110); scroll.update(50); assert(scroll.offset()==800);
    scroll.setBounds(50,200); assert(scroll.offset()==0); scroll.begin(10,120); assert(scroll.end(130));
    scroll.begin(10,200); scroll.move(40,210); scroll.move(10,220); assert(!scroll.end(220));
    // Touch controllers commonly repeat their last coordinates on release.
    ScrollModel flick; flick.setBounds(1000,200);
    flick.begin(100,1000);flick.move(80,1020);flick.move(80,1030);
    assert(!flick.end(1030));assert(flick.update(16));assert(flick.offset()>20);
    // A pause while holding the finger cancels momentum, including duplicate samples.
    ScrollModel held;held.setBounds(1000,200);
    held.begin(100,2000);held.move(80,2020);held.move(80,2050);held.move(80,2111);
    assert(!held.end(2120));assert(!held.update(16));assert(held.offset()==20);
    // No intermediate held samples: end() itself enforces the stale-velocity cutoff.
    ScrollModel stopped;stopped.setBounds(1000,200);
    stopped.begin(100,3000);stopped.move(80,3020);assert(!stopped.end(3111));
    assert(!stopped.update(16));assert(stopped.offset()==20);
    // Exactly 90 ms is still a recent movement; the boundary is inclusive.
    stopped.begin(100,4000);stopped.move(80,4020);stopped.move(80,4110);
    assert(!stopped.end(4110));assert(stopped.update(16));assert(stopped.offset()>40);
    NameEditor e; e.begin("Milo"); e.press(NameEditor::Backspace); assert(!strcmp(e.text(),"Mil")); e.press(14); assert(!strcmp(e.text(),"MilO")); e.begin(""); assert(e.press(NameEditor::Done)); assert(!strcmp(e.text(),"Milo")); e.begin("abcdefghijklmnopqrs"); assert(strlen(e.text())==16); e.begin("\xE4\xB8\xAD!"); assert(!*e.text());
    Rect area{10,100,300,130}; for(int k=0;k<30;++k) { Rect r=nameKeyRect(area,k); assert(nameKeyAt(area,r.x+r.w/2,r.y+r.h/2)==k); } assert(nameKeyAt(area,0,100)==-1);assert(nameKeyAt(area,10,100)==-1);
    assert(textWidth("Milo",Latin18)>20);assert(textWidth("Milo\nM",Latin18)==textWidth("Milo",Latin18));
    const char* corpus="空闲聆听思考说话开心难过困倦惊讶工作等待受阻完成自动自然好奇专注喜悦怀疑害羞眨眼眩晕警觉平静环绕弹跳闪动波浪闪耀";
    while(*corpus) { uint32_t cp=nextCodepoint(corpus);assert(glyph(Cjk18,cp)->code==cp); }
    // Track the actual semantic source, not only a copied list of expected labels.
    const char* sources[]={"projects/shared-libs/bot-ux/src/BotUx.cpp",
        "projects/m5stack-stopwatch/bot-ux-watch/include/WatchStrings.h",
        "projects/m5stack-stopwatch/bot-ux-watch/src/main.cpp",
        "projects/m5stack-stopwatch/bot-ux-watch/src/WatchFace.cpp",
        "projects/m5stack-core2/bot-ux-codex-core2/src/Settings.cpp"};
    for(const char* source:sources) {
        std::ifstream file(source);assert(file.good());
        std::string semanticSource((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        const char* cursor=semanticSource.c_str();
        while(*cursor) {
            uint32_t cp=nextCodepoint(cursor);
            if((cp>=0x4e00 && cp<=0x9fff)||cp==0xff0c||cp==0x3002) assert(glyph(Cjk18,cp)->code==cp);
        }
    }
    const NameKeyboardLabels zhKeys={"删除","空格","Aa","确定"};
    for(const char* label:{zhKeys.backspace,zhKeys.space,zhKeys.letterCase,zhKeys.done})
        assert(textWidth(label,Cjk18)<=46);
    drawNameKeyboard(c,e,area,0x2126,0xffff,Cjk18,-1,&zhKeys);
    Canvas textCanvas(320,30); drawText(textCanvas,"Milo 正在休息",2,0,0xffff,Cjk18);
    partial=0; for(auto p:textCanvas.p) if(p!=0x0841&&p!=0xffff) ++partial; assert(partial>50);
    drawText(c,"Milo 正在休息",10,53,0xffff,Cjk18);
    drawText(c,"Native 4-bit coverage",10,78,0x9e7f,Latin14);
    drawNameKeyboard(c,e,area,0x2126,0xffff);
    assert(motion::easeOut(0)==0&&motion::easeOut(1)==1);
    if(argc>1) { std::ofstream out(argv[1],std::ios::binary);out<<"P6\n320 240\n255\n";for(auto p:c.p) { char rgb[3]={(char)((p>>11)*255/31),(char)(((p>>5)&63)*255/63),(char)((p&31)*255/31)};out.write(rgb,3); } }
    std::cout<<"coverage, Chinese glyphs, clipping, scrolling, keyboard and motion passed\n";
}

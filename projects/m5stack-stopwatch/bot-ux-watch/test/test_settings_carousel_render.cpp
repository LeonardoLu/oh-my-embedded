#include <stdint.h>
namespace lgfx { inline namespace v1 {
struct rgb565_t {
    uint16_t raw;
    rgb565_t& operator=(uint16_t value) { raw=value; return *this; }
};
} }
#include "WatchSettingsCarousel.h"
#include "WatchSettingsRows.h"
#include "WatchStrings.h"
#include <UxText.h>
#include <assert.h>
#include <fstream>
#include <string>
#include <vector>

namespace {
struct Canvas {
    std::vector<uint16_t> pixels;
    int w=466,h=466;
    Canvas():pixels(466*466,0) {}
    int width() const { return w; }
    int height() const { return h; }
    uint16_t readPixel(int x,int y) const { return pixels[y*w+x]; }
    void drawPixel(int x,int y,uint16_t color) {
        if(x>=0&&x<w&&y>=0&&y<h) pixels[y*w+x]=color;
    }
    void fillRect(int x,int y,int width,int height,uint16_t color) {
        for(int py=y;py<y+height;++py) for(int px=x;px<x+width;++px)
            drawPixel(px,py,color);
    }
    void pushImage(int x,int y,int width,int height,const lgfx::rgb565_t* source) {
        for(int py=0;py<height;++py) for(int px=0;px<width;++px)
            drawPixel(x+px,y+py,source[py*width+px].raw);
    }
    void fillCircle(int cx,int cy,int radius,uint16_t color) {
        for(int y=-radius;y<=radius;++y) for(int x=-radius;x<=radius;++x)
            if(x*x+y*y<=radius*radius) drawPixel(cx+x,cy+y,color);
    }
    void drawCircle(int cx,int cy,int radius,uint16_t color) {
        int x=radius,y=0,error=1-radius;
        while(x>=y) {
            const int points[][2]={{x,y},{y,x},{-y,x},{-x,y},{-x,-y},{-y,-x},{y,-x},{x,-y}};
            for(const auto& point:points) drawPixel(cx+point[0],cy+point[1],color);
            ++y;
            if(error<0) error+=2*y+1;
            else { --x; error+=2*(y-x)+1; }
        }
    }
    void drawFastHLine(int x,int y,int width,uint16_t color) { fillRect(x,y,width,1,color); }
    void drawFastVLine(int x,int y,int height,uint16_t color) { fillRect(x,y,1,height,color); }
    void drawLine(int x0,int y0,int x1,int y1,uint16_t color) {
        int dx=x1>x0?x1-x0:x0-x1,sx=x0<x1?1:-1;
        int dy=y1>y0?y0-y1:y1-y0,sy=y0<y1?1:-1,error=dx+dy;
        for(;;) {
            drawPixel(x0,y0,color);
            if(x0==x1&&y0==y1) break;
            int twice=2*error;
            if(twice>=dy) { error+=dy; x0+=sx; }
            if(twice<=dx) { error+=dx; y0+=sy; }
        }
    }
    void fillTriangle(int x0,int y0,int x1,int y1,int x2,int y2,uint16_t color) {
        int minX=x0<x1?(x0<x2?x0:x2):(x1<x2?x1:x2);
        int maxX=x0>x1?(x0>x2?x0:x2):(x1>x2?x1:x2);
        int minY=y0<y1?(y0<y2?y0:y2):(y1<y2?y1:y2);
        int maxY=y0>y1?(y0>y2?y0:y2):(y1>y2?y1:y2);
        const int area=(x1-x0)*(y2-y0)-(y1-y0)*(x2-x0);
        for(int y=minY;y<=maxY;++y) for(int x=minX;x<=maxX;++x) {
            int a=(x1-x0)*(y-y0)-(y1-y0)*(x-x0);
            int b=(x2-x1)*(y-y1)-(y2-y1)*(x-x1);
            int c=(x0-x2)*(y-y2)-(y0-y2)*(x-x2);
            if(area>=0?(a>=0&&b>=0&&c>=0):(a<=0&&b<=0&&c<=0)) drawPixel(x,y,color);
        }
    }
};

struct NativeView {
    Canvas& canvas;
    bool chinese;
    bool launcher;
    uint16_t background() const { return launcher?0:0x0841; }
    uint16_t panel() const { return 0x18E3; }
    uint16_t ink() const { return 0xFFFF; }
    uint16_t muted() const { return 0x9E7F; }
    uint16_t accent() const { return 0x2DDF; }
    bool pressed(ux::Rect) const { return false; }
    const ux::Font& font(const char* text,bool large=false) const {
        bool cjk=false;
        for(const unsigned char* p=(const unsigned char*)text;*p;++p)
            if(*p>=128) { cjk=true; break; }
        return large?(cjk?ux::Cjk28:ux::Latin28):(cjk?ux::Cjk24:ux::Latin24);
    }
    const char* localized(const char* text) const {
        return watchstrings::translate(text,chinese);
    }
    void aligned(const char* source,int x,int y,uint16_t color,int align,bool large=false) {
        const char* text=localized(source); const auto& face=font(text,large);
        int width=ux::textWidth(text,face);
        if(align==0) x-=width/2; else if(align>0) x-=width;
        ux::drawText(canvas,text,x,y-ux::lineHeight(face)/2,color,face);
    }
    void large(const char* text,int16_t x,int16_t y,uint16_t color) {
        aligned(text,x,y,color,0,true);
    }
    void body(const char* text,int16_t x,int16_t y,uint16_t color) {
        aligned(text,x,y,color,0);
    }
    void ellipsized(const char* text,int16_t x,int16_t y,int16_t,uint16_t color) {
        aligned(text,x,y,color,0);
    }
    void left(const char* text,int16_t x,int16_t y,uint16_t color) {
        aligned(text,x,y,color,-1);
    }
    void right(const char* text,int16_t x,int16_t y,uint16_t color) {
        aligned(text,x,y,color,1);
    }
    void center(const char* text,int16_t x,int16_t y,uint16_t color) {
        aligned(text,x,y,color,0);
    }
};

void clearRound(Canvas& canvas,uint16_t background) {
    canvas.fillRect(0,0,466,466,0);
    canvas.fillCircle(233,233,233,background);
}
void drawFooter(Canvas& canvas,NativeView& view) {
    for(int y=watchcontrols::doneBounds().y;y<466;++y) {
        auto span=watchcontrols::doneRowSpan(y);
        canvas.drawFastHLine(span.x,span.y,span.w,view.accent());
    }
    view.body("DONE",watchcontrols::doneLabelX(),watchcontrols::doneLabelY(),view.background());
}
void drawSettingsFooter(Canvas&,NativeView& view) {
    view.body("DONE",watchcontrols::doneLabelX(),watchcontrols::doneLabelY(),view.muted());
}
void writePpm(const std::string& path,const Canvas& canvas) {
    std::ofstream out(path,std::ios::binary); assert(out.good());
    out<<"P6\n466 466\n255\n";
    for(uint16_t p:canvas.pixels) {
        char rgb[3]={(char)((p>>11)*255/31),(char)(((p>>5)&63)*255/63),(char)((p&31)*255/31)};
        out.write(rgb,3);
    }
}
void renderCarousel(const std::string& path,bool chinese,uint8_t selected) {
    Canvas canvas; NativeView view{canvas,chinese,true}; clearRound(canvas,view.background());
    view.large("SETTINGS",233,48,view.ink());
    const char* labels[]={"TIME","BOT","DISPLAY","SOUND","POWER"};
    watchsettingscarousel::draw(canvas,view,selected*watchcontrols::settingsCarousel().step,
                                selected,labels);
    drawSettingsFooter(canvas,view); writePpm(path,canvas);
}
void renderPowerBottom(const std::string& path) {
    Canvas canvas; NativeView view{canvas,true,false}; clearRound(canvas,view.background());
    view.large("POWER SAVING",233,48,view.ink());
    const char* labels[]={"POWER SAVE","DIM LEVEL","DIM AFTER","AUTO OFF","WAKE",
                          "CHARGE AWAKE","FORCED OFF","FROM","UNTIL"};
    const char* values[]={"ON","1 / 5","15 S","1 MIN","TOUCH + KEYS",
                          "OFF","ON","23:00","07:00"};
    const float offset=244;
    for(uint8_t i=0;i<9;++i) {
        auto row=watchcontrols::optionRowBounds(i,offset);
        if(row.y+row.h<=76||row.y>=354) continue;
        watchsettingsrows::drawArrowRow(canvas,view,row,labels[i],values[i],false);
    }
    drawFooter(canvas,view); writePpm(path,canvas);
}
}

int main(int argc,char** argv) {
    assert(ux::textWidth("DISPLAY",ux::Latin28)<=watchcontrols::settingsCarousel().cardWidth-12);
    assert(ux::textWidth(watchstrings::translate("POWER",true),ux::Cjk28)
           <=watchcontrols::settingsCarousel().cardWidth-12);
    assert(ux::textWidth(watchstrings::translate("POWER SAVING",true),ux::Cjk28)<=284);
    assert(ux::textWidth(watchstrings::translate("FORCED OFF",true),ux::Cjk24)
        +ux::textWidth("OFF",ux::Latin24)<=282);
    if(argc>1) {
        const std::string root=argv[1];
        renderCarousel(root+"/settings-en-time.ppm",false,0);
        renderCarousel(root+"/settings-en-bot.ppm",false,1);
        renderCarousel(root+"/settings-en-display.ppm",false,2);
        renderCarousel(root+"/settings-en-sound.ppm",false,3);
        renderCarousel(root+"/settings-en-power.ppm",false,4);
        renderCarousel(root+"/settings-zh-power.ppm",true,4);
        renderPowerBottom(root+"/power-zh-bottom.ppm");
    }
}

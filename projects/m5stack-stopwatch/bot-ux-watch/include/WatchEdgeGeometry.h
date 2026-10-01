#pragma once

#include <stdint.h>

namespace watchedge {

struct RowSpan {
    int16_t x;
    int16_t y;
    int16_t width;
};

constexpr int16_t displaySize() { return 466; }
constexpr int16_t displayCenter() { return 233; }
constexpr int16_t displayRadius() { return 233; }
constexpr int16_t hudHeight() { return 90; }

inline int16_t integerSqrt(int32_t value) {
    if(value<=0) return 0;
    int16_t low=0,high=displayRadius();
    while(low<high) {
        int16_t mid=(int16_t)((low+high+1)/2);
        if((int32_t)mid*mid<=value) low=mid;
        else high=mid-1;
    }
    return low;
}

inline RowSpan displayRowSpan(int16_t y) {
    if(y<0||y>=displaySize()) return {0,y,0};
    int32_t dy=y-displayCenter();
    int16_t half=integerSqrt((int32_t)displayRadius()*displayRadius()-dy*dy);
    int16_t left=displayCenter()-half,right=displayCenter()+half;
    if(left<0) left=0;
    if(right>=displaySize()) right=displaySize()-1;
    return {left,y,(int16_t)(right-left+1)};
}

inline bool displayContains(int x, int y) {
    if(x<0||x>=displaySize()||y<0||y>=displaySize()) return false;
    int32_t dx=x-displayCenter(),dy=y-displayCenter();
    return dx*dx+dy*dy<=(int32_t)displayRadius()*displayRadius();
}

constexpr int16_t batteryToothShoulderY() { return 14; }
constexpr int16_t batteryToothBodyLeft() { return 154; }
constexpr int16_t batteryToothBodyRight() { return 312; }
constexpr int16_t batteryToothRoundStartY() { return 53; }
constexpr int16_t batteryToothBottomY() { return 71; }
constexpr int16_t batteryToothCornerRadius() { return 18; }
constexpr int16_t batteryPercentX() { return 165; }
constexpr int16_t batteryPercentY() { return 18; }
constexpr int16_t batteryGaugeX() { return 232; }
constexpr int16_t batteryGaugeY() { return 21; }

inline RowSpan intersect(RowSpan a, RowSpan b) {
    int16_t left=a.x>b.x?a.x:b.x;
    int16_t aRight=a.x+a.width,bRight=b.x+b.width;
    int16_t right=aRight<bRight?aRight:bRight;
    return {left,a.y,(int16_t)(right>left?right-left:0)};
}

inline RowSpan batteryToothLocalRowSpan(int16_t y) {
    if(y<0||y>batteryToothBottomY()) return {0,y,0};
    if(y<=batteryToothShoulderY()) return displayRowSpan(y);
    int16_t left=batteryToothBodyLeft(),right=batteryToothBodyRight();
    if(y>=batteryToothRoundStartY()) {
        int32_t dy=y-batteryToothRoundStartY();
        int16_t extent=integerSqrt((int32_t)batteryToothCornerRadius()
            *batteryToothCornerRadius()-dy*dy);
        left=(int16_t)(left+batteryToothCornerRadius()-extent);
        right=(int16_t)(right-batteryToothCornerRadius()+extent);
    }
    return {left,y,(int16_t)(right-left+1)};
}

inline int16_t batteryToothOffset(float progress) {
    if(progress<0) progress=0;
    else if(progress>1) progress=1;
    float inverse=1-progress;
    float eased=1-inverse*inverse*inverse;
    int16_t travel=(int16_t)(eased*(batteryToothBottomY()+1)+0.5f);
    return (int16_t)(travel-(batteryToothBottomY()+1));
}

inline RowSpan batteryToothRowSpan(int16_t screenY, int16_t offsetY) {
    if(screenY<0||screenY>=hudHeight()) return {0,screenY,0};
    RowSpan local=batteryToothLocalRowSpan((int16_t)(screenY-offsetY));
    if(local.width<=0) return {0,screenY,0};
    local.y=screenY;
    return intersect(local,displayRowSpan(screenY));
}

inline bool batteryToothContains(int x, int y, int16_t offsetY=0) {
    RowSpan span=batteryToothRowSpan((int16_t)y,offsetY);
    return span.width>0&&x>=span.x&&x<span.x+span.width;
}

enum class BatteryBand : uint8_t { Red, Yellow, Green };

inline BatteryBand batteryBand(uint8_t percent) {
    return percent<=10?BatteryBand::Red:percent<=30?BatteryBand::Yellow:BatteryBand::Green;
}

constexpr uint32_t batteryBandRgb(BatteryBand band) {
    return band==BatteryBand::Green?0x42D978u
        :band==BatteryBand::Yellow?0xF2C94Cu:0xFF5B57u;
}

constexpr uint32_t batteryInkRgb() { return 0x101820u; }

inline uint8_t batteryChargeShade(uint32_t nowMs) {
    uint16_t phase=(uint16_t)(nowMs%1600);
    return (uint8_t)(8+(phase<800?phase:1600-phase)*40/800);
}

inline float batteryGaugeFill(uint8_t percent) {
    if(percent>100) percent=100;
    return percent*38.0f/100.0f;
}

}  // namespace watchedge

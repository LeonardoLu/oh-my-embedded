#pragma once
#include <stdint.h>
#include <math.h>
namespace ux {
inline float clamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
inline uint16_t blend565(uint16_t bg, uint16_t fg, uint8_t a) {
    unsigned inv = 255-a;
    return ((((bg>>11)*inv+(fg>>11)*a+127)/255)<<11)
        | (((((bg>>5)&63)*inv+((fg>>5)&63)*a+127)/255)<<5)
        | (((bg&31)*inv+(fg&31)*a+127)/255);
}
template<class Canvas> inline void pixel(Canvas& c,int x,int y,uint16_t color,uint8_t alpha) {
    if (!alpha || x<0 || y<0 || x>=c.width() || y>=c.height()) return;
    c.drawPixel(x,y,alpha==255 ? color : blend565(c.readPixel(x,y),color,alpha));
}
// Half-width of a scanline inside an inset rounded rectangle.
inline float rowExtent(float qy,float halfWidth,float radius,float inset) {
    float limit=radius-inset;
    if(qy>limit) return -1;
    if(qy<=0) return halfWidth-inset;
    return halfWidth-radius+sqrtf(fmaxf(0,limit*limit-qy*qy));
}
inline float roundedDistance(float qx,float qy,float radius) {
    if(qx<=0 || qy<=0) return fmaxf(qx,qy)-radius;
    return sqrtf(qx*qx+qy*qy)-radius;
}
// Native coverage only at the boundary; opaque interiors use one span per row.
template<class Canvas> void roundRect(Canvas& c,float x,float y,float w,float h,float r,uint16_t color) {
    if(w<=0 || h<=0) return;
    r=clamp(r,0,fminf(w,h)*0.5f);
    int x0=(int)fmaxf(0,floorf(x)),y0=(int)fmaxf(0,floorf(y));
    int x1=(int)fminf(c.width(),ceilf(x+w)),y1=(int)fminf(c.height(),ceilf(y+h));
    float cx=x+w*0.5f;
    for(int py=y0;py<y1;++py) {
        float qy=fabsf(py+0.5f-y-h*0.5f)-h*0.5f+r;
        float extent=rowExtent(qy,w*0.5f,r,0.5f);
        int left=x1,right=x1;
        if(extent>=0) {
            left=(int)clamp(ceilf(cx-extent-0.5f),x0,x1);
            right=(int)clamp(floorf(cx+extent-0.5f)+1,left,x1);
            if(right>left) c.fillRect(left,py,right-left,1,color);
        }
        for(int px=x0;px<x1;++px) {
            if(px==left && right>left) { px=right-1;continue; }
            float qx=fabsf(px+0.5f-cx)-w*0.5f+r;
            float d=roundedDistance(qx,qy,r);
            pixel(c,px,py,color,(uint8_t)(clamp(0.5f-d,0,1)*255+0.5f));
        }
    }
}
// An inward stroke preserves existing content; no background fill is required.
template<class Canvas> void strokeRoundRect(Canvas& c,float x,float y,float w,float h,float r,uint16_t color,float stroke=1) {
    if(w<=0 || h<=0 || stroke<=0) return;
    r=clamp(r,0,fminf(w,h)*0.5f);
    int x0=(int)fmaxf(0,floorf(x)),y0=(int)fmaxf(0,floorf(y));
    int x1=(int)fminf(c.width(),ceilf(x+w)),y1=(int)fminf(c.height(),ceilf(y+h));
    float cx=x+w*0.5f;
    for(int py=y0;py<y1;++py) {
        float qy=fabsf(py+0.5f-y-h*0.5f)-h*0.5f+r;
        float extent=rowExtent(qy,w*0.5f,r,stroke+0.5f);
        int left=x1,right=x1;
        if(extent>=0) { left=(int)clamp(ceilf(cx-extent-0.5f),x0,x1);right=(int)clamp(floorf(cx+extent-0.5f)+1,left,x1); }
        for(int px=x0;px<x1;++px) {
            if(px==left && right>left) { px=right-1;continue; }
            float d=roundedDistance(fabsf(px+0.5f-cx)-w*0.5f+r,qy,r);
            float a=clamp(0.5f-d,0,1)-clamp(0.5f-d-stroke,0,1);
            pixel(c,px,py,color,(uint8_t)(a*255+0.5f));
        }
    }
}
template<class Canvas> void circle(Canvas& c,float x,float y,float radius,uint16_t color) {
    roundRect(c,x-radius,y-radius,2*radius,2*radius,radius,color);
}
template<class Canvas> void line(Canvas& c,float ax,float ay,float bx,float by,float width,uint16_t color) {
    if(width<=0) return;
    float dx=bx-ax,dy=by-ay,len=dx*dx+dy*dy,r=width*0.5f;
    int x0=(int)fmaxf(0,floorf(fminf(ax,bx)-r-1)),y0=(int)fmaxf(0,floorf(fminf(ay,by)-r-1));
    int x1=(int)fminf(c.width(),ceilf(fmaxf(ax,bx)+r+1)),y1=(int)fminf(c.height(),ceilf(fmaxf(ay,by)+r+1));
    for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) {
        float t=len>0?clamp(((x+0.5f-ax)*dx+(y+0.5f-ay)*dy)/len,0,1):0;
        float ex=x+0.5f-ax-t*dx,ey=y+0.5f-ay-t*dy;
        pixel(c,x,y,color,(uint8_t)(clamp(r+0.5f-sqrtf(ex*ex+ey*ey),0,1)*255+0.5f));
    }
}
}

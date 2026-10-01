#pragma once
#include <stdint.h>
#include <math.h>
#include "UxRender.h"
namespace ux {
// Positive offset scrolls content upward. Call begin/move/end from touch events,
// then update every frame. A drag never turns back into a tap on release.
class ScrollModel {
public:
    void setBounds(float contentHeight,float viewportHeight) { _max=fmaxf(0,contentHeight-viewportHeight); _offset=clamp(_offset,0,_max); }
    void setOffset(float value) { _offset=clamp(value,0,_max); _velocity=0; }
    float offset() const { return _offset; }
    bool dragging() const { return _drag; }
    bool active() const { return _down; }
    void begin(float y,uint32_t now) { _down=true;_drag=false;_startY=_lastY=y;_startOffset=_offset;_lastMs=_lastMotionMs=now;_velocity=0; }
    void move(float y,uint32_t now) {
        if(!_down) return;
        if(fabsf(y-_startY)>=6) _drag=true;
        if(_drag) {
            uint32_t dt=now-_lastMs;
            float before=_offset;
            _offset=clamp(_startOffset+_startY-y,0,_max);
            if(dt && _offset!=before) {
                _velocity=clamp((_offset-before)/dt,-2.5f,2.5f);
                _lastMotionMs=now;
            } else if(now-_lastMotionMs>90) _velocity=0;
        }
        _lastY=y;_lastMs=now;
    }
    bool end(uint32_t now) { bool tap=_down&&!_drag; if(now-_lastMotionMs>90) _velocity=0; _down=false; return tap; }
    void cancel() { _down=false;_drag=false;_velocity=0; }
    bool update(uint32_t dtMs) {
        if(_down || fabsf(_velocity)<0.005f) return false;
        float dt=fminf(dtMs,50), before=_offset;
        float decay=expf(-dt/180.0f);
        _offset=clamp(_offset+_velocity*180*(1-decay),0,_max);
        _velocity*=decay;
        if(_offset==0 || _offset==_max) _velocity=0;
        return fabsf(_offset-before)>0.01f;
    }
private:
    float _max=0,_offset=0,_velocity=0,_startY=0,_lastY=0,_startOffset=0;
    uint32_t _lastMs=0,_lastMotionMs=0; bool _down=false,_drag=false;
};
struct Rect { int x,y,w,h; bool contains(int px,int py) const { return px>=x&&py>=y&&px<x+w&&py<y+h; } };
}

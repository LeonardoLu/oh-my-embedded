#include "TouchContact.h"
#include "UxPointer.h"
#include <assert.h>
int main() {
    watchinput::TouchContact c; ux::PointerSession p; ux::Rect row{62,76,342,58};
    c.sample(true,200,100); assert(c.wasPressed()); p.begin(100,row,c.x,c.y,100,true);
    c.sample(true,200,60); p.move(c.x,c.y); assert(p.scrolling());
    c.sample(false,0,0); assert(c.wasReleased()&&c.x==200&&c.y==60);
    assert(p.end(c.x,c.y,300)==-1);
    // No idle poll between scroll release and the next contact: it is a new down.
    c.sample(true,200,100); assert(c.wasPressed()&&c.isPressed());
    p.begin(100,row,c.x,c.y,310,true);
    c.sample(true,212,112); assert(!c.wasPressed()); p.move(c.x,c.y);
    c.sample(false,-1,-1); assert(c.wasReleased()); assert(p.end(c.x,c.y,1200)==100);
    c.sample(false,0,0); assert(!c.wasReleased());
    watchinput::RegionDoubleTap tap;
    assert(!tap.tap(true,100)); assert(tap.tap(true,520));
    assert(!tap.tap(true,1000)); assert(!tap.tap(false,1100)); assert(!tap.tap(true,1200));
    assert(!tap.tap(true,1700)); assert(tap.tap(true,1800));
}

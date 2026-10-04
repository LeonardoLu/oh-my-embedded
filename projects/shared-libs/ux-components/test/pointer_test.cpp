#include "UxPointer.h"
#include <assert.h>
int main() {
    ux::PointerSession p; ux::Rect button{244,374,150,58}, row{62,76,342,58};
    // TouchUpInside: drift, leave/re-enter and exact 1 s inclusive.
    p.begin(2,button,310,396,100,false); p.move(322,408); assert(p.end(322,408,1100)==2);
    p.begin(2,button,300,399,2000,false); p.move(170,399); assert(!p.pressed());
    p.move(300,399); assert(p.pressed()); assert(p.end(300,399,2900)==2);
    p.begin(2,button,300,399,3000,false); assert(p.end(243,399,3200)==-1);
    p.begin(2,button,300,399,3000,false); assert(p.end(300,399,4001)==-1);
    p.begin(2,button,300,399,0xFFFFFF00u,false); assert(p.end(300,399,0x200)==2);
    // Horizontal movement within a row is still touch-up-inside; vertical drag owns release.
    p.begin(100,row,200,100,0,true); p.move(250,103); assert(p.end(250,103,800)==100);
    p.begin(100,row,200,100,0,true); p.move(212,119); assert(!p.scrolling());
    assert(p.end(212,119,900)==100);
    p.begin(100,row,200,100,0,true); p.move(201,80); assert(p.scrolling());
    p.move(200,100); assert(p.end(200,100,400)==-1);
    p.begin(-1,{0,0,0,0},200,140,0,true); p.move(200,110); assert(p.scrolling());
    assert(p.end(200,110,400)==-1);
    p.begin(100,row,200,100,0,true); p.cancel(); assert(p.end(200,100,100)==-1);
    ux::ScrollModel scroll; scroll.setBounds(720,288); scroll.setOffset(100);
    p.begin(100,row,200,100,100,true); p.move(200,70);
    scroll.begin(p.startY(),p.startedAt()); scroll.move(p.y(),140); assert(scroll.offset()==130);
    p.move(200,60); scroll.move(p.y(),160); assert(p.end(200,60,160)==-1);
    assert(!scroll.end(160)); assert(scroll.update(16));
    scroll.cancel(); float stopped=scroll.offset(); assert(!scroll.update(50)); assert(scroll.offset()==stopped);
}

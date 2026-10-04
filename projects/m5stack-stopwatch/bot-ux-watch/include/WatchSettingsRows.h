#pragma once

#include <UxInput.h>
#include <UxRender.h>

namespace watchsettingsrows {

template<class Canvas,class View>
void drawArrowRow(Canvas& canvas,View& view,ux::Rect row,
                  const char* label,const char* value,bool selected) {
    const int16_t cy=row.y+row.h/2;
    const bool pressed=view.pressed(row);
    if(selected||pressed) ux::roundRect(canvas,row.x,row.y,row.w,row.h,17,
        pressed?ux::blend565(view.panel(),view.accent(),55):view.panel());
    view.left(label,84,cy,view.muted());
    view.right(value,382,cy,view.ink());
    view.center("<",68,cy,view.accent());
    view.center(">",398,cy,view.accent());
}

} // namespace watchsettingsrows

#pragma once

#include "WatchControls.h"
#include "WatchSettingsIcons.h"
#include <UxRender.h>

namespace watchsettingscarousel {

// View supplies palette, pressed-state lookup and native text drawing. Keeping
// the launcher renderer here lets firmware and host evidence use identical geometry.
template<class Canvas,class IconCanvas,class View>
void draw(Canvas& canvas,IconCanvas& iconCanvas,View& view,float offset,uint8_t selected,
          const char* const labels[]) {
    const auto layout=watchcontrols::settingsCarousel();
    const uint8_t count=(uint8_t)MenuItem::Done;
    for(uint8_t i=0;i<count;++i) {
        const auto item=watchcontrols::carouselCardBounds(i,offset);
        if(item.x+item.w<=layout.x||item.x>=layout.x+layout.w) continue;
        const int16_t cx=item.x+item.w/2;
        watchsettingsicons::draw(iconCanvas,(MenuItem)i,cx,215,view.background(),view.ink());
        view.large(labels[i],cx,340,view.pressed(item)?view.muted():view.ink());
    }
    const uint16_t quiet=ux::blend565(view.background(),view.ink(),128);
    for(uint8_t i=0;i<count;++i)
        ux::circle(canvas,201+i*16,378,i==selected?7:4,
                   i==selected?view.ink():quiet);

    const auto previous=watchcontrols::carouselPreviousBounds();
    const auto next=watchcontrols::carouselNextBounds();
    const uint16_t previousColor=view.pressed(previous)?view.ink():view.muted();
    const uint16_t nextColor=view.pressed(next)?view.ink():view.muted();
    ux::line(canvas,69,201,55,216,6,previousColor);
    ux::line(canvas,55,216,69,231,6,previousColor);
    ux::line(canvas,397,201,411,216,6,nextColor);
    ux::line(canvas,411,216,397,231,6,nextColor);
}

template<class Canvas,class View>
void draw(Canvas& canvas,View& view,float offset,uint8_t selected,
          const char* const labels[]) {
    draw(canvas,canvas,view,offset,selected,labels);
}

} // namespace watchsettingscarousel

#pragma once

#include "WatchControls.h"
#include "WatchSettingsIconAssets.h"

namespace watchsettingsicons {

inline watchsettingsiconassets::Icon assetFor(MenuItem item) {
    using watchsettingsiconassets::Icon;
    switch(item) {
        case MenuItem::Time: return Icon::Time;
        case MenuItem::Personalize: return Icon::Bot;
        case MenuItem::Display: return Icon::Display;
        case MenuItem::Sound: return Icon::Sound;
        case MenuItem::Power: return Icon::Power;
        default: return Icon::Count;
    }
}

template<class Canvas>
void draw(Canvas& canvas,MenuItem item,int16_t cx,int16_t cy,
          uint16_t,uint16_t) {
    watchsettingsiconassets::draw(canvas,assetFor(item),cx,cy);
}

} // namespace watchsettingsicons

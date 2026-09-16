#pragma once
#include <UxInput.h>
#include <UxKeyboard.h>
#include "WatchEdgeGeometry.h"

enum class Screen : uint8_t { Face, Settings, Personalize, Editor, TimeSettings };
enum class Editor : uint8_t { None, Time, Date, Format, Expression, Appearance, Motion, Color, Display, Name, Preview, Layout, Language, Gaze, Sound, Power };
enum class MenuItem : uint8_t { Time, Personalize, Display, Sound, Power, Done, Count };
enum class TimeItem : uint8_t { Time, Date, Format, Back, Count };
enum class PersonalItem : uint8_t { Expression, Action, Appearance, Color, Name, Language, Preview, Gaze, Intensity, Speed, Back, Count };

namespace watchcontrols {
enum Id { None=-1, Done=1, CarouselPrevious=2, CarouselNext=3, First=10,
          NameKey=40, MenuRow=100, TimeRow=150, PersonalRow=200,
          ColorPad=300, HueBar=301 };
struct Target { int id; ux::Rect bounds; int16_t radius; };
struct ListLayout { int16_t x,y,w,h,step,rowHeight,radius; uint8_t visibleRows; };
struct CarouselLayout { int16_t x,y,w,h,step,cardWidth,cardHeight,radius; };
constexpr ListLayout mainList() { return {62,76,342,264,66,58,25,4}; }
constexpr CarouselLayout settingsCarousel() { return {0,76,466,330,466,220,250,0}; }
// Settings animation only changes this content band. It contains the full
// 200 px icon, both arrows, the translated label and every page dot, while
// leaving the static title and Done footer outside the panel transfer.
constexpr ux::Rect settingsCarouselMotionBand() { return {0,110,466,280}; }
constexpr ux::Rect carouselPreviousBounds() { return {34,176,56,80}; }
constexpr ux::Rect carouselNextBounds() { return {376,176,56,80}; }
constexpr ListLayout previewList() { return {58,242,350,96,48,44,17,2}; }
constexpr ux::Rect doneBounds() { return {0,406,466,60}; }
constexpr int16_t doneCircleCenterX() { return watchedge::displayCenter(); }
constexpr int16_t doneCircleCenterY() { return watchedge::displayCenter(); }
constexpr int16_t doneLabelY() { return 432; }
constexpr int16_t doneLabelX() { return doneCircleCenterX(); }
constexpr int16_t doneCircleRadius() { return watchedge::displayRadius(); }
constexpr ux::Rect nameKeyboardBounds() { return {78,126,310,214}; }
constexpr ux::Rect useThemeBounds() { return {163,207,140,32}; }
constexpr ux::Rect colorPadBounds() { return {66,242,260,96}; }
constexpr ux::Rect hueBarBounds() { return {344,242,56,96}; }
constexpr ListLayout optionList() { return {58,76,350,278,58,44,17,5}; }
constexpr ListLayout displayList() { return optionList(); }
constexpr uint8_t displayRowCount() { return 6; }
constexpr uint8_t soundRowCount() { return 4; }
constexpr uint8_t powerRowCount() { return 9; }
inline int16_t optionRowCenter(uint8_t index, float offset=0) {
    return (int16_t)(100+index*optionList().step-(int16_t)offset);
}
inline ux::Rect optionRowBounds(uint8_t index, float offset=0) {
    const auto layout=optionList();
    return {layout.x,(int16_t)(optionRowCenter(index,offset)-layout.rowHeight/2),
            layout.w,layout.rowHeight};
}
inline int16_t displayRowCenter(uint8_t index, float offset=0) {
    return optionRowCenter(index,offset);
}
inline ux::Rect displayRowBounds(uint8_t index, float offset=0) {
    return optionRowBounds(index,offset);
}

inline ux::Rect carouselCardBounds(uint8_t index,float offset) {
    const auto layout=settingsCarousel();
    int16_t center=(int16_t)lroundf(233.0f+index*layout.step-offset);
    return {center-layout.cardWidth/2,layout.y+(layout.h-layout.cardHeight)/2,
            layout.cardWidth,layout.cardHeight};
}
inline bool carouselDragIntent(int dx,int dy) {
    if(dx<0) dx=-dx;
    if(dy<0) dy=-dy;
    return dx>=14&&dx>=dy;
}

// A fixed-size horizontal model for the five top-level settings cards. Finger
// motion follows one-to-one, and release settles to the nearest projected card.
class CarouselModel {
public:
    void configure(uint8_t count,int16_t step) {
        _count=count; _step=step>0?step:1;
        if(_selected>=_count) _selected=_count?_count-1:0;
        _offset=targetOffset(); _velocity=0; _settling=false; _down=false;
    }
    void select(uint8_t item,bool animated=false) {
        if(!_count) return;
        _selected=item<_count?item:_count-1;
        _velocity=0; _settling=animated;
        if(!animated) _offset=targetOffset();
    }
    void moveSelection(int8_t delta) {
        if(!_count||!delta) return;
        int next=((int)_selected+delta)%_count;
        if(next<0) next+=_count;
        select((uint8_t)next,true);
    }
    void begin(float x,uint32_t now) {
        _down=true; _drag=false; _settling=false;
        _startX=x; _startOffset=_offset;
        _lastMs=_lastMotionMs=now; _velocity=0;
    }
    void move(float x,uint32_t now) {
        if(!_down) return;
        if(fabsf(x-_startX)>=14.0f) _drag=true;
        if(_drag) {
            float before=_offset;
            _offset=clamp(_startOffset+_startX-x,0,maxOffset());
            uint32_t dt=now-_lastMs;
            if(dt&&_offset!=before) {
                _velocity=clamp((_offset-before)/dt,-2.5f,2.5f);
                _lastMotionMs=now;
            } else if(now-_lastMotionMs>90) _velocity=0;
            chooseNearest(_offset);
        }
        _lastMs=now;
    }
    bool end(uint32_t now) {
        bool dragged=_down&&_drag;
        if(dragged) {
            if(now-_lastMotionMs>90) _velocity=0;
            chooseNearest(clamp(_offset+_velocity*110.0f,0,maxOffset()));
        }
        if(_down) _settling=true;
        _down=false;
        return dragged;
    }
    bool update(uint32_t dtMs) {
        if(_down||!_settling) return false;
        float before=_offset,target=targetOffset();
        float dt=dtMs>50?50.0f:(float)dtMs;
        _offset+=(target-_offset)*(1.0f-expf(-dt/82.0f));
        if(fabsf(target-_offset)<0.25f) { _offset=target; _settling=false; }
        return fabsf(_offset-before)>0.01f;
    }
    void setOffset(float value) {
        _offset=clamp(value,0,maxOffset()); chooseNearest(_offset);
        _velocity=0; _settling=false; _down=false; _drag=false;
    }
    void cancel() {
        if(_down) { chooseNearest(_offset); _settling=_offset!=targetOffset(); }
        _down=false; _drag=false; _velocity=0;
    }
    float offset() const { return _offset; }
    uint8_t selected() const { return _selected; }
    bool active() const { return _down; }
    bool dragging() const { return _down&&_drag; }
    bool settling() const { return _settling; }
private:
    static float clamp(float value,float low,float high) {
        return value<low?low:value>high?high:value;
    }
    float maxOffset() const { return _count?(float)(_count-1)*_step:0; }
    float targetOffset() const { return (float)_selected*_step; }
    void chooseNearest(float value) {
        if(!_count) { _selected=0; return; }
        int item=(int)floorf(value/_step+0.5f);
        if(item<0) item=0;
        if(item>=_count) item=_count-1;
        _selected=(uint8_t)item;
    }
    float _offset=0,_velocity=0,_startX=0,_startOffset=0;
    uint32_t _lastMs=0,_lastMotionMs=0;
    int16_t _step=1;
    uint8_t _count=0,_selected=0;
    bool _down=false,_drag=false,_settling=false;
};

inline bool doneContains(int x, int y) {
    const auto bounds=doneBounds();
    return bounds.contains(x,y)&&watchedge::displayContains(x,y);
}

// The visible footer is the same circular segment used for hit testing. Each
// returned row is {left, y, width, 1}, with a half-open horizontal interval.
inline ux::Rect doneRowSpan(int16_t y) {
    const auto bounds=doneBounds();
    if(y<bounds.y||y>=bounds.y+bounds.h) return {0,y,0,0};
    auto span=watchedge::displayRowSpan(y);
    return {span.x,span.y,span.width,1};
}

inline bool roundedContains(ux::Rect r, int16_t radius, int x, int y) {
    if (!r.contains(x,y)) return false;
    if (radius <= 0) return true;
    int16_t maxRadius=(r.w<r.h?r.w:r.h)/2;
    if(radius>maxRadius) radius=maxRadius;
    if(x>=r.x+radius&&x<r.x+r.w-radius) return true;
    if(y>=r.y+radius&&y<r.y+r.h-radius) return true;
    int32_t cx=x<r.x+radius?r.x+radius:r.x+r.w-radius-1;
    int32_t cy=y<r.y+radius?r.y+radius:r.y+r.h-radius-1;
    int32_t dx=x-cx,dy=y-cy;
    return dx*dx+dy*dy<=(int32_t)radius*radius;
}

inline bool editorUsesPreviewList(Editor editor) {
    return editor==Editor::Appearance||editor==Editor::Motion||editor==Editor::Preview||editor==Editor::Gaze;
}
inline bool editorUsesScrollList(Editor editor) {
    return editorUsesPreviewList(editor)||editor==Editor::Display
        ||editor==Editor::Sound||editor==Editor::Power;
}
constexpr ListLayout editorList(Editor editor) {
    return editor==Editor::Display||editor==Editor::Sound||editor==Editor::Power
        ? optionList() : previewList();
}
inline uint8_t editorRowCount(Editor editor) {
    switch(editor) {
        case Editor::Appearance:return 2; case Editor::Motion:return 4;
        case Editor::Preview:return 3; case Editor::Gaze:return 1;
        case Editor::Display:return displayRowCount(); case Editor::Sound:return soundRowCount();
        case Editor::Power:return powerRowCount();
        default:return 0;
    }
}
inline ux::Rect rowBounds(ListLayout layout,uint8_t index,float offset) {
    int16_t top=layout.y+index*layout.step-(int16_t)offset+(layout.step-layout.rowHeight)/2;
    return {layout.x,top,layout.w,layout.rowHeight};
}
inline bool inViewport(ListLayout layout,int x,int y) {
    return x>=layout.x&&x<layout.x+layout.w&&y>=layout.y&&y<layout.y+layout.h;
}
inline Target at(Screen screen, Editor editor, float offset, int x, int y) {
    Target result{None,{0,0,0,0},0};
    auto match = [&](int id, ux::Rect bounds, int16_t radius=0) {
        if(result.id==None&&roundedContains(bounds,radius,x,y)) result={id,bounds,radius};
    };
    if (screen != Screen::Face) {
        if(doneContains(x,y)) return {Done,doneBounds(),0};
    }
    if (screen == Screen::Settings) {
        const auto layout=settingsCarousel();
        if(x<layout.x||x>=layout.x+layout.w||y<layout.y||y>=layout.y+layout.h)
            return result;
        if(roundedContains(carouselPreviousBounds(),28,x,y))
            return {CarouselPrevious,carouselPreviousBounds(),28};
        if(roundedContains(carouselNextBounds(),28,x,y))
            return {CarouselNext,carouselNextBounds(),28};
        for(uint8_t i=0;i<(uint8_t)MenuItem::Done;++i) {
            auto bounds=carouselCardBounds(i,offset);
            if(roundedContains(bounds,layout.radius,x,y))
                return {MenuRow+i,bounds,layout.radius};
        }
        return result;
    }
    if (screen == Screen::TimeSettings || screen == Screen::Personalize) {
        const auto layout=mainList();
        if(!inViewport(layout,x,y)) return result;
        int count = screen == Screen::TimeSettings ? (int)TimeItem::Back : (int)PersonalItem::Back;
        for (int i=0; i<count; ++i) {
            auto bounds=rowBounds(layout,(uint8_t)i,offset);
            if(roundedContains(bounds,layout.radius,x,y))
                return {screen==Screen::TimeSettings?TimeRow+i:PersonalRow+i,bounds,layout.radius};
        }
        return result;
    }
    if (screen != Screen::Editor) return result;

    if(editorUsesPreviewList(editor)) {
        const auto layout=previewList();
        if(!inViewport(layout,x,y)) return result;
        for(uint8_t i=0;i<editorRowCount(editor);++i) {
            auto bounds=rowBounds(layout,i,offset);
            if(roundedContains(bounds,layout.radius,x,y)) return {First+i,bounds,layout.radius};
        }
    } else if (editor==Editor::Display||editor==Editor::Sound||editor==Editor::Power) {
        const auto layout=optionList();
        if(!inViewport(layout,x,y)) return result;
        for(uint8_t i=0;i<editorRowCount(editor);++i) {
            auto bounds=optionRowBounds(i,offset);
            if(roundedContains(bounds,layout.radius,x,y)) return {First+i,bounds,layout.radius};
        }
    } else if (editor==Editor::Name) {
        const auto area=nameKeyboardBounds();
        int key=ux::nameKeyAt(area,x,y);
        if (key>=0) {
            const auto rect=ux::nameKeyRect(area,key);
            match(NameKey+key,rect,6);
        }
    } else if (editor==Editor::Language) {
        match(First,{72,160,152,70},35); match(First+1,{242,160,152,70},35);
    } else if (editor==Editor::Layout) {
        const int rows[]={170,250};
        for(int i=0;i<2;++i) match(First+i,{58,rows[i]-22,350,44},17);
    } else if (editor==Editor::Color) {
        match(First,useThemeBounds(),16); match(ColorPad,colorPadBounds()); match(HueBar,hueBarBounds());
    } else if (editor==Editor::Time || editor==Editor::Date) {
        const int timeCenters[]={157,309}, dateCenters[]={108,233,358};
        const int* centers=editor==Editor::Time?timeCenters:dateCenters;
        int count=editor==Editor::Time?2:3, width=editor==Editor::Time?100:86;
        for(int i=0;i<count;++i) {
            match(First+i*3,{centers[i]-width/2,116,width,52},26);
            match(First+i*3+1,{centers[i]-width/2,256,width,52},26);
            match(First+i*3+2,{centers[i]-width/2-5,175,width+10,72});
        }
    } else if (editor==Editor::Format) {
        match(First,{78,128,146,76},38); match(First+1,{242,128,146,76},38); match(First+2,{100,242,266,58},29);
    } else if (editor==Editor::Expression) {
        match(First,{48,126,76,96},38); match(First+1,{342,126,76,96},38); match(First+2,{144,62,178,178},89);
    }
    return result;
}
}

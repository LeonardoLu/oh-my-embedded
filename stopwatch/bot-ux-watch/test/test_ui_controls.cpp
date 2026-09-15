#include "WatchControls.h"
#include "UxPointer.h"
#include <assert.h>
#include <initializer_list>
using namespace watchcontrols;
int main() {
    // Every non-face screen shares the exact same single Done target.
    for(int e=(int)Editor::Time;e<=(int)Editor::Power;++e) {
        assert(at(Screen::Editor,(Editor)e,0,doneLabelX(),doneLabelY()).id==Done);
        assert(at(Screen::Editor,(Editor)e,0,0,351).id==None);
    }
    for(auto screen:{Screen::Settings,Screen::TimeSettings,Screen::Personalize}) for(int off=0;off<500;++off) {
        assert(at(screen,Editor::None,off,doneLabelX(),doneLabelY()).id==Done);
    }
    auto done=doneBounds();
    assert(done.x==0&&done.y==406&&done.w==466&&done.h==60);
    assert(doneCircleCenterX()==233&&doneCircleCenterY()==233&&doneCircleRadius()==233);
    assert(doneLabelX()==233&&doneLabelY()==432);
    // Exhaustively prove that every visible scanline and hit uses the same
    // bottom segment of the mathematical 466 px display circle.
    for(int y=-1;y<=466;++y) {
        auto span=doneRowSpan((int16_t)y);
        bool validRow=y>=done.y&&y<done.y+done.h;
        assert((span.w>0)==validRow);
        assert(span.h==(validRow?1:0));
        if(validRow) assert(span.y==y&&span.x>=0&&span.x+span.w<=466);
        for(int x=0;x<466;++x) {
            int dx=x-233,dy=y-233;
            bool expected=validRow&&dx*dx+dy*dy<=233*233;
            assert(doneContains(x,y)==expected);
            assert((validRow&&x>=span.x&&x<span.x+span.w)==expected);
            assert((at(Screen::Settings,Editor::None,0,x,y).id==Done)==expected);
        }
    }
    assert(doneRowSpan(405).w==0);
    assert(doneRowSpan(406).x==77&&doneRowSpan(406).w==313);
    assert(doneRowSpan(465).x==212&&doneRowSpan(465).w==43);
    assert(!doneContains(-1,doneLabelY())&&!doneContains(466,doneLabelY()));
    // Every top-level card centers at its exact horizontal snap position.
    auto carousel=settingsCarousel();
    assert(carousel.x==0&&carousel.y==76&&carousel.w==466&&carousel.h==330);
    assert(carousel.step==466&&carousel.cardWidth==220&&carousel.cardHeight==250);
    for(uint8_t i=0;i<(uint8_t)MenuItem::Done;++i) {
        float offset=i*carousel.step;
        auto card=carouselCardBounds(i,offset);
        assert(card.x+card.w/2==233&&card.y==116&&card.y+card.h==366);
        assert(at(Screen::Settings,Editor::None,offset,233,220).id==MenuRow+i);
    }
    assert(at(Screen::Settings,Editor::None,0,62,216).id==CarouselPrevious);
    assert(at(Screen::Settings,Editor::None,0,404,216).id==CarouselNext);
    // At half travel the outgoing and incoming 200 px icon slots only peek at
    // opposite edges; the fixed arrow panels keep their own hit priority.
    assert(at(Screen::Settings,Editor::None,233,100,300).id==MenuRow);
    assert(at(Screen::Settings,Editor::None,233,360,300).id==MenuRow+1);
    assert(at(Screen::Settings,Editor::None,233,233,300).id==None);
    assert(at(Screen::Settings,Editor::None,233,233,75).id==None);
    assert(at(Screen::Settings,Editor::None,0,233,378).id==None);
    assert(at(Screen::Settings,Editor::None,233,233,405).id==None);
    assert(!carouselDragIntent(13,0));
    assert(carouselDragIntent(-14,12));
    assert(!carouselDragIntent(18,22));
    // Time and Personality retain their bounded vertical lists.
    assert(at(Screen::TimeSettings,Editor::None,0,233,109).id==TimeRow+(int)TimeItem::Time);
    assert(at(Screen::TimeSettings,Editor::None,0,233,241).id==TimeRow+(int)TimeItem::Format);
    int personalMax=(int)PersonalItem::Back*66-264;
    assert(at(Screen::Personalize,Editor::None,personalMax,233,321).id==PersonalRow+(int)PersonalItem::Speed);
    // Preview editors show two 48 px steps with unchanged 44 px controls.
    assert(editorRowCount(Editor::Motion)==4&&previewList().visibleRows==2);
    assert(at(Screen::Editor,Editor::Motion,0,233,266).id==First);
    assert(at(Screen::Editor,Editor::Motion,96,233,314).id==First+3);
    assert(at(Screen::Editor,Editor::Preview,48,233,314).id==First+2);
    // Shared keyboard draw/hit rectangles, including gap rejection.
    ux::Rect area=nameKeyboardBounds();
    for(int k=0;k<30;++k) {
        auto r=ux::nameKeyRect(area,k);
        assert(at(Screen::Editor,Editor::Name,0,r.x+r.w/2,r.y+r.h/2).id==NameKey+k);
        assert(r.h>=38);
    }
    assert(area.y+area.h==340);
    assert(at(Screen::Editor,Editor::Name,0,area.x,area.y).id==None);
    assert(at(Screen::Editor,Editor::Color,0,160,290).id==ColorPad);
    assert(at(Screen::Editor,Editor::Color,0,360,290).id==HueBar);
    assert(colorPadBounds().y+colorPadBounds().h==338);
    assert(hueBarBounds().y+hueBarBounds().h==338);
    assert(at(Screen::Editor,Editor::Color,0,160,338).id==None);
    assert(displayRowCount()==6&&soundRowCount()==4&&powerRowCount()==9);
    auto display=displayList();
    assert(display.x==58&&display.y==76&&display.w==350&&display.h==278&&display.step==58);
    assert(editorUsesScrollList(Editor::Display));
    assert(editorList(Editor::Display).h==display.h);
    assert(!editorUsesPreviewList(Editor::Display));
    // Display retains both layout controls and Sound fits without scrolling;
    // Power reaches its schedule endpoints at the bounded offset.
    for(uint8_t i=0;i<displayRowCount();++i) {
        auto row=displayRowBounds(i);
        assert(displayRowCenter(i)==100+i*58);
        if(i<5) {
            assert(row.y>=76&&row.y+row.h<=354);
            assert(at(Screen::Editor,Editor::Display,0,233,displayRowCenter(i)).id==First+i);
        } else {
            assert(at(Screen::Editor,Editor::Display,0,233,displayRowCenter(i)).id!=First+i);
        }
    }
    constexpr float displayEndOffset=70;
    assert(at(Screen::Editor,Editor::Display,displayEndOffset,233,
              optionRowCenter(5,displayEndOffset)).id==First+5);
    for(uint8_t i=0;i<soundRowCount();++i)
        assert(at(Screen::Editor,Editor::Sound,0,233,optionRowCenter(i)).id==First+i);
    constexpr float powerEndOffset=244;
    assert(optionRowCenter(7,powerEndOffset)==262);
    assert(optionRowCenter(8,powerEndOffset)==320);
    assert(at(Screen::Editor,Editor::Power,powerEndOffset,233,optionRowCenter(7,powerEndOffset)).id==First+7);
    assert(at(Screen::Editor,Editor::Power,powerEndOffset,233,optionRowCenter(8,powerEndOffset)).id==First+8);
    assert(at(Screen::Editor,Editor::Display,0,233,355).id==None);
    // Horizontal release projects a quick flick and eases to the selected card.
    CarouselModel model; model.configure(5,carousel.step);
    model.begin(233,0); model.move(90,100);
    assert(model.dragging()&&model.offset()==143&&model.selected()==0);
    assert(model.end(100)&&model.selected()==1&&model.settling());
    for(int i=0;i<80&&model.settling();++i) model.update(16);
    assert(!model.settling()&&model.offset()==466);
    // A tap that interrupts an animated button move resumes the same snap.
    model.select(4); model.select(2,true); model.update(16);
    float interrupted=model.offset(); assert(interrupted>932&&interrupted<1864);
    model.begin(233,200); assert(!model.end(220)&&model.selected()==2&&model.settling());
    for(int i=0;i<80&&model.settling();++i) model.update(16);
    assert(model.offset()==932);
    model.select(4); model.moveSelection(1);
    assert(model.selected()==0&&model.settling());
    // The carousel also gives PointerSession vertical ownership. A deliberate
    // vertical drag cannot turn back into a card activation on release.
    ux::PointerSession carouselPointer;
    auto firstCard=carouselCardBounds(0,0);
    carouselPointer.begin(MenuRow,firstCard,233,220,0,true);
    carouselPointer.move(245,242);
    assert(carouselPointer.scrolling());
    assert(carouselPointer.end(233,220,400)==None);
    // Buttons accept up-inside through 1 s; page cancellation consumes the release.
    ux::PointerSession p; auto t=at(Screen::Editor,Editor::Format,0,doneLabelX(),doneLabelY());
    p.begin(t.id,t.bounds,doneLabelX(),doneLabelY(),100,false); p.move(250,409);
    assert(p.end(250,409,1000)==Done);
    p.begin(t.id,t.bounds,doneLabelX(),doneLabelY(),100,false); p.cancel();
    assert(p.end(doneLabelX(),doneLabelY(),300)==None);
}

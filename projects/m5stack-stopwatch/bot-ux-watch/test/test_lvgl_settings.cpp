#include "WatchLvgl.h"
#include "WatchControls.h"
#include "WatchStrings.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
uint32_t testMillis=0;
M5Test M5;
struct Event { WatchLvgl::Action action; int row; int value; };
std::vector<Event> events;
void action(WatchLvgl::Action action,uint8_t row,int value,const char*) { events.push_back({action,row,value}); }
void advance(WatchLvgl& ui,int ms) { for(int i=0;i<ms;i+=8) { testMillis+=8; ui.update(testMillis,true); } }
void tap(WatchLvgl& ui,int x,int y) { ui.pointer(true,x,y); advance(ui,80); ui.pointer(false,x,y); advance(ui,80); }
bool hasLabel(lv_obj_t* parent,const char* text) {
    if(lv_obj_check_type(parent,&lv_label_class)&&!strcmp(lv_label_get_text(parent),text)) return true;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(parent);++i)
        if(hasLabel(lv_obj_get_child(parent,i),text)) return true;
    return false;
}
void checkFooter(WatchLvgl& ui,const M5Canvas& canvas) {
    const auto bounds=watchcontrols::doneBounds();
    auto* footer=lv_obj_get_child(lv_scr_act(),-2); // The error label follows Done.
    lv_area_t area; lv_obj_get_coords(footer,&area);
    assert(area.x1==0&&area.y1==406&&area.x2==465&&area.y2==465);
    for(int y=bounds.y;y<bounds.y+bounds.h;++y) for(int x=0;x<466;++x) {
        lv_point_t point={(lv_coord_t)x,(lv_coord_t)y};
        const bool inside=watchcontrols::doneContains(x,y);
        assert(lv_obj_hit_test(footer,&point)==inside);
        // Check the rendered curve independently of the label's glyph pixels.
        if(x<180||x>286||y<416||y>448) {
            if((canvas.pixels[y*466+x]!=lv_color_hex(0x080D10).full)!=inside)
                fprintf(stderr,"Footer raster (%d,%d): color %04x, inside %d\n",x,y,canvas.pixels[y*466+x],inside);
            assert((canvas.pixels[y*466+x]!=lv_color_hex(0x080D10).full)==inside);
        }
    }
    events.clear();
    for(const auto& point:std::vector<lv_point_t>{{80,406},{386,408},{110,415},{318,440},{233,465}}) {
        tap(ui,point.x,point.y);
        if(events.size()!=1||events.back().action!=WatchLvgl::Action::Save)
            fprintf(stderr,"Footer tap (%d,%d): %zu events\n",point.x,point.y,events.size());
        assert(events.size()==1&&events.back().action==WatchLvgl::Action::Save);
        events.clear();
    }
    for(const auto& point:std::vector<lv_point_t>{{76,406},{390,406},{211,465},{255,465},{230,405}}) {
        tap(ui,point.x,point.y); assert(events.empty());
    }
}
void capture(const M5Canvas& canvas,const char* path) {
    FILE* file=fopen(path,"wb"); assert(file); fprintf(file,"P6\n466 466\n255\n");
    for(int y=0;y<466;++y) for(int x=0;x<466;++x) {
        uint16_t c=canvas.pixels[y*466+x];
        int dx=x-233,dy=y-233; if(dx*dx+dy*dy>233*233) c=0;
        unsigned char rgb[]={(unsigned char)(((c>>11)&31)*255/31),(unsigned char)(((c>>5)&63)*255/63),(unsigned char)((c&31)*255/31)};
        fwrite(rgb,1,3,file);
    }
    fclose(file);
}
int main(int argc,char** argv) {
    WatchLvgl ui; M5Canvas canvas; assert(ui.begin(canvas,action));
    WatchLvgl::Model menu; menu.page=1; menu.count=6;
    const char* titles[]={"TIME","BOT","DISPLAY","SOUND","POWER","CORALLIUM"};
    for(int i=0;i<6;++i) snprintf(menu.rows[i].label,48,"%s",titles[i]);
    ui.show(menu); advance(ui,32);
    if(argc>1) capture(canvas,argv[1]);
    checkFooter(ui,canvas);
    tap(ui,200,120); assert(events.size()==1&&events.back().action==WatchLvgl::Action::Open&&events.back().row==0);
    events.clear();
    ui.pointer(true,200,300); advance(ui,16);
    for(int y=300;y>=130;y-=10) { ui.pointer(true,200,y); advance(ui,16); }
    ui.pointer(false,200,130); advance(ui,500);
    assert(events.empty()); // A scroll must never turn into a row activation.
    tap(ui,230,432); assert(events.size()==1&&events.back().action==WatchLvgl::Action::Save);
    WatchLvgl::Model editor; editor.page=2; editor.editor=true; editor.title="DISPLAY"; editor.count=2;
    strcpy(editor.rows[0].label,"BRIGHTNESS"); strcpy(editor.rows[0].value,"3");
    strcpy(editor.rows[1].label,"THEME"); strcpy(editor.rows[1].value,"Night");
    ui.show(editor); advance(ui,32); events.clear();
    if(argc>2) capture(canvas,argv[2]);
    checkFooter(ui,canvas);
    tap(ui,360,153); assert(events.size()==1&&events.back().action==WatchLvgl::Action::More&&events.back().row==0);
    tap(ui,292,153); assert(events.size()==2&&events.back().action==WatchLvgl::Action::Less&&events.back().row==0);
    tap(ui,233,378); assert(events.back().action==WatchLvgl::Action::Cancel);
    tap(ui,292,432); assert(events.back().action==WatchLvgl::Action::Save);
    events.clear(); ui.pointer(true,292,153); advance(ui,16); ui.resetPointer(); advance(ui,32); assert(events.empty());
    watchstrings::chinese()=true;
    WatchLvgl::Model color; color.page=3; color.editor=color.preview=color.color=true; color.title="BOT COLOR";
    ui.show(color); advance(ui,32);
    checkFooter(ui,canvas);
    const lv_font_t* font=lv_obj_get_style_text_font(lv_scr_act(),LV_PART_MAIN);
    for(uint32_t code:{0x76F8u,0x9971u,0x548Cu}) {
        lv_font_glyph_dsc_t glyph{}; assert(lv_font_get_glyph_dsc(font,&glyph,code,0));
        assert(glyph.adv_w==24&&glyph.bpp==4);
    }
    events.clear(); ui.pointer(true,82,267); advance(ui,32);
    ui.pointer(true,378,267); advance(ui,32); ui.pointer(false,378,267); advance(ui,32);
    assert(!events.empty()&&events.back().action==WatchLvgl::Action::Color&&events.back().value>=350);
    WatchLvgl::Model connection; connection.page=4; connection.editor=connection.immediate=true;
    connection.title="CONNECTION"; connection.count=3;
    strcpy(connection.rows[0].label,"BLUETOOTH"); strcpy(connection.rows[0].value,"ON (5 MIN)");
    connection.rows[0].kind=WatchLvgl::RowKind::Toggle; connection.rows[0].checked=true;
    strcpy(connection.rows[1].label,"NAME"); strcpy(connection.rows[1].value,"Night");
    connection.rows[1].kind=WatchLvgl::RowKind::Info; connection.rows[1].literal=true;
    strcpy(connection.rows[2].label,"PROTOCOL"); strcpy(connection.rows[2].value,"v1");
    connection.rows[2].kind=WatchLvgl::RowKind::Link;
    ui.show(connection); advance(ui,32); events.clear();
    checkFooter(ui,canvas);
    assert(hasLabel(lv_scr_act(),"连接")&&hasLabel(lv_scr_act(),"Night"));
    assert(!hasLabel(lv_scr_act(),"夜色")&&!hasLabel(lv_scr_act(),"取消"));
    tap(ui,344,151); assert(events.size()==1&&events.back().row==0&&events.back().action==WatchLvgl::Action::More);
    events.clear();
    tap(ui,360,260); assert(events.empty()); // Status information is not an editable setting.
    tap(ui,360,370); assert(events.size()==1&&events.back().row==2&&events.back().action==WatchLvgl::Action::More);
    tap(ui,230,432); assert(events.back().action==WatchLvgl::Action::Save);
    WatchLvgl::Model name; name.page=5; name.editor=name.name=true; name.title="BOT NAME";
    strcpy(name.botName,"Bot");
    ui.show(name); advance(ui,32); events.clear();
    if(argc>3) capture(canvas,argv[3]);
    checkFooter(ui,canvas);
    tap(ui,233,378); assert(events.size()==1&&events.back().action==WatchLvgl::Action::Cancel);
    watchstrings::chinese()=false;
    puts("PASS LVGL settings: circular footer raster/hits, native pointer, scroll cancellation, +/- rows, save/cancel, wake reset");
}

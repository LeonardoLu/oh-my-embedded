#include "WatchLvgl.h"
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
    tap(ui,200,120); assert(events.size()==1&&events.back().action==WatchLvgl::Action::Open&&events.back().row==0);
    events.clear();
    ui.pointer(true,200,300); advance(ui,16);
    for(int y=300;y>=130;y-=10) { ui.pointer(true,200,y); advance(ui,16); }
    ui.pointer(false,200,130); advance(ui,500);
    assert(events.empty()); // A scroll must never turn into a row activation.
    tap(ui,230,412); assert(events.size()==1&&events.back().action==WatchLvgl::Action::Save);
    WatchLvgl::Model editor; editor.page=2; editor.editor=true; editor.title="DISPLAY"; editor.count=2;
    strcpy(editor.rows[0].label,"BRIGHTNESS"); strcpy(editor.rows[0].value,"3");
    strcpy(editor.rows[1].label,"THEME"); strcpy(editor.rows[1].value,"Night");
    ui.show(editor); advance(ui,32); events.clear();
    tap(ui,360,153); assert(events.size()==1&&events.back().action==WatchLvgl::Action::More&&events.back().row==0);
    tap(ui,292,153); assert(events.size()==2&&events.back().action==WatchLvgl::Action::Less&&events.back().row==0);
    tap(ui,168,412); assert(events.back().action==WatchLvgl::Action::Cancel);
    tap(ui,292,412); assert(events.back().action==WatchLvgl::Action::Save);
    events.clear(); ui.pointer(true,292,153); advance(ui,16); ui.resetPointer(); advance(ui,32); assert(events.empty());
    watchstrings::chinese()=true;
    WatchLvgl::Model color; color.page=3; color.editor=color.preview=color.color=true; color.title="BOT COLOR";
    ui.show(color); advance(ui,32);
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
    assert(hasLabel(lv_scr_act(),"连接")&&hasLabel(lv_scr_act(),"Night"));
    assert(!hasLabel(lv_scr_act(),"夜色")&&!hasLabel(lv_scr_act(),"取消"));
    tap(ui,344,151); assert(events.size()==1&&events.back().row==0&&events.back().action==WatchLvgl::Action::More);
    events.clear();
    tap(ui,360,260); assert(events.empty()); // Status information is not an editable setting.
    tap(ui,360,370); assert(events.size()==1&&events.back().row==2&&events.back().action==WatchLvgl::Action::More);
    tap(ui,230,412); assert(events.back().action==WatchLvgl::Action::Save);
    watchstrings::chinese()=false;
    puts("PASS LVGL settings: native pointer, scroll cancellation, +/- rows, save/cancel, wake reset");
}

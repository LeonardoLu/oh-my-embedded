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
int focusedRow=-1;
void action(WatchLvgl::Action action,uint8_t row,int value,const char*) {
    if(action==WatchLvgl::Action::Focus) focusedRow=row;
    else events.push_back({action,row,value});
}
void advance(WatchLvgl& ui,int ms) { for(int i=0;i<ms;i+=8) { testMillis+=8; ui.update(testMillis,true); } }
void tap(WatchLvgl& ui,int x,int y) { ui.pointer(true,x,y); advance(ui,80); ui.pointer(false,x,y); advance(ui,80); }
bool hasLabel(lv_obj_t* parent,const char* text) {
    if(lv_obj_check_type(parent,&lv_label_class)&&!strcmp(lv_label_get_text(parent),text)) return true;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(parent);++i)
        if(hasLabel(lv_obj_get_child(parent,i),text)) return true;
    return false;
}
lv_obj_t* control(lv_obj_t* parent,const lv_obj_class_t* type) {
    if(lv_obj_check_type(parent,type)) return parent;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(parent);++i)
        if(auto* obj=control(lv_obj_get_child(parent,i),type)) return obj;
    return nullptr;
}
lv_obj_t* namedLabel(lv_obj_t* parent,const char* text) {
    if(lv_obj_check_type(parent,&lv_label_class)&&!strcmp(lv_label_get_text(parent),text)) return parent;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(parent);++i)
        if(auto* obj=namedLabel(lv_obj_get_child(parent,i),text)) return obj;
    return nullptr;
}
void choose(WatchLvgl& ui,lv_obj_t* dropdown,uint16_t index) {
    auto* list=lv_dropdown_get_list(dropdown);
    assert(lv_dropdown_is_open(dropdown));
    lv_area_t area; lv_obj_get_coords(list,&area);
    assert(area.x1>=60&&area.x2<=405&&area.y1>=104&&area.y2<=381);
    for(int y=area.y1;y<=area.y2;++y)
        assert(watchedge::displayContains(area.x1,y)&&watchedge::displayContains(area.x2,y));
    auto* text=lv_obj_get_child(list,0);
    const auto* font=lv_obj_get_style_text_font(text,LV_PART_MAIN);
    int line=font->line_height+lv_obj_get_style_text_line_space(text,LV_PART_MAIN);
    lv_obj_scroll_to_y(list,index*line,LV_ANIM_OFF); lv_obj_update_layout(list);
    lv_area_t copy; lv_obj_get_coords(text,&copy);
    tap(ui,(area.x1+area.x2)/2,copy.y1+index*line+font->line_height/2);
    assert(!lv_dropdown_is_open(dropdown));
}
lv_obj_t* keyboard(lv_obj_t* parent) {
    if(lv_obj_check_type(parent,&lv_keyboard_class)) return parent;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(parent);++i)
        if(auto* obj=keyboard(lv_obj_get_child(parent,i))) return obj;
    return nullptr;
}
void checkNoCancel() {
    assert(!hasLabel(lv_scr_act(),"CANCEL")&&!hasLabel(lv_scr_act(),"取消"));
    if(auto* obj=keyboard(lv_scr_act())) {
        const auto* map=lv_keyboard_get_map_array(obj);
        for(unsigned i=0;map[i][0];++i)
            assert(strcmp(map[i],LV_SYMBOL_KEYBOARD)&&strcmp(map[i],LV_SYMBOL_CLOSE));
        lv_area_t area; lv_obj_get_coords(obj,&area);
        assert(area.y2==381);
    }
}
void keyboardKey(WatchLvgl& ui,lv_obj_t* obj,const char* key) {
    auto* matrix=(lv_btnmatrix_t*)obj;
    for(uint16_t id=0;id<matrix->btn_cnt;++id) {
        if(strcmp(lv_btnmatrix_get_btn_text(obj,id),key)) continue;
        const auto& area=matrix->button_areas[id];
        tap(ui,obj->coords.x1+(area.x1+area.x2)/2,obj->coords.y1+(area.y1+area.y2)/2);
        return;
    }
    assert(false&&"Missing keyboard key");
}
void checkFooter(WatchLvgl& ui,const M5Canvas& canvas) {
    checkNoCancel();
    const auto background=lv_obj_get_style_bg_color(lv_scr_act(),LV_PART_MAIN).full;
    const auto bounds=watchcontrols::doneBounds();
    lv_obj_t* footer=nullptr;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(lv_scr_act());++i) {
        auto* obj=lv_obj_get_child(lv_scr_act(),i);
        if(lv_obj_has_flag(obj,LV_OBJ_FLAG_ADV_HITTEST)) footer=obj;
    }
    assert(footer);
    lv_area_t area; lv_obj_get_coords(footer,&area);
    assert(area.x1==0&&area.y1==406&&area.x2==465&&area.y2==465);
    for(int y=bounds.y;y<bounds.y+bounds.h;++y) for(int x=0;x<466;++x) {
        lv_point_t point={(lv_coord_t)x,(lv_coord_t)y};
        const bool inside=watchcontrols::doneContains(x,y);
        assert(lv_obj_hit_test(footer,&point)==inside);
        // Check the rendered curve independently of the label's glyph pixels.
        if(x<180||x>286||y<416||y>448) {
            if((canvas.pixels[y*466+x]!=background)!=inside)
                fprintf(stderr,"Footer raster (%d,%d): color %04x, inside %d\n",x,y,canvas.pixels[y*466+x],inside);
            assert((canvas.pixels[y*466+x]!=background)==inside);
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
    const char* titles[]={"TIME","BOT","DISPLAY","SOUND","POWER","CONNECTION"};
    for(int i=0;i<6;++i) snprintf(menu.rows[i].label,48,"%s",titles[i]);
    ui.show(menu); advance(ui,32);
    assert(lv_obj_get_style_bg_color(lv_scr_act(),LV_PART_MAIN).full==lv_color_hex(0x000000).full);
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
    editor.rows[0].kind=WatchLvgl::RowKind::Slider;
    editor.rows[0].number=3; editor.rows[0].minimum=1; editor.rows[0].maximum=5;
    strcpy(editor.rows[1].label,"THEME"); strcpy(editor.rows[1].value,"Night");
    editor.rows[1].maximum=2;
    editor.rows[1].optionLabel=[](uint8_t value) { const char* names[]={"Night","Dusk","Mono"}; return names[value]; };
    ui.show(editor); advance(ui,32); events.clear();
    if(argc>2) capture(canvas,argv[2]);
    checkFooter(ui,canvas);
    assert(!hasLabel(lv_scr_act(),"+")&&!hasLabel(lv_scr_act(),"-"));
    tap(ui,378,174); assert(events.size()==1&&events.back().action==WatchLvgl::Action::SetValue&&events.back().row==0&&events.back().value==5);
    tap(ui,78,174); assert(events.size()==2&&events.back().action==WatchLvgl::Action::SetValue&&events.back().value==1);
    auto* dropdown=control(lv_scr_act(),&lv_dropdown_class); assert(dropdown);
    events.clear(); focusedRow=-1; tap(ui,160,224);
    assert(lv_dropdown_is_open(dropdown)&&events.empty()&&focusedRow==1);
    if(argc>4) capture(canvas,argv[4]);
    choose(ui,dropdown,2); assert(events.size()==1&&events.back().action==WatchLvgl::Action::SetValue&&events.back().row==1&&events.back().value==2);
    events.clear();
    tap(ui,233,390); assert(events.empty()); // Removed footer control cannot cancel.
    tap(ui,292,432); assert(events.back().action==WatchLvgl::Action::Save);
    editor.error=true; ui.show(editor); advance(ui,32);
    auto* error=namedLabel(lv_scr_act(),"Error"); assert(error);
    auto* list=lv_obj_get_child(lv_scr_act(),1);
    lv_area_t errorArea,listArea; lv_obj_get_coords(error,&errorArea); lv_obj_get_coords(list,&listArea);
    assert(!lv_obj_has_flag(error,LV_OBJ_FLAG_HIDDEN)&&errorArea.y2<listArea.y1);
    events.clear(); ui.pointer(true,222,174); advance(ui,16); events.clear(); ui.resetPointer(); advance(ui,32); assert(events.empty());
    editor.rows[0].minimum=editor.rows[0].maximum=editor.rows[0].number=1;
    ui.show(editor); advance(ui,32);
    auto* slider=control(lv_scr_act(),&lv_slider_class); assert(lv_obj_has_state(slider,LV_STATE_DISABLED));
    events.clear(); tap(ui,300,174); assert(events.empty());
    editor.rows[0].maximum=5; ui.show(editor); advance(ui,32); assert(!lv_obj_has_state(slider,LV_STATE_DISABLED));
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
    connection.rows[0].number=1; connection.rows[0].maximum=1;
    strcpy(connection.rows[1].label,"NAME"); strcpy(connection.rows[1].value,"Night");
    connection.rows[1].kind=WatchLvgl::RowKind::Info; connection.rows[1].literal=true;
    strcpy(connection.rows[2].label,"PROTOCOL"); strcpy(connection.rows[2].value,"v1");
    connection.rows[2].kind=WatchLvgl::RowKind::Link;
    ui.show(connection); advance(ui,32); events.clear();
    checkFooter(ui,canvas);
    assert(hasLabel(lv_scr_act(),"连接")&&hasLabel(lv_scr_act(),"Night"));
    assert(!hasLabel(lv_scr_act(),"夜色")&&!hasLabel(lv_scr_act(),"取消"));
    tap(ui,344,142); assert(events.size()==1&&events.back().row==0&&events.back().action==WatchLvgl::Action::SetValue&&events.back().value==0);
    events.clear();
    tap(ui,140,140); assert(events.size()==1&&events.back().action==WatchLvgl::Action::SetValue&&events.back().value==1);
    events.clear();
    tap(ui,360,260); assert(events.empty()); // Status information is not an editable setting.
    tap(ui,360,370); assert(events.size()==1&&events.back().row==2&&events.back().action==WatchLvgl::Action::More);
    tap(ui,230,432); assert(events.back().action==WatchLvgl::Action::Save);
    WatchLvgl::Model name; name.page=5; name.editor=name.name=true; name.title="BOT NAME";
    strcpy(name.botName,"Bot");
    ui.show(name); advance(ui,32); events.clear();
    if(argc>3) capture(canvas,argv[3]);
    checkFooter(ui,canvas);
    auto* keys=keyboard(lv_scr_act()); assert(keys);
    keyboardKey(ui,keys,"q"); assert(!strcmp(ui.name(),"Botq"));
    keyboardKey(ui,keys,"ABC"); checkNoCancel();
    keyboardKey(ui,keys,"Q"); assert(!strcmp(ui.name(),"BotqQ"));
    keyboardKey(ui,keys,"1#"); checkNoCancel();
    keyboardKey(ui,keys,"9"); assert(!strcmp(ui.name(),"BotqQ9"));
    keyboardKey(ui,keys,"abc"); checkNoCancel();
    keyboardKey(ui,keys,LV_SYMBOL_OK); assert(events.back().action==WatchLvgl::Action::Save);
    watchstrings::chinese()=false;
    WatchLvgl::Model date; date.page=6; date.editor=true; date.title="SET DATE"; date.count=2;
    strcpy(date.rows[0].label,"DAY"); date.rows[0].number=31; date.rows[0].minimum=1; date.rows[0].maximum=31;
    strcpy(date.rows[1].label,"YEAR"); date.rows[1].number=2026; date.rows[1].minimum=2020; date.rows[1].maximum=2099;
    ui.show(date); advance(ui,32); events.clear();
    dropdown=control(lv_scr_act(),&lv_dropdown_class);
    date.rows[0].maximum=date.rows[0].number=28;
    ui.show(date); advance(ui,32);
    assert(lv_dropdown_get_option_cnt(dropdown)==28&&lv_dropdown_get_selected(dropdown)==27&&events.empty());
    auto* year=control(lv_obj_get_child(lv_obj_get_child(lv_scr_act(),1),1),&lv_dropdown_class);
    tap(ui,160,125); // Opens Day; selecting must return the actual 1-based value.
    choose(ui,dropdown,0); assert(events.size()==1&&events.back().value==1);
    events.clear(); tap(ui,160,250); assert(lv_dropdown_is_open(year));
    choose(ui,year,79); assert(events.size()==1&&events.back().row==1&&events.back().value==2099);
    WatchLvgl::Model scroll; scroll.page=7; scroll.editor=true; scroll.title="POWER SAVING"; scroll.count=5;
    for(auto& row:scroll.rows) { strcpy(row.label,"STATUS"); row.kind=WatchLvgl::RowKind::Info; }
    scroll.rows[1].kind=WatchLvgl::RowKind::Slider;
    scroll.rows[1].number=3; scroll.rows[1].minimum=1; scroll.rows[1].maximum=5;
    ui.show(scroll); advance(ui,32); events.clear();
    ui.pointer(true,224,330); advance(ui,16);
    for(int y=330;y>=130;y-=10) { ui.pointer(true,224,y); advance(ui,16); }
    ui.pointer(false,224,130); advance(ui,300);
    assert(events.empty()); // Scrolling over the slider cannot change it.
    puts("PASS LVGL settings: circular footer, absolute sliders/choices, whole-row toggles, range refresh, scroll exclusion, save, keyboard modes, wake reset");
}

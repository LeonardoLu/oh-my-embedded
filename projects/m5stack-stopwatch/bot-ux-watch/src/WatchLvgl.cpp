#include "WatchLvgl.h"
#include "WatchControls.h"
#include "WatchStrings.h"
#include <UxText.h>
#include <cstring>

extern const ux::Font WatchExtra24;
namespace {
lv_font_t bodyFont;
constexpr uint32_t background=0x000000,card=0x181819,ink=0xFCFCFF;
constexpr uint32_t muted=0x91919B,divider=0x3B3C3D,accent=0xFF4C01;
// Names accept only ASCII letters, digits, spaces, '-' and '_'. The native
// mode/cursor/confirm keys remain, with no keyboard close/cancel key.
const char* nameLower[]={
    "1#","q","w","e","r","t","y","u","i","o","p",LV_SYMBOL_BACKSPACE,"\n",
    "ABC","a","s","d","f","g","h","j","k","l","\n",
    "_","-","z","x","c","v","b","n","m","\n",
    LV_SYMBOL_LEFT," ",LV_SYMBOL_RIGHT,LV_SYMBOL_OK,""
};
const char* nameUpper[]={
    "1#","Q","W","E","R","T","Y","U","I","O","P",LV_SYMBOL_BACKSPACE,"\n",
    "abc","A","S","D","F","G","H","J","K","L","\n",
    "_","-","Z","X","C","V","B","N","M","\n",
    LV_SYMBOL_LEFT," ",LV_SYMBOL_RIGHT,LV_SYMBOL_OK,""
};
const char* nameNumbers[]={
    "1","2","3","4","5","6","7","8","9","0",LV_SYMBOL_BACKSPACE,"\n",
    "abc","-","_","\n",LV_SYMBOL_LEFT," ",LV_SYMBOL_RIGHT,LV_SYMBOL_OK,""
};
constexpr lv_btnmatrix_ctrl_t functionKey=LV_KEYBOARD_CTRL_BTN_FLAGS;
const lv_btnmatrix_ctrl_t nameLetterControls[]={
    functionKey|5,4,4,4,4,4,4,4,4,4,4,functionKey|7,
    functionKey|6,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,
    functionKey|2,6,functionKey|2,functionKey|2
};
const lv_btnmatrix_ctrl_t nameNumberControls[]={
    1,1,1,1,1,1,1,1,1,1,functionKey|2,
    functionKey|2,1,1,functionKey|2,6,functionKey|2,functionKey|2
};
const ux::Font& fontFor(uint32_t codepoint) {
    if(codepoint<128) return ux::Latin24;
    const auto* glyph=ux::glyph(ux::Cjk24,codepoint);
    return glyph&&glyph->code==codepoint?ux::Cjk24:WatchExtra24;
}
bool glyphDescription(const lv_font_t*, lv_font_glyph_dsc_t* out, uint32_t codepoint, uint32_t) {
    const auto& font=fontFor(codepoint);
    const auto* glyph=ux::glyph(font,codepoint);
    if(!glyph) return false;
    out->adv_w=glyph->advance;
    out->box_w=glyph->w; out->box_h=glyph->h;
    out->ofs_x=glyph->x;
    out->ofs_y=font.lineHeight-glyph->y-glyph->h;
    out->bpp=4;
    return true;
}
const uint8_t* glyphBitmap(const lv_font_t*, uint32_t codepoint) {
    const auto& font=fontFor(codepoint);
    const auto* glyph=ux::glyph(font,codepoint);
    return glyph ? font.coverage+glyph->offset : nullptr;
}
const char* localized(const char* text) {
    if(watchstrings::chinese()) {
        if(!strcmp(text,"HUE")) return "色相";
        if(!strcmp(text,"SATURATION")) return "饱和度";
        static const watchstrings::Entry connectionCopy[]={
            {"WINDOW","连接窗口"},{"PROTOCOL","协议"},{"SUPPORTED","支持"},{"ENCRYPTED","加密"}
        };
        for(const auto& entry:connectionCopy) if(!strcmp(text,entry.en)) return entry.zh;
    }
    return watchstrings::translate(text);
}
void clean(lv_obj_t* obj) {
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
}
void separator(lv_obj_t* parent,int y,int width) {
    auto* obj=lv_obj_create(parent); clean(obj);
    lv_obj_clear_flag(obj,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj,0,y); lv_obj_set_size(obj,width,1);
    lv_obj_set_style_bg_color(obj,lv_color_hex(divider),0);
    lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
}
lv_obj_t* label(lv_obj_t* parent,const char* text,int x,int y,int width,bool literal=false) {
    auto* obj=lv_label_create(parent);
    lv_label_set_text(obj,literal?text:localized(text));
    lv_label_set_long_mode(obj,LV_LABEL_LONG_DOT);
    lv_obj_set_pos(obj,x,y); lv_obj_set_width(obj,width);
    return obj;
}
void footerEvent(lv_event_t* e) {
    if(lv_event_get_code(e)==LV_EVENT_HIT_TEST) {
        auto* hit=lv_event_get_hit_test_info(e);
        hit->res=watchcontrols::doneContains(hit->point->x,hit->point->y);
    } else if(lv_event_get_code(e)==LV_EVENT_DRAW_MAIN) {
        auto* obj=lv_event_get_target(e);
        auto* context=lv_event_get_draw_ctx(e);
        lv_draw_rect_dsc_t style;
        lv_draw_rect_dsc_init(&style);
        style.bg_color=lv_obj_get_style_bg_color(obj,LV_PART_MAIN);
        const auto bounds=watchcontrols::doneBounds();
        // Use the same circular scanlines for visible pixels and LVGL hit testing.
        for(int16_t y=bounds.y;y<bounds.y+bounds.h;++y) {
            if(y<context->clip_area->y1||y>context->clip_area->y2) continue;
            const auto span=watchcontrols::doneRowSpan(y);
            lv_area_t area={(lv_coord_t)span.x,y,(lv_coord_t)(span.x+span.w-1),y};
            lv_draw_rect(context,&style,&area);
        }
    }
}
}

bool WatchLvgl::begin(M5Canvas& capture, Handler handler) {
    _capture=&capture; _handler=handler;
    _pixels=(lv_color_t*)heap_caps_malloc(466*24*sizeof(lv_color_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    _preview=(lv_color_t*)heap_caps_malloc(178*178*sizeof(lv_color_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!_pixels||!_preview) return false;
    memset(_preview,0,178*178*sizeof(lv_color_t));
    lv_init();
    bodyFont.get_glyph_dsc=glyphDescription;
    bodyFont.get_glyph_bitmap=glyphBitmap;
    bodyFont.line_height=ux::Latin24.lineHeight;
    bodyFont.base_line=0;
    lv_disp_draw_buf_init(&_drawBuffer,_pixels,nullptr,466*24);
    lv_disp_drv_init(&_displayDriver);
    _displayDriver.hor_res=_displayDriver.ver_res=466;
    _displayDriver.draw_buf=&_drawBuffer;
    _displayDriver.flush_cb=flush; _displayDriver.user_data=this;
    _display=lv_disp_drv_register(&_displayDriver);
    lv_indev_drv_init(&_inputDriver);
    _inputDriver.type=LV_INDEV_TYPE_POINTER;
    _inputDriver.read_cb=read; _inputDriver.user_data=this;
    _input=lv_indev_drv_register(&_inputDriver);
    _root=lv_scr_act();
    _imageDescriptor.header.cf=LV_IMG_CF_TRUE_COLOR;
    _imageDescriptor.header.w=_imageDescriptor.header.h=178;
    _imageDescriptor.data_size=178*178*sizeof(lv_color_t);
    _imageDescriptor.data=(const uint8_t*)_preview;
    _lastTick=millis();
    return _display&&_input;
}

void WatchLvgl::flush(lv_disp_drv_t* driver,const lv_area_t* area,lv_color_t* pixels) {
    auto* self=(WatchLvgl*)driver->user_data;
    if(self->_active) {
        int w=area->x2-area->x1+1,h=area->y2-area->y1+1;
        // Explicit RGB565 type avoids depending on M5GFX's byte-swap setting.
        auto* rgb=(const lgfx::rgb565_t*)pixels;
        self->_capture->pushImage(area->x1,area->y1,w,h,rgb);
        M5.Display.pushImage(area->x1,area->y1,w,h,rgb);
    }
    lv_disp_flush_ready(driver);
}
void WatchLvgl::read(lv_indev_drv_t* driver,lv_indev_data_t* data) {
    auto* self=(WatchLvgl*)driver->user_data;
    data->point.x=self->_x; data->point.y=self->_y;
    data->state=self->_active&&self->_pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
}
void WatchLvgl::pointer(bool down,int16_t x,int16_t y) { _pressed=down; _x=x; _y=y; }
void WatchLvgl::resetPointer() {
    _pressed=false;
    if(_input) lv_indev_reset(_input,nullptr);
}
void WatchLvgl::bind(lv_obj_t* obj,Action action,uint8_t row) {
    auto& binding=_bindings[_bindingCount++]; binding={this,action,row};
    lv_obj_add_event_cb(obj,event,LV_EVENT_ALL,&binding);
}
void WatchLvgl::event(lv_event_t* e) {
    auto* binding=(Binding*)lv_event_get_user_data(e);
    auto code=lv_event_get_code(e);
    if(binding->action==Action::Color && code==LV_EVENT_VALUE_CHANGED) {
        binding->owner->_handler(binding->action,binding->row,lv_slider_get_value(lv_event_get_target(e)),nullptr);
    } else if(binding->action==Action::Name && code==LV_EVENT_VALUE_CHANGED) {
        binding->owner->_handler(binding->action,0,0,lv_textarea_get_text(lv_event_get_target(e)));
    } else if(binding->action!=Action::Name && binding->action!=Action::Color && code==LV_EVENT_SHORT_CLICKED) {
        binding->owner->_handler(binding->action,binding->row,0,nullptr);
    }
}
lv_obj_t* WatchLvgl::button(lv_obj_t* parent,const char* text,int x,int y,int w,int h,Action action,uint8_t row) {
    auto* obj=lv_btn_create(parent); clean(obj);
    lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,w,h);
    lv_obj_set_style_radius(obj,14,0);
    lv_obj_set_style_bg_color(obj,lv_color_hex(card),0);
    lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(obj,lv_color_hex(divider),LV_STATE_PRESSED);
    lv_obj_set_style_border_width(obj,1,0);
    lv_obj_set_style_border_color(obj,lv_color_hex(divider),0);
    auto* copy=lv_label_create(obj); lv_label_set_text(copy,localized(text)); lv_obj_center(copy);
    bind(obj,action,row);
    return obj;
}
void WatchLvgl::build(const Model& model) {
    resetPointer(); lv_obj_clean(_root); clean(_root);
    _bindingCount=0; _list=_image=_name=_error=nullptr;
    memset(_rows,0,sizeof(_rows)); memset(_values,0,sizeof(_values)); memset(_switches,0,sizeof(_switches)); memset(_sliders,0,sizeof(_sliders));
    _selected=UINT8_MAX;
    lv_obj_set_style_bg_color(_root,lv_color_hex(background),0);
    lv_obj_set_style_bg_opa(_root,LV_OPA_COVER,0);
    lv_obj_set_style_text_font(_root,&bodyFont,0);
    lv_obj_set_style_text_color(_root,lv_color_hex(ink),0);
    auto* title=label(_root,model.title,100,40,266);
    lv_obj_set_style_text_align(title,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_color(title,lv_color_hex(muted),0);
    if(model.preview) {
        _image=lv_img_create(_root); lv_img_set_src(_image,&_imageDescriptor);
        lv_img_set_zoom(_image,184); lv_obj_set_pos(_image,144,65);
    }
    const int contentTop=model.preview?218:model.editor?104:88,contentBottom=382;
    _list=lv_obj_create(_root); clean(_list);
    lv_obj_set_pos(_list,60,contentTop);
    lv_obj_set_size(_list,346,contentBottom-contentTop);
    lv_obj_add_flag(_list,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(_list,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(_list,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_bottom(_list,8,0);
    lv_obj_set_style_bg_color(_list,lv_color_hex(divider),LV_PART_SCROLLBAR);
    lv_obj_set_style_width(_list,3,LV_PART_SCROLLBAR);
    if(model.name) {
        _name=lv_textarea_create(_root); lv_obj_set_pos(_name,90,91); lv_obj_set_size(_name,286,52);
        lv_textarea_set_one_line(_name,true); lv_textarea_set_max_length(_name,16);
        lv_textarea_set_text(_name,model.botName);
        lv_textarea_set_accepted_chars(_name,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_");
        lv_obj_set_style_bg_color(_name,lv_color_hex(card),0);
        lv_obj_set_style_text_color(_name,lv_color_hex(ink),0);
        lv_obj_set_style_border_color(_name,lv_color_hex(divider),0);
        lv_obj_set_style_border_color(_name,lv_color_hex(accent),LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(_name,lv_color_hex(accent),LV_PART_CURSOR|LV_STATE_FOCUSED);
        lv_obj_set_style_radius(_name,14,0);
        bind(_name,Action::Name,0);
        auto* keyboard=lv_keyboard_create(_root);
        lv_obj_set_align(keyboard,LV_ALIGN_TOP_LEFT);
        lv_obj_set_pos(keyboard,65,157); lv_obj_set_size(keyboard,336,contentBottom-157);
        lv_keyboard_set_map(keyboard,LV_KEYBOARD_MODE_TEXT_LOWER,nameLower,nameLetterControls);
        lv_keyboard_set_map(keyboard,LV_KEYBOARD_MODE_TEXT_UPPER,nameUpper,nameLetterControls);
        lv_keyboard_set_map(keyboard,LV_KEYBOARD_MODE_SPECIAL,nameNumbers,nameNumberControls);
        lv_keyboard_set_textarea(keyboard,_name);
        lv_obj_add_event_cb(keyboard,[](lv_event_t* e) {
            auto* self=(WatchLvgl*)lv_event_get_user_data(e);
            self->_handler(Action::Save,0,0,nullptr);
        },LV_EVENT_READY,this);
        lv_obj_set_style_bg_color(keyboard,lv_color_hex(background),0);
        lv_obj_set_style_border_width(keyboard,0,0);
        lv_obj_set_style_pad_all(keyboard,4,0);
        lv_obj_set_style_text_font(keyboard,&lv_font_montserrat_14,LV_PART_ITEMS);
        lv_obj_set_style_text_color(keyboard,lv_color_hex(ink),LV_PART_ITEMS);
        lv_obj_set_style_text_color(keyboard,lv_color_hex(ink),LV_PART_ITEMS|LV_STATE_CHECKED);
        lv_obj_set_style_text_color(keyboard,lv_color_hex(ink),LV_PART_ITEMS|LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(keyboard,lv_color_hex(card),LV_PART_ITEMS);
        lv_obj_set_style_bg_color(keyboard,lv_color_hex(card),LV_PART_ITEMS|LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(keyboard,lv_color_hex(accent),LV_PART_ITEMS|LV_STATE_PRESSED);
        lv_obj_set_style_border_color(keyboard,lv_color_hex(divider),LV_PART_ITEMS);
        lv_obj_set_style_radius(keyboard,10,LV_PART_ITEMS);
    } else if(model.color) {
        const char* names[]={"HUE","SATURATION","BRIGHTNESS"};
        for(uint8_t i=0;i<3;++i) {
            label(_list,names[i],12,i*78,320);
            _sliders[i]=lv_slider_create(_list);
            lv_obj_set_pos(_sliders[i],22,40+i*78); lv_obj_set_size(_sliders[i],296,18);
            lv_slider_set_range(_sliders[i],0,i?100:359);
            lv_obj_set_style_bg_color(_sliders[i],lv_color_hex(divider),LV_PART_MAIN);
            lv_obj_set_style_bg_opa(_sliders[i],LV_OPA_COVER,LV_PART_MAIN);
            lv_obj_set_style_bg_color(_sliders[i],lv_color_hex(accent),LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(_sliders[i],lv_color_hex(ink),LV_PART_KNOB);
            bind(_sliders[i],Action::Color,i);
        }
        button(_list,"USE THEME",65,242,216,48,Action::Theme);
    } else {
        for(uint8_t i=0;i<model.count;++i) {
            const int height=model.editor?98:72;
            auto* row=lv_obj_create(_list); clean(row); _rows[i]=row;
            lv_obj_set_pos(row,0,i*height); lv_obj_set_size(row,336,height);
            lv_obj_set_style_bg_color(row,lv_color_hex(background),0);
            lv_obj_set_style_bg_opa(row,LV_OPA_COVER,0);
            lv_obj_set_style_bg_color(row,lv_color_hex(card),LV_STATE_PRESSED);
            separator(row,height-1,336);
            if(!model.editor) { lv_obj_add_flag(row,LV_OBJ_FLAG_CLICKABLE); bind(row,Action::Open,i); }
            label(row,model.rows[i].label,12,model.editor?9:20,model.editor?312:278);
            if(!model.editor) {
                auto* arrow=label(row,">",306,20,20);
                lv_obj_set_style_text_color(arrow,lv_color_hex(muted),0);
            }
            if(model.editor) {
                bool info=model.rows[i].kind==RowKind::Info;
                _values[i]=label(row,model.rows[i].value,17,49,info?302:191,model.rows[i].literal);
                lv_obj_set_style_text_color(_values[i],lv_color_hex(muted),0);
                if(model.rows[i].kind==RowKind::Toggle) {
                    auto* control=lv_switch_create(row); _switches[i]=control;
                    lv_obj_set_pos(control,248,45); lv_obj_set_size(control,74,40);
                    lv_obj_set_style_bg_color(control,lv_color_hex(divider),LV_PART_MAIN);
                    lv_obj_set_style_bg_color(control,lv_color_hex(accent),LV_PART_INDICATOR|LV_STATE_CHECKED);
                    lv_obj_set_style_bg_color(control,lv_color_hex(ink),LV_PART_KNOB);
                    auto& binding=_bindings[_bindingCount++]; binding={this,Action::More,i};
                    lv_obj_add_event_cb(control,[](lv_event_t* e) {
                        auto* binding=(Binding*)lv_event_get_user_data(e);
                        binding->owner->_handler(Action::More,binding->row,0,nullptr);
                    },LV_EVENT_VALUE_CHANGED,&binding);
                } else if(!info) {
                    if(model.rows[i].kind==RowKind::Setting) button(row,"-",218,42,50,48,Action::Less,i);
                    button(row,model.rows[i].kind==RowKind::Link?">":"+",276,42,50,48,Action::More,i);
                }
            }
        }
    }
    const auto bounds=watchcontrols::doneBounds();
    auto* footer=button(_root,"DONE",bounds.x,bounds.y,bounds.w,bounds.h,Action::Save);
    lv_obj_set_style_radius(footer,0,0);
    lv_obj_set_style_border_width(footer,0,0);
    lv_obj_set_style_bg_opa(footer,LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_color(footer,lv_color_hex(accent),0);
    lv_obj_set_style_bg_color(footer,lv_color_hex(0xFF732E),LV_STATE_PRESSED);
    lv_obj_set_style_text_color(footer,lv_color_hex(background),0);
    lv_obj_add_flag(footer,LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_event_cb(footer,footerEvent,LV_EVENT_ALL,nullptr);
    lv_obj_align(lv_obj_get_child(footer,0),LV_ALIGN_CENTER,0,
        watchcontrols::doneLabelY()-(bounds.y+bounds.h/2));
    _error=label(_root,"Error",168,70,130);
    lv_obj_set_style_text_color(_error,lv_color_hex(0xFF8A80),0);
    lv_obj_set_style_text_align(_error,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_add_flag(_error,LV_OBJ_FLAG_HIDDEN);
}
void WatchLvgl::show(const Model& model) {
    if(_page!=model.page) { _page=model.page; build(model); }
    for(uint8_t i=0;i<model.count;++i) {
        const char* value=model.rows[i].literal?model.rows[i].value:localized(model.rows[i].value);
        if(_values[i] && strcmp(lv_label_get_text(_values[i]),value)) lv_label_set_text(_values[i],value);
        if(_switches[i]) {
            if(model.rows[i].checked) lv_obj_add_state(_switches[i],LV_STATE_CHECKED);
            else lv_obj_clear_state(_switches[i],LV_STATE_CHECKED);
        }
        if(_rows[i]) {
            lv_obj_set_style_border_width(_rows[i],model.keyboardNavigation&&model.selected==i?2:0,0);
            lv_obj_set_style_border_color(_rows[i],lv_color_hex(accent),0);
        }
    }
    if(model.keyboardNavigation&&_selected!=model.selected&&model.selected<model.count&&_rows[model.selected])
        lv_obj_scroll_to_view(_rows[model.selected],LV_ANIM_ON);
    _selected=model.selected;
    if(_sliders[0]) {
        lv_slider_set_value(_sliders[0],model.hue,LV_ANIM_OFF);
        lv_slider_set_value(_sliders[1],model.saturation,LV_ANIM_OFF);
        lv_slider_set_value(_sliders[2],model.brightness,LV_ANIM_OFF);
    }
    if(model.error) lv_obj_clear_flag(_error,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(_error,LV_OBJ_FLAG_HIDDEN);
}
void WatchLvgl::update(uint32_t now,bool active) {
    lv_tick_inc(now-_lastTick); _lastTick=now;
    if(active&&!_active) lv_obj_invalidate(_root);
    _active=active;
    if(active) lv_timer_handler();
    else resetPointer();
}
void WatchLvgl::preview(M5Canvas& sprite) {
    if(!_image) return;
    const auto* source=(const lgfx::swap565_t*)sprite.getBuffer();
    for(unsigned i=0;i<178*178;++i) _preview[i].full=__builtin_bswap16(source[i].raw);
    lv_obj_invalidate(_image);
}
const char* WatchLvgl::name() const { return _name?lv_textarea_get_text(_name):""; }

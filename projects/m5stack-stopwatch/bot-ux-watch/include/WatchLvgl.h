#pragma once
#include <M5Unified.h>
#include <lvgl.h>

// LVGL owns every settings hit target and scroll gesture. The app owns state,
// persistence and RTC validation; callbacks only express user intent.
class WatchLvgl {
public:
    enum class Action : uint8_t { Open, Less, More, Save, Cancel, Name, Color, Theme, SetValue, Focus };
    using Handler = void (*)(Action, uint8_t, int, const char*);
    enum class RowKind : uint8_t { Choice, Info, Link, Toggle, Slider };
    struct Row {
        char label[48] = {}; char value[64] = {};
        RowKind kind = RowKind::Choice;
        bool literal = false, checked = false;
        int16_t number = 0, minimum = 0, maximum = 0;
        const char* (*optionLabel)(uint8_t) = nullptr;
    };
    struct Model {
        uint16_t page = 0;
        const char* title = "SETTINGS";
        Row rows[11];
        uint8_t count = 0, selected = 0;
        bool editor = false, preview = false, color = false, name = false, immediate = false;
        bool keyboardNavigation = false, error = false;
        uint16_t hue = 0;
        uint8_t saturation = 0, brightness = 0;
        char botName[17] = {};
    };
    bool begin(M5Canvas& capture, Handler handler);
    void show(const Model& model);
    void pointer(bool down, int16_t x, int16_t y);
    void resetPointer();
    void update(uint32_t now, bool active);
    void preview(M5Canvas& sprite);
    const char* name() const;
private:
    static void flush(lv_disp_drv_t*, const lv_area_t*, lv_color_t*);
    static void read(lv_indev_drv_t*, lv_indev_data_t*);
    static void event(lv_event_t*);
    void build(const Model&);
    void positionChoice(uint8_t);
    void updateChoice(uint8_t, const Row&);
    lv_obj_t* button(lv_obj_t*, const char*, int, int, int, int, Action, uint8_t = 0);
    void bind(lv_obj_t*, Action, uint8_t);
    struct Binding { WatchLvgl* owner; Action action; uint8_t row; };
    Binding _bindings[48];
    uint8_t _bindingCount = 0;
    Handler _handler = nullptr;
    M5Canvas* _capture = nullptr;
    lv_disp_draw_buf_t _drawBuffer{};
    lv_disp_drv_t _displayDriver{};
    lv_indev_drv_t _inputDriver{};
    lv_disp_t* _display = nullptr;
    lv_indev_t* _input = nullptr;
    lv_obj_t* _root = nullptr;
    lv_obj_t* _list = nullptr;
    lv_obj_t* _rows[11] = {};
    lv_obj_t* _values[11] = {};
    lv_obj_t* _switches[11] = {};
    lv_obj_t* _choices[11] = {};
    lv_obj_t* _rowSliders[11] = {};
    int16_t _minimum[11] = {}, _maximum[11] = {};
    lv_obj_t* _image = nullptr;
    lv_obj_t* _name = nullptr;
    lv_obj_t* _sliders[3] = {};
    lv_obj_t* _error = nullptr;
    lv_img_dsc_t _imageDescriptor{};
    lv_color_t* _pixels = nullptr;
    lv_color_t* _preview = nullptr;
    bool _pressed = false, _active = false;
    int16_t _x = 0, _y = 0;
    uint16_t _page = UINT16_MAX;
    uint32_t _lastTick = 0;
    uint8_t _selected = UINT8_MAX;
};

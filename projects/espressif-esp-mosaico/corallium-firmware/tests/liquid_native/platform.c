/* Exercise real LIQUID app logic, Canvas ownership and native scene rendering. */
#include "gsp_sim_bridge.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint16_t *pixels;
    uint32_t hash;
    esp_gsp_canvas_release_cb_t release;
    void *ctx;
} borrowed_t;

static borrowed_t s_borrowed[2];
static unsigned s_borrowed_count;
static bool s_probe;
static bool s_reject;

static uint32_t frame_hash(const uint16_t *pixels)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < 456 * 320; ++i) hash = (hash ^ pixels[i]) * 16777619u;
    return hash;
}

static esp_gsp_err_t push(esp_gsp_handle_t ui, uint16_t bind, const void *pixels,
    size_t stride, esp_gsp_canvas_release_cb_t release, void *ctx)
{
    if (!s_probe) return esp_gsp_canvas_try_push(ui, bind, pixels, stride, release, ctx);
    if (s_reject) return ESP_GSP_ERR_TIMEOUT;
    assert(s_borrowed_count < 2);
    assert(stride == 456 * sizeof(uint16_t));
    for (unsigned i = 0; i < s_borrowed_count; ++i) assert(s_borrowed[i].pixels != pixels);
    const uint16_t *frame = pixels;
    s_borrowed[s_borrowed_count++] = (borrowed_t){frame, frame_hash(frame), release, ctx};
    return ESP_GSP_OK;
}

#define esp_gsp_canvas_try_push push
#include LIQUID_APP_SOURCE
#undef esp_gsp_canvas_try_push

static const mosaic_app_descriptor_t s_hub = {
    .id = MOSAIC_APP_ROOT_ID, .name = "hub",
    .launch_action = MOSAIC_APP_NO_LAUNCH_ACTION,
};
const mosaic_app_descriptor_t *const mosaic_app_registry[] = {&s_hub, &mosaic_liquid_app};
const size_t mosaic_app_registry_count = 2;
const mosaic_app_package_t mosaic_app_packages[] = {{0}};
const size_t mosaic_app_package_count = 0;

esp_err_t mosaic_imu_get_gravity(float *gx, float *gy)
{
    *gx = 0.75f;
    *gy = 0.65f;
    return ESP_OK;
}

static void action(esp_gsp_handle_t ui, uint16_t id)
{
    const mosaic_event_t event = {.type = MOSAIC_EVENT_UI_CALL,
                                  .data.call.action_id = id};
    mosaic_liquid_app.on_event(ui, &event);
}

static void tick(esp_gsp_handle_t ui, void *ctx)
{
    (void)ctx;
    const mosaic_event_t event = {.type = MOSAIC_EVENT_TIMER,
        .timestamp_us = (int64_t)gsp_sim_bridge_time_ms() * 1000};
    mosaic_liquid_app.on_event(ui, &event);
}

static void call(esp_gsp_handle_t ui, const esp_gsp_event_t *event, void *ctx)
{
    (void)ctx;
    if (event->type == ESP_GSP_EVENT_CALL) {
        const mosaic_app_descriptor_t *target = NULL;
        if (event->action_id == GSP_ACT_ID_LIQUID_RETURN) {
            assert(mosaic_app_route_event(&mosaic_liquid_app, event, &target) && target == &s_hub);
            puts("BACK_ROUTE");
            return;
        }
        assert(!mosaic_app_route_event(&mosaic_liquid_app, event, &target));
        action(ui, event->action_id);
    }
}

esp_gsp_err_t gsp_bridge_app_init(esp_gsp_handle_t ui)
{
    const esp_gsp_event_t back = {.type = ESP_GSP_EVENT_CALL,
                                  .action_id = GSP_ACT_ID_LIQUID_RETURN};
    const mosaic_app_descriptor_t *target = NULL;
    assert(mosaic_app_route_event(&mosaic_liquid_app, &back, &target) && target == &s_hub);
    const uint16_t controls[] = {GSP_ACT_ID_LIQUID_THEME, GSP_ACT_ID_LIQUID_RESET};
    for (unsigned i = 0; i < 2; ++i) {
        const esp_gsp_event_t control = {.type = ESP_GSP_EVENT_CALL, .action_id = controls[i]};
        assert(!mosaic_app_route_event(&mosaic_liquid_app, &control, &target));
    }
    assert(!mosaic_liquid_select_style((liquid_engine_style_t)99));
    assert(mosaic_liquid_select_style(LIQUID_PIXEL));
    s_probe = true;
    s_reject = true;
    mosaic_liquid_app.on_started(ui);
    assert(s_frames != NULL && atomic_load(&s_frames->references) == 1);
    assert(!atomic_load(&s_frames->frames[0].busy));
    s_reject = false;
    tick(ui, NULL);
    tick(ui, NULL);
    assert(s_borrowed_count == 2);
    action(ui, GSP_ACT_ID_LIQUID_THEME);
    tick(ui, NULL); /* Neither borrowed frame may be overwritten. */
    assert(s_borrowed_count == 2);
    for (unsigned i = 0; i < 2; ++i) assert(frame_hash(s_borrowed[i].pixels) == s_borrowed[i].hash);
    mosaic_liquid_app.on_stopping(ui);
    assert(s_frames == NULL && s_liquid == NULL);
    /* Late callbacks retain their own session, after app teardown. */
    for (unsigned i = 0; i < 2; ++i) s_borrowed[i].release(s_borrowed[i].ctx);
    s_borrowed_count = 0;
    s_probe = false;
    const char *value = getenv("LIQUID_NATIVE_STYLE");
    int style = value != NULL ? atoi(value) : 0;
    assert(style >= 0 && style <= 2);
    assert(mosaic_liquid_select_style((liquid_engine_style_t)style));
    mosaic_liquid_app.on_started(ui);
    value = getenv("LIQUID_NATIVE_PALETTE");
    const unsigned palette = value != NULL ? (unsigned)atoi(value) : 5;
    assert(palette < LIQUID_PALETTES);
    for (unsigned i = 0; i < (palette + 8 - 5) % 8; ++i) action(ui, GSP_ACT_ID_LIQUID_THEME);
    (void)esp_gsp_timer_create(ui, 33, tick, NULL);
    return esp_gsp_on_event(ui, call, NULL);
}

void gsp_bridge_app_deinit(esp_gsp_handle_t ui)
{
    (void)esp_gsp_on_event(ui, NULL, NULL);
    mosaic_liquid_app.on_stopping(ui);
}

/* Real Works C and input routing with explicit offline catalog doubles. */
#include "gsp_sim_bridge.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char s_names[4][64];
static bool s_native_visible[3], s_lua_visible[4];
static int s_selection = -1;

static esp_gsp_err_t set_text(esp_gsp_handle_t ui, uint16_t bind, const char *text);
static esp_gsp_err_t set_visible(esp_gsp_handle_t ui, uint16_t bind, bool visible);
#define esp_gsp_set_text set_text
#define esp_gsp_set_visible set_visible
#define ESP_PLATFORM
#include WORKS_APP_SOURCE
#undef ESP_PLATFORM
#undef esp_gsp_set_text
#undef esp_gsp_set_visible

static const mosaic_app_descriptor_t s_hub = {
    .id = 0, .name = "hub", .launch_action = MOSAIC_APP_NO_LAUNCH_ACTION,
};
static const mosaic_app_descriptor_t s_liquid = {
    .id = 14, .name = "liquid", .launch_action = MOSAIC_APP_NO_LAUNCH_ACTION,
};
const mosaic_app_descriptor_t *const mosaic_app_registry[] = {&s_hub, &s_liquid, &mosaic_works_app};
const size_t mosaic_app_registry_count = 3;
const mosaic_app_package_t mosaic_app_packages[] = {{0}};
const size_t mosaic_app_package_count = 0;

static esp_gsp_err_t set_text(esp_gsp_handle_t ui, uint16_t bind, const char *text)
{
    for (size_t i = 0; i < 4; ++i) {
        if (bind == s_row_name_binds[i]) snprintf(s_names[i], sizeof(s_names[i]), "%s", text);
    }
    return esp_gsp_set_text(ui, bind, text);
}
static esp_gsp_err_t set_visible(esp_gsp_handle_t ui, uint16_t bind, bool visible)
{
    for (size_t i = 0; i < 3; ++i) if (bind == s_native_visible_binds[i]) s_native_visible[i] = visible;
    for (size_t i = 0; i < 4; ++i) if (bind == s_row_lua_visible_binds[i]) s_lua_visible[i] = visible;
    return esp_gsp_set_visible(ui, bind, visible);
}
/* The host bridge exposes individual setters, but omits the batch wrapper. */
esp_gsp_err_t esp_gsp_component_set_many(esp_gsp_handle_t ui,
    const gsp_component_update_t *updates, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        assert(updates[i].prop == GSP_COMPONENT_PROP_VISIBLE);
        assert(updates[i].value.type == GSP_VALUE_BOOL);
        esp_gsp_err_t err = esp_gsp_component_set_visible(ui, updates[i].key,
            updates[i].value.data.boolean);
        if (err != ESP_GSP_OK) return err;
    }
    return ESP_GSP_OK;
}
bool mosaic_liquid_select_style(liquid_engine_style_t style)
{
    assert(style >= LIQUID_PIXEL && style <= LIQUID_WATER);
    s_selection = style;
    return true;
}
esp_err_t mosaic_loader_invalidate_app(uint16_t app_id, uint32_t revision)
{
    (void)revision;
    assert(app_id == 7);
    return ESP_OK;
}
static bool unavailable(void) { return getenv("WORKS_UNAVAILABLE") != NULL; }
esp_err_t works_runtime_init(const works_runtime_config_t *config)
{
    assert(config->on_changed != NULL);
    return unavailable() ? ESP_FAIL : ESP_OK;
}
esp_err_t works_runtime_refresh(void) { return ESP_OK; }
esp_err_t works_runtime_flush(void) { return ESP_OK; }
esp_err_t works_runtime_get_count(size_t *count)
{
    if (unavailable()) return ESP_FAIL;
    *count = 3;
    return ESP_OK;
}
esp_err_t works_runtime_get_item(size_t index, works_runtime_item_snapshot_t *item)
{
    static const char *const ids[] = {"album_app", "dino", "flappybird"};
    static const char *const names[] = {"Album", "Dino", "Flappy Bird"};
    if (unavailable() || index >= 3) return ESP_ERR_NOT_FOUND;
    memset(item, 0, sizeof(*item));
    snprintf(item->app_id, sizeof(item->app_id), "%s", ids[index]);
    snprintf(item->display_name, sizeof(item->display_name), "%s", names[index]);
    item->builtin = true;
    return ESP_OK;
}
esp_err_t works_runtime_get_recent(size_t index, works_runtime_item_snapshot_t *item)
{
    return index == 0 ? works_runtime_get_item(1, item) : ESP_ERR_NOT_FOUND;
}
esp_err_t works_runtime_request_toggle(const char *id)
{
    assert(strcmp(id, "dino") == 0 || strcmp(id, "flappybird") == 0 || strcmp(id, "album_app") == 0);
    printf("LUA_TOGGLE %s\n", id);
    return ESP_OK;
}

static void call(esp_gsp_handle_t ui, const esp_gsp_event_t *event, void *ctx)
{
    (void)ctx;
    if (event->type != ESP_GSP_EVENT_CALL) return;
    const mosaic_event_t translated = {.type = MOSAIC_EVENT_UI_CALL,
        .data.call = {.action_id = event->action_id, .arg = event->arg}};
    mosaic_works_app.on_event(ui, &translated);
    const mosaic_app_descriptor_t *target = NULL;
    if (mosaic_app_route_event(&mosaic_works_app, event, &target)) {
        assert(target == &s_liquid && s_selection >= 0);
        printf("NATIVE_ROUTE %s\n", s_native_names[s_selection]);
    }
    if (s_selected_tab != WORKS_TAB_RECENT && s_installed_page == 0) {
        for (size_t i = 0; i < 3; ++i) {
            assert(strcmp(s_names[i], s_native_names[i]) == 0);
            assert(s_native_visible[i] && !s_lua_visible[i]);
        }
    } else {
        for (size_t i = 0; i < 3; ++i) assert(!s_native_visible[i]);
    }
}
esp_gsp_err_t gsp_bridge_app_init(esp_gsp_handle_t ui)
{
    const mosaic_event_t start = {.type = MOSAIC_EVENT_START};
    mosaic_works_app.on_event(ui, &start);
    for (size_t i = 0; i < 3; ++i) {
        assert(strcmp(s_names[i], s_native_names[i]) == 0);
        assert(s_native_visible[i] && !s_lua_visible[i]);
    }
    assert(works_page_item_count() == (unavailable() ? 3 : 5));
    return esp_gsp_on_event(ui, call, NULL);
}
void gsp_bridge_app_deinit(esp_gsp_handle_t ui) { (void)ui; }

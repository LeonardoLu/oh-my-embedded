/* Run the real Settings binder and state setters through ESP-GSP's C bridge. */
#include "gsp_sim_bridge.h"
#include "settings_objects.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool s_bluetooth_enabled;
static bool s_capture_probe;
static unsigned s_capture_refreshes;
static unsigned s_snapshot_calls;
static unsigned s_control_writes;

static esp_gsp_err_t native_list_refresh(esp_gsp_handle_t ui,
    esp_gsp_list_t list)
{
    if (s_capture_probe) ++s_capture_refreshes;
    return esp_gsp_list_refresh(ui, list);
}

static bool settings_native_bluetooth_enabled(void)
{
    return s_bluetooth_enabled;
}

static esp_gsp_err_t native_bluetooth_status(esp_gsp_handle_t ui,
    const char *text)
{
    const esp_gsp_err_t err = gsp_settings_settings_bluetooth_status_set_text(ui, text);
    if (err == ESP_GSP_OK) fprintf(stderr, "native Bluetooth status=%s\n", text);
    return err;
}

static esp_gsp_err_t native_poweroff_label(esp_gsp_handle_t ui,
    const char *text)
{
    const esp_gsp_err_t err =
        gsp_settings_settings_poweroff_timeout_value_set_text(ui, text);
    if (err == ESP_GSP_OK) fprintf(stderr, "native Power-off label=%s\n", text);
    return err;
}

#define gsp_settings_settings_bluetooth_status_set_text native_bluetooth_status
#define gsp_settings_settings_poweroff_timeout_value_set_text native_poweroff_label
#define esp_gsp_list_refresh native_list_refresh
#include SETTINGS_APP_SOURCE
#undef gsp_settings_settings_bluetooth_status_set_text
#undef gsp_settings_settings_poweroff_timeout_value_set_text
#undef esp_gsp_list_refresh

static unsigned s_ticks;
static uint32_t s_poweroff_timeout;
static unsigned s_display_input;
static bool s_capture_refresh;

int64_t esp_timer_get_time(void)
{
    return (int64_t)gsp_sim_bridge_time_ms() * 1000;
}

const char *esp_err_to_name(esp_err_t err)
{
    (void)err;
    return "native Settings provider error";
}

static esp_err_t snapshot(void *ctx, mosaic_settings_snapshot_t *out)
{
    (void)ctx;
    ++s_snapshot_calls;
    memset(out, 0, sizeof(*out));
    out->brightness = 80;
    out->volume = 80;
    out->screen_timeout_ms = 30000;
    out->dim_timeout_ms = 10000;
    out->poweroff_timeout_ms = s_poweroff_timeout;
    out->network.enabled = true;
    out->network.desired_enabled = true;
    out->network.state = MOSAIC_SETTINGS_WIFI_IDLE;
    return ESP_OK;
}

static esp_err_t rotation(void *ctx, uint16_t value)
{
    (void)ctx;
    (void)value;
    ++s_control_writes;
    return ESP_OK;
}

static esp_err_t level(void *ctx, int value, bool persist)
{
    (void)ctx;
    (void)value;
    (void)persist;
    ++s_control_writes;
    return ESP_OK;
}

static esp_err_t network(void *ctx)
{
    (void)ctx;
    ++s_control_writes;
    return ESP_OK;
}

static esp_err_t poweroff_timeout(void *ctx, uint32_t timeout_ms)
{
    (void)ctx;
    ++s_control_writes;
    s_poweroff_timeout = timeout_ms;
    fprintf(stderr, "native Power-off saved=%u\n", (unsigned)timeout_ms);
    return ESP_OK;
}

static void pointer(esp_gsp_handle_t ui, int32_t x, int32_t y,
    bool pressed, void *ctx)
{
    (void)ctx;
    const int32_t before = s_state.display_scroll_offset;
    const mosaic_event_t input = {
        .type = MOSAIC_EVENT_POINTER,
        .timestamp_us = esp_timer_get_time(),
        .data.pointer = {.x = x, .y = y, .pressed = pressed},
    };
    mosaic_settings_app.on_event(ui, &input);
    if (before != s_state.display_scroll_offset) {
        fprintf(stderr, "native Display scroll=%ld\n",
            (long)s_state.display_scroll_offset);
    }
}

static void call(esp_gsp_handle_t ui, const esp_gsp_event_t *event,
    void *ctx)
{
    (void)ctx;
    if (event->type != ESP_GSP_EVENT_CALL) return;
    const mosaic_event_t input = {
        .type = MOSAIC_EVENT_UI_CALL,
        .timestamp_us = esp_timer_get_time(),
        .data.call = {
            .action_id = event->action_id, .arg = event->arg,
            .scene_id = event->scene_id, .list = event->list,
            .item = event->item,
        },
    };
    mosaic_settings_app.on_event(ui, &input);
    if (event->action_id == GSP_ACT_ID_SETTINGS_DISPLAY_OPTIONS_OPEN) {
        fprintf(stderr, "native chooser open=%u kind=%u\n",
            s_state.display_options_open, (unsigned)s_state.display_option);
    } else if (event->action_id == GSP_ACT_ID_SETTINGS_DISPLAY_OPTION_SELECT) {
        fprintf(stderr, "native chooser open=%u value=%u scroll=%ld\n",
            s_state.display_options_open,
            (unsigned)s_state.snapshot.poweroff_timeout_ms,
            (long)s_state.display_scroll_offset);
    }
}

static void tick(esp_gsp_handle_t ui, void *ctx)
{
    (void)ctx;
    ++s_ticks;
    if (s_ticks <= 6 || (s_ticks >= 60 && s_ticks <= 65)) {
        settings_root_list_refresh(ui);
    }
    const mosaic_event_t input = {
        .type = MOSAIC_EVENT_TIMER,
        .timestamp_us = esp_timer_get_time(),
    };
    mosaic_settings_app.on_event(ui, &input);
    if (s_capture_refresh && s_ticks == 80) {
        uint16_t page_before = UINT16_MAX;
        assert(esp_gsp_stack_view_get_top(ui, GSP_OBJ_KEY_SETTINGS_STACK,
            &page_before) == ESP_GSP_OK);
        unsigned char before[sizeof(s_state)];
        memcpy(before, &s_state, sizeof(before));
        const esp_gsp_list_t list_before = s_root_list;
        const unsigned snapshots_before = s_snapshot_calls;
        const unsigned writes_before = s_control_writes;
        const unsigned refreshes_before = s_capture_refreshes;
        const mosaic_event_t capture = {.type = MOSAIC_EVENT_CAPTURE_REFRESH};
        s_capture_probe = true;
        mosaic_settings_app.on_event(ui, &capture);
        s_capture_probe = false;
        uint16_t page_after = UINT16_MAX;
        assert(esp_gsp_stack_view_get_top(ui, GSP_OBJ_KEY_SETTINGS_STACK,
            &page_after) == ESP_GSP_OK);
        assert(page_before == page_after && s_root_list == list_before);
        assert(!memcmp(before, &s_state, sizeof(before)));
        assert(s_snapshot_calls == snapshots_before &&
            s_control_writes == writes_before);
        const unsigned refreshes = s_capture_refreshes - refreshes_before;
        assert(refreshes == (page_before == 0 && list_before != ESP_GSP_LIST_NONE));
        fprintf(stderr, "native capture refresh page=%u requests=%u state=preserved providers=0 writes=0\n",
            page_before, refreshes);
    }
    /* Native host CLI samples are not forwarded by every bridge release.
     * Inject only the input boundary: the registered observers still invoke
     * the unchanged Settings C scrolling/action/setter/render paths. */
    if (s_display_input && s_ticks == 20) {
        pointer(ui, 240, 430, true, NULL);
        /* Include the tap slop so this drag reaches the bottom clamp. */
        for (int32_t y = 415; y >= 115; y -= 15) {
            pointer(ui, 240, y, true, NULL);
        }
        pointer(ui, 240, 115, false, NULL);
    }
    if (s_display_input >= 2 && s_ticks == 30) {
        const esp_gsp_event_t open = {
            .type = ESP_GSP_EVENT_CALL,
            .action_id = GSP_ACT_ID_SETTINGS_DISPLAY_OPTIONS_OPEN,
            .arg = SETTINGS_DISPLAY_OPTION_POWEROFF_TIMEOUT,
        };
        call(ui, &open, NULL);
    }
    if (s_display_input == 3 && s_ticks == 40) {
        const esp_gsp_event_t select = {
            .type = ESP_GSP_EVENT_CALL,
            .action_id = GSP_ACT_ID_SETTINGS_DISPLAY_OPTION_SELECT,
            .arg = 0,
        };
        call(ui, &select, NULL);
    }
}

esp_gsp_err_t gsp_bridge_app_init(esp_gsp_handle_t ui)
{
    const mosaic_settings_ops_t ops = {
        .get_snapshot = snapshot,
        .set_rotation = rotation,
        .set_brightness = level,
        .set_volume = level,
        .request_network_reconfigure = network,
        .set_poweroff_timeout = poweroff_timeout,
    };
    const char *checked = getenv("SETTINGS_NATIVE_CHECKED");
    s_bluetooth_enabled = checked && checked[0] == '1';
    const char *page = getenv("SETTINGS_NATIVE_PAGE");
    if (page) {
        /* The presentation fixture authors this initial page. Mirror it in
         * the bridge's navigation double so real C pointer guards agree. */
        (void)esp_gsp_stack_view_push(ui, GSP_OBJ_KEY_SETTINGS_STACK,
            (uint16_t)atoi(page), false);
    }
    const char *input = getenv("SETTINGS_NATIVE_DISPLAY_INPUT");
    s_capture_refresh = getenv("SETTINGS_NATIVE_CAPTURE_REFRESH") != NULL;
    if (input) {
        s_display_input = !strcmp(input, "selected") ? 3
            : !strcmp(input, "chooser") ? 2 : 1;
        fprintf(stderr, "native input boundary=fixture %s\n", input);
    }
    mosaic_settings_configure(&ops);
    const mosaic_event_t start = {.type = MOSAIC_EVENT_START};
    mosaic_settings_app.on_event(ui, &start);
    if (checked) settings_bluetooth_render(ui);
    (void)esp_gsp_timer_create(ui, 16, tick, NULL);
    esp_gsp_err_t err = esp_gsp_set_pointer_observer(ui, pointer, NULL);
    if (err == ESP_GSP_OK) err = esp_gsp_on_event(ui, call, NULL);
    return err;
}

void gsp_bridge_app_deinit(esp_gsp_handle_t ui)
{
    (void)esp_gsp_set_pointer_observer(ui, NULL, NULL);
    (void)esp_gsp_on_event(ui, NULL, NULL);
    const mosaic_event_t stop = {.type = MOSAIC_EVENT_STOP};
    mosaic_settings_app.on_event(ui, &stop);
}

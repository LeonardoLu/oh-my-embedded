/* Run the real Settings binder and state setters through ESP-GSP's C bridge. */
#include "gsp_sim_bridge.h"
#include "settings_objects.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool s_bluetooth_enabled;

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

#define gsp_settings_settings_bluetooth_status_set_text native_bluetooth_status
#include SETTINGS_APP_SOURCE
#undef gsp_settings_settings_bluetooth_status_set_text

static unsigned s_ticks;

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
    memset(out, 0, sizeof(*out));
    out->brightness = 80;
    out->volume = 80;
    out->screen_timeout_ms = 30000;
    out->dim_timeout_ms = 10000;
    out->network.enabled = true;
    out->network.desired_enabled = true;
    out->network.state = MOSAIC_SETTINGS_WIFI_IDLE;
    return ESP_OK;
}

static esp_err_t rotation(void *ctx, uint16_t value)
{
    (void)ctx;
    (void)value;
    return ESP_OK;
}

static esp_err_t level(void *ctx, int value, bool persist)
{
    (void)ctx;
    (void)value;
    (void)persist;
    return ESP_OK;
}

static esp_err_t network(void *ctx)
{
    (void)ctx;
    return ESP_OK;
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
}

esp_gsp_err_t gsp_bridge_app_init(esp_gsp_handle_t ui)
{
    const mosaic_settings_ops_t ops = {
        .get_snapshot = snapshot,
        .set_rotation = rotation,
        .set_brightness = level,
        .set_volume = level,
        .request_network_reconfigure = network,
    };
    const char *checked = getenv("SETTINGS_NATIVE_CHECKED");
    s_bluetooth_enabled = checked && checked[0] == '1';
    mosaic_settings_configure(&ops);
    const mosaic_event_t start = {.type = MOSAIC_EVENT_START};
    mosaic_settings_app.on_event(ui, &start);
    if (checked) settings_bluetooth_render(ui);
    (void)esp_gsp_timer_create(ui, 16, tick, NULL);
    return ESP_GSP_OK;
}

void gsp_bridge_app_deinit(esp_gsp_handle_t ui)
{
    const mosaic_event_t stop = {.type = MOSAIC_EVENT_STOP};
    mosaic_settings_app.on_event(ui, &stop);
}

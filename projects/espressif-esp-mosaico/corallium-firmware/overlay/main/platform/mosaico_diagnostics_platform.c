// SPDX-License-Identifier: MIT
#include "mosaico_diagnostics_platform.h"
#include "sdkconfig.h"

#if CONFIG_MOSAICO_USB_DIAGNOSTICS
#include <string.h>
#include "app_system_config.h"
#include "corallium.h"
#include "mosaico_audio.h"
#include "mosaico_diagnostics.h"
#include "mosaic_settings.h"
#include "mosaic_ui.h"
#include "wifi_manager.h"

static esp_err_t diagnostics_status(void *ctx, mosaico_diagnostics_status_t *out)
{
    mosaic_ui_diagnostics_t ui;
    esp_err_t err = mosaic_ui_get_diagnostics(&ui, 300);
    if (err != ESP_OK) return err;
    app_system_config_t controls;
    err = app_settings_service_get_display_config(ctx, &controls);
    if (err != ESP_OK) return err;
    wifi_manager_status_t wifi = {0};
    wifi_manager_get_status(&wifi);
    mosaic_settings_battery_t battery = {0};
    /* The sampler publishes even an unavailable first result before startup.
     * Use its Settings cache; status must never poll or initialize the gauge. */
    (void)mosaic_settings_get_battery(&battery);

    memset(out, 0, sizeof(*out));
    memcpy(out->app_name, ui.app_name, sizeof(out->app_name));
    out->app_id = ui.app_id;
    out->display_owner = ui.display_owner == MOSAIC_UI_DISPLAY_OWNER_GSP
        ? MOSAICO_DIAGNOSTICS_OWNER_GSP
        : ui.display_owner == MOSAIC_UI_DISPLAY_OWNER_EXCLUSIVE
        ? MOSAICO_DIAGNOSTICS_OWNER_EXCLUSIVE : MOSAICO_DIAGNOSTICS_OWNER_NONE;
    out->width = ui.width;
    out->height = ui.height;
    out->rotation = ui.rotation;
    out->started = ui.started;
    out->asleep = ui.asleep;
    out->dimmed = ui.dimmed;
    out->panel_enabled = ui.panel_enabled;
    out->hub_presenter_active = ui.hub_presenter_active;
    out->runtime_started = ui.runtime_started;
    out->quiesced = ui.quiesced;
    out->screen_paused = ui.screen_paused;
    out->drawer_open = ui.drawer_open;
    out->gesture_active = ui.gesture_active;
    out->capture_supported = ui.capture_supported;
    out->queue_depth = ui.queue_depth;
    out->render_frames = ui.render_frames;
    out->render_busy_us = ui.render_busy_us;
    out->render_errors = ui.render_errors;
    out->last_render_error = ui.last_render_error;
    out->runtime_errors = ui.runtime_errors;
    out->last_runtime_error = ui.last_runtime_error;
    out->last_input_error = ui.last_input_error;
    const int volume = mosaico_audio_get_volume();
    out->volume_percent = volume >= 0 && volume <= 100 ? volume : controls.volume;
    out->brightness_percent = controls.brightness;
    out->panel_brightness_percent = ui.brightness_percent;
    out->dim_timeout_ms = controls.dim_timeout_ms;
    out->sleep_timeout_ms = controls.screen_timeout_ms;
    out->poweroff_timeout_ms = controls.poweroff_timeout_ms;
    out->dim_while_charging = controls.dim_while_charging;
    out->sleep_while_charging = controls.sleep_while_charging;
    out->wifi_enabled = wifi.desired_enabled;
    out->wifi_connected = wifi.sta_connected;
    out->bluetooth_enabled = corallium_pairing_active();
    out->battery_available = battery.available;
    out->charging = battery.charging;
    return ESP_OK;
}

static esp_err_t diagnostics_tap(void *ctx, uint16_t x, uint16_t y)
{
    (void)ctx;
    return mosaic_ui_simulate_tap(x, y);
}

static esp_err_t diagnostics_drag(void *ctx, uint16_t x0, uint16_t y0,
    uint16_t x1, uint16_t y1, uint32_t duration_ms)
{
    (void)ctx;
    return mosaic_ui_simulate_drag(x0, y0, x1, y1, duration_ms);
}

static esp_err_t diagnostics_back(void *ctx)
{
    (void)ctx;
    return mosaic_ui_back();
}

static esp_err_t diagnostics_open_app(void *ctx, mosaico_diagnostics_app_t app)
{
    (void)ctx;
    const char *name;
    switch (app) {
    case MOSAICO_DIAGNOSTICS_APP_SETTINGS: name = "settings"; break;
    case MOSAICO_DIAGNOSTICS_APP_WORKS: name = "works"; break;
    case MOSAICO_DIAGNOSTICS_APP_ALBUM: name = "album"; break;
    case MOSAICO_DIAGNOSTICS_APP_WEATHER: name = "weather"; break;
    default: return ESP_ERR_INVALID_ARG;
    }
    /* The UI loader admits a request without waiting for GSP startup. */
    return mosaic_ui_open_app(name);
}

static esp_err_t diagnostics_capture_begin(void *ctx,
    mosaico_diagnostics_frame_t *out, void **handle)
{
    (void)ctx;
    *handle = NULL;
    mosaic_ui_frame_info_t info;
    mosaic_ui_frame_handle_t frame = NULL;
    const esp_err_t err = mosaic_ui_capture_begin(&info, &frame, 3000);
    if (err != ESP_OK) return err;
    *out = (mosaico_diagnostics_frame_t){
        .width = info.width, .height = info.height,
        .stride_bytes = info.stride_bytes, .size_bytes = info.size_bytes,
    };
    *handle = frame;
    return ESP_OK;
}

static esp_err_t diagnostics_capture_read(void *ctx, void *handle,
    uint32_t offset, uint8_t *dst, size_t length)
{
    (void)ctx;
    return mosaic_ui_capture_read(handle, offset, dst, length);
}

static void diagnostics_capture_end(void *ctx, void *handle)
{
    (void)ctx;
    mosaic_ui_capture_end(handle);
}
#endif

esp_err_t mosaico_diagnostics_platform_start(app_settings_service_handle_t settings)
{
#if CONFIG_MOSAICO_USB_DIAGNOSTICS
    static mosaico_diagnostics_ops_t ops;
    ops = (mosaico_diagnostics_ops_t){
        .ctx = settings, .status = diagnostics_status,
        .tap = diagnostics_tap, .drag = diagnostics_drag,
        .back = diagnostics_back, .open_app = diagnostics_open_app,
        .capture_begin = diagnostics_capture_begin,
        .capture_read = diagnostics_capture_read, .capture_end = diagnostics_capture_end,
    };
    return mosaico_diagnostics_start(&ops);
#else
    (void)settings;
    return ESP_OK;
#endif
}

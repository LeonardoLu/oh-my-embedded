// SPDX-License-Identifier: MIT
/* Actual runtime/catalog: replacing a child must start its package timer. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "mosaic_runtime.h"

static const mosaic_app_route_t works_routes[] = {{7, "liquid"}};
static const mosaic_app_descriptor_t hub = {
    .id = 0, .name = "hub", .launch_action = MOSAIC_APP_NO_LAUNCH_ACTION,
};
static const mosaic_app_descriptor_t works = {
    .id = 1, .name = "works", .back_action = MOSAIC_APP_SHELL_BACK_ACTION,
    .routes = works_routes, .route_count = 1,
};
static const mosaic_app_descriptor_t liquid = {
    .id = 2, .name = "liquid", .back_action = 1,
};
const mosaic_app_descriptor_t *const mosaic_app_registry[] = {&hub, &works, &liquid};
const size_t mosaic_app_registry_count = 3;
const mosaic_app_package_t mosaic_app_packages[] = {
    {.descriptor = &hub}, {.descriptor = &works, .tick_ms = 1000},
    {.descriptor = &liquid, .tick_ms = 33},
};
const size_t mosaic_app_package_count = 3;

static struct {
    const mosaic_app_descriptor_t *active;
    mosaic_platform_ui_event_cb_t event;
    void *event_ctx;
    unsigned ticks[3];
    bool fail_replace;
} platform;

static esp_err_t open_app(void *ctx, const mosaic_app_descriptor_t *app,
    mosaic_platform_ui_event_cb_t event, void *event_ctx,
    mosaic_platform_app_handle_t *out)
{
    assert(ctx == &platform);
    platform.active = app;
    platform.event = event;
    platform.event_ctx = event_ctx;
    *out = &platform;
    return ESP_OK;
}
static esp_err_t replace_app(void *ctx, mosaic_platform_app_handle_t current,
    const mosaic_app_descriptor_t *app, mosaic_platform_ui_event_cb_t event,
    void *event_ctx, mosaic_platform_app_handle_t *out)
{
    assert(current == &platform);
    if (platform.fail_replace) {
        platform.fail_replace = false;
        return ESP_FAIL;
    }
    return open_app(ctx, app, event, event_ctx, out);
}
static esp_err_t close_app(void *ctx, mosaic_platform_app_handle_t app)
{
    assert(ctx == &platform && app == &platform);
    return ESP_OK;
}
static esp_err_t step_app(void *ctx, mosaic_platform_app_handle_t app, int64_t now)
{
    (void)now;
    assert(ctx == &platform && app == &platform);
    return ESP_OK;
}
static esp_err_t dispatch(void *ctx, mosaic_platform_app_handle_t app,
    const mosaic_event_t *event)
{
    assert(ctx == &platform && app == &platform);
    if (event->type == MOSAIC_EVENT_TIMER) {
        assert(strcmp(event->data.timer.id, "app_tick") == 0);
        ++platform.ticks[platform.active->id];
    }
    return ESP_OK;
}
static esp_err_t pointer(void *ctx, mosaic_platform_app_handle_t app,
    int32_t x, int32_t y, bool pressed)
{
    (void)ctx; (void)app; (void)x; (void)y; (void)pressed;
    return ESP_OK;
}
static void choose_liquid(void)
{
    const esp_gsp_event_t event = {.type = ESP_GSP_EVENT_CALL, .action_id = 7};
    platform.event(platform.event_ctx, &event);
}

int main(void)
{
    const mosaic_platform_ops_t ops = {.open_app = open_app, .replace_app = replace_app,
        .close_app = close_app, .step_app = step_app, .dispatch_event = dispatch,
        .feed_pointer = pointer};
    const mosaic_runtime_config_t config = {.platform = &ops, .platform_ctx = &platform};
    mosaic_runtime_handle_t runtime;
    assert(mosaic_runtime_create(&config, &runtime) == ESP_OK);
    assert(mosaic_runtime_start(runtime, "works") == ESP_OK);
    assert(mosaic_runtime_step(runtime, 100000) == ESP_OK);
    choose_liquid();
    assert(mosaic_runtime_step(runtime, 100000) == ESP_OK);
    assert(mosaic_runtime_active_app(runtime) == &liquid);
    assert(mosaic_runtime_step(runtime, 132999) == ESP_OK && platform.ticks[2] == 0);
    assert(mosaic_runtime_step(runtime, 133000) == ESP_OK && platform.ticks[2] == 1);
    assert(mosaic_runtime_step(runtime, 166000) == ESP_OK && platform.ticks[2] == 2);
    assert(platform.ticks[1] == 0);

    assert(mosaic_runtime_back(runtime) == ESP_OK);
    assert(mosaic_runtime_step(runtime, 200000) == ESP_OK);
    assert(mosaic_runtime_active_app(runtime) == &works);
    assert(mosaic_runtime_step(runtime, 1199999) == ESP_OK && platform.ticks[1] == 0);
    assert(mosaic_runtime_step(runtime, 1200000) == ESP_OK && platform.ticks[1] == 1);
    assert(platform.ticks[2] == 2);

    platform.fail_replace = true;
    choose_liquid();
    assert(mosaic_runtime_step(runtime, 1300000) == ESP_FAIL);
    assert(mosaic_runtime_active_app(runtime) == &works);
    assert(mosaic_runtime_step(runtime, 2300000) == ESP_OK && platform.ticks[1] == 2);
    assert(mosaic_runtime_back(runtime) == ESP_OK);
    assert(mosaic_runtime_step(runtime, 2400000) == ESP_OK);
    assert(mosaic_runtime_active_app(runtime) == &hub);
    assert(mosaic_runtime_step(runtime, 9999999) == ESP_OK);
    assert(platform.ticks[0] == 0 && platform.ticks[1] == 2 && platform.ticks[2] == 2);
    mosaic_runtime_delete(runtime);
    puts("PASS: direct child timer scheduling, Back period restoration, failed replacement recovery and timer isolation");
    return 0;
}

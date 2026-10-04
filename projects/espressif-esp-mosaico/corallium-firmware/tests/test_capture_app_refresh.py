#!/usr/bin/env python3
"""Run verbatim capture handlers with SDK request/error boundaries doubled.

Settings' whole dispatcher and real List/scroll are also covered by
test_settings_native.py. This probe exercises the capture branch's root-page
gate and the whole Weather handler, including enqueue failure diagnostics;
it makes no claim about target decode readiness or displayed pixels.
"""

import argparse
import os
from pathlib import Path
import re
import subprocess


PROJECT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--upstream", type=Path, required=True)
parser.add_argument("--output", type=Path)
args = parser.parse_args()
upstream = args.upstream.resolve()
ui = upstream / "components/mosaic_ui"
output = (args.output or PROJECT.parents[2] / "tmp/mosaico/host-tests/capture-app-refresh").resolve()
output.mkdir(parents=True, exist_ok=True)
settings_path = ui / "apps/settings/settings_app.c"
weather_path = ui / "apps/weather/weather_app.c"
platform_path = ui / "core/mosaic_esp_platform.c"
settings = settings_path.read_text()
weather = weather_path.read_text()
platform = platform_path.read_text()
branch = re.search(r"^    case MOSAIC_EVENT_CAPTURE_REFRESH:.*?(?=^    case )",
                   settings, re.M | re.S)
handler = re.search(r"^static void weather_event\(.*?^}\n", weather, re.M | re.S)
helper = re.search(r"^esp_err_t mosaic_esp_platform_prepare_capture\(.*?^}\n",
                   platform, re.M | re.S)
assert branch and handler and helper, "Missing actual capture event handlers/helper"
assert ".on_event = weather_event," in weather, "Weather handler is not registered"
source = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "mosaic_runtime.h"

/* Only the SDK request boundary is doubled. It retains a pre-existing row
 * view and reports its admission error; it does not simulate image pixels. */
static esp_gsp_handle_t ui = (esp_gsp_handle_t)1;
static esp_gsp_list_t s_root_list, s_forecast_list;
static const gsp_component_key_t GSP_OBJ_KEY_SETTINGS_STACK = 42;
static const char *TAG = "settings_app";
static uint16_t page;
static esp_gsp_err_t page_error, refresh_error;
static unsigned refresh_calls, warning_calls;
static esp_gsp_list_t last_list;
static char warning[64];

static void record_warning(const char *format, ...)
{
    ++warning_calls;
    va_list args;
    va_start(args, format);
    const int length = vsnprintf(warning, sizeof(warning), format, args);
    va_end(args);
    assert(length > 0 && (size_t)length < sizeof(warning));
}
#define ESP_LOGW(tag, ...) do { (void)(tag); record_warning(__VA_ARGS__); } while (0)

esp_gsp_err_t esp_gsp_stack_view_get_top(esp_gsp_handle_t handle,
    gsp_component_key_t key, uint16_t *out)
{
    assert(handle == ui && key == GSP_OBJ_KEY_SETTINGS_STACK);
    if (page_error == ESP_GSP_OK) *out = page;
    return page_error;
}

esp_gsp_err_t esp_gsp_list_refresh(esp_gsp_handle_t handle, esp_gsp_list_t list)
{
    assert(handle == ui && list != ESP_GSP_LIST_NONE);
    ++refresh_calls;
    last_list = list;
    return refresh_error;
}

static void settings_capture_event(esp_gsp_handle_t ui, const mosaic_event_t *event)
{
    if (event == NULL) return;
    switch (event->type) {
SETTINGS_BRANCH
    default: break;
    }
}

WEATHER_HANDLER

/* The helper reads these private session fields only. Their layout is a
 * boundary double; the native descriptor/package types stay authoritative. */
typedef struct {
    const mosaic_app_package_t *package;
    esp_gsp_handle_t ui;
    void *logic;
    bool stopping;
} mosaic_esp_app_t;
typedef struct mosaic_esp_platform_t {
    mosaic_esp_app_t *active;
    bool paused;
    void *screen_pause;
} *mosaic_esp_platform_handle_t;

static const int64_t clock_us = 123456789;
int64_t esp_timer_get_time(void) { return clock_us; }
PLATFORM_HELPER

static unsigned native_calls;
static mosaic_event_t received;
static void native_event(esp_gsp_handle_t handle, const struct mosaic_event *event)
{
    assert(handle == ui);
    ++native_calls;
    received = *event;
}

static void test_platform_helper(void)
{
    mosaic_app_descriptor_t descriptor = {.on_event = native_event};
    mosaic_app_package_t package = {
        .descriptor = &descriptor,
        .logic = (const struct mosaic_logic_ops *)1,
    };
    mosaic_esp_app_t app = {.package = &package, .ui = ui, .logic = (void *)1};
    struct mosaic_esp_platform_t session = {.active = &app};
    assert(mosaic_esp_platform_prepare_capture(NULL) == ESP_ERR_INVALID_STATE);
    for (unsigned guard = 0; guard < 7; ++guard) {
        session = (struct mosaic_esp_platform_t){.active = &app};
        app = (mosaic_esp_app_t){.package = &package, .ui = ui, .logic = (void *)1};
        package.descriptor = &descriptor;
        switch (guard) {
        case 0: session.active = NULL; break;
        case 1: session.paused = true; break;
        case 2: session.screen_pause = (void *)1; break;
        case 3: app.stopping = true; break;
        case 4: app.ui = NULL; break;
        case 5: app.package = NULL; break;
        case 6: package.descriptor = NULL; break;
        }
        assert(mosaic_esp_platform_prepare_capture(&session) == ESP_ERR_INVALID_STATE);
        assert(native_calls == 0);
    }
    session = (struct mosaic_esp_platform_t){.active = &app};
    app = (mosaic_esp_app_t){.package = &package, .ui = ui, .logic = (void *)1};
    package.descriptor = &descriptor;
    assert(mosaic_esp_platform_prepare_capture(&session) == ESP_OK);
    assert(native_calls == 1 && received.type == MOSAIC_EVENT_CAPTURE_REFRESH);
    assert(received.timestamp_us == clock_us);
    const mosaic_event_t empty = {0};
    assert(!memcmp(&received.data, &empty.data, sizeof(received.data)));
    assert(app.logic == (void *)1 && app.ui == ui && session.active == &app);
    descriptor.on_event = NULL;
    assert(mosaic_esp_platform_prepare_capture(&session) == ESP_OK);
    assert(native_calls == 1);
    puts("PASS: verbatim platform helper: all inactive/paused/stopping/null guards, "
        "native-only callback with current timestamp and no payload, optional callback no-op");
}

static void reset(void)
{
    s_root_list = 3;
    s_forecast_list = 4;
    page = 0;
    page_error = refresh_error = ESP_GSP_OK;
    refresh_calls = warning_calls = 0;
    last_list = ESP_GSP_LIST_NONE;
    memset(warning, 0, sizeof(warning));
}

int main(void)
{
    test_platform_helper();
    const mosaic_event_t capture = {.type = MOSAIC_EVENT_CAPTURE_REFRESH};
    for (page = 0; page < 14; ++page) {
        const uint16_t before = page;
        s_root_list = 3;
        refresh_calls = warning_calls = 0;
        settings_capture_event(ui, &capture);
        assert(page == before && s_root_list == 3);
        assert(refresh_calls == (page == 0) && warning_calls == 0);
        if (page == 0) assert(last_list == s_root_list);
    }
    reset();
    s_root_list = ESP_GSP_LIST_NONE;
    settings_capture_event(ui, &capture);
    assert(refresh_calls == 0 && warning_calls == 0);
    reset();
    page_error = ESP_GSP_ERR_INVALID_STATE;
    settings_capture_event(ui, &capture);
    assert(refresh_calls == 0 && warning_calls == 0);
    reset();
    refresh_error = ESP_GSP_ERR_INVALID_STATE;
    settings_capture_event(ui, &capture);
    assert(refresh_calls == 1 && warning_calls == 1);
    assert(!strcmp(warning, "capture refresh failed err=259"));
    assert(page == 0 && s_root_list == 3);

    reset();
    weather_event(ui, &capture);
    assert(refresh_calls == 1 && last_list == s_forecast_list);
    assert(s_forecast_list == 4 && warning_calls == 0);
    reset();
    s_forecast_list = ESP_GSP_LIST_NONE;
    weather_event(ui, &capture);
    assert(refresh_calls == 0 && warning_calls == 0);
    reset();
    weather_event(ui, NULL);
    settings_capture_event(ui, NULL);
    for (int type = MOSAIC_EVENT_START; type < MOSAIC_EVENT_CAPTURE_REFRESH; ++type) {
        const mosaic_event_t other = {.type = (mosaic_event_type_t)type};
        weather_event(ui, &other);
    }
    assert(refresh_calls == 0 && warning_calls == 0);
    reset();
    refresh_error = ESP_GSP_ERR_INVALID_STATE;
    weather_event(ui, &capture);
    assert(refresh_calls == 1 && warning_calls == 1 && s_forecast_list == 4);
    assert(!strcmp(warning, "capture refresh failed err=259"));
    puts("PASS: verbatim capture handlers: Settings root gate/all 13 child pages, "
        "unbound lists/read failure/no unrelated Weather events, retained list/page, "
        "numeric request-failure diagnostics; SDK boundary double, no pixel/readiness claim");
}
'''
settings_line = settings.count("\n", 0, branch.start()) + 1
weather_line = weather.count("\n", 0, handler.start()) + 1
platform_line = platform.count("\n", 0, helper.start()) + 1
source = source.replace("SETTINGS_BRANCH", f'#line {settings_line} "{settings_path}"\n' + branch[0])
source = source.replace("WEATHER_HANDLER", f'#line {weather_line} "{weather_path}"\n' + handler[0])
source = source.replace("PLATFORM_HELPER", f'#line {platform_line} "{platform_path}"\n' + helper[0])
probe = output / "capture-app-refresh.c"
probe.write_text(source)
command = [os.environ.get("CC", "clang"), "-std=c11", "-Wall", "-Wextra", "-Werror",
           "-fsanitize=address,undefined", "-DESP_PLATFORM"]
for include in (PROJECT / "tests/settings_native/stubs", ui / "include", ui / "common", ui / "runtime",
                upstream / "managed_components/espressif__esp-gsp/include"):
    command.extend(["-I", str(include)])
executable = output / "capture-app-refresh"
subprocess.run([*command, str(probe), "-o", str(executable)], check=True)
subprocess.run([str(executable)], check=True)

#!/usr/bin/env python3
"""Execute real fixed-app diagnostic glue, registry lookup and launch admission."""
import os
from pathlib import Path
import re
import subprocess
import sys


project = Path(__file__).resolve().parents[1]
upstream = Path(sys.argv[1]).resolve()
output = project.parents[2] / "tmp/mosaico/diagnostics-open-host-tests"
output.mkdir(parents=True, exist_ok=True)
platform = (project / "overlay/main/platform/mosaico_diagnostics_platform.c").read_text()
ui = (upstream / "components/mosaic_ui/mosaic_ui.c").read_text()
loader = (upstream / "components/mosaic_ui/core/mosaic_loader.c").read_text()
catalog = (upstream / "components/mosaic_ui/common/mosaic_app_catalog.c").read_text()


def function(source, name):
    match = re.search(
        r"^(?:static )?(?:esp_err_t|const mosaic_app_descriptor_t\s*\*)\s*"
        + re.escape(name) + r"\([^)]*\)\s*\{.*?^}\n", source, re.M | re.S)
    assert match, "Missing actual C function: " + name
    return match.group(0)


test = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "mosaico_diagnostics.h"
#include "mosaico_diagnostics_platform.h"

static unsigned starts;
static esp_err_t start_result = ESP_OK;
static mosaico_diagnostics_ops_t admitted_ops;
esp_err_t mosaico_diagnostics_start(const mosaico_diagnostics_ops_t *ops) {
    assert(ops != NULL);
    ++starts;
    admitted_ops = *ops;
    return start_result;
}

#if CONFIG_MOSAICO_USB_DIAGNOSTICS
typedef struct { const char *name; } mosaic_app_descriptor_t;
static const mosaic_app_descriptor_t settings = { "settings" };
static const mosaic_app_descriptor_t works = { "works" };
static const mosaic_app_descriptor_t album = { "album" };
static const mosaic_app_descriptor_t weather = { "weather" };
static const mosaic_app_descriptor_t *mosaic_app_registry[] = {
    &settings, &works, &album, &weather,
};
static size_t mosaic_app_registry_count = 4;
enum { MOSAIC_LOADER_COMMAND_REQUEST_APP = 1, pdTRUE = 1, pdFALSE = 0 };
typedef struct {
    int type;
    union { const mosaic_app_descriptor_t *app; } data;
} mosaic_loader_command_t;
static void *s_command_queue;
static _Atomic bool s_capture_in_progress;
static _Atomic bool s_capture_interrupted;
static _Atomic bool s_started;
static _Atomic bool s_hub_presenter_active;
static unsigned queue_calls, activity_calls;
static bool queue_full;
static mosaic_loader_command_t queued;
static int xQueueSend(void *queue, const mosaic_loader_command_t *command,
        uint32_t ticks) {
    assert(queue == s_command_queue && queue != NULL);
    assert(ticks == 0); /* Direct-open must not block waiting for queue space. */
    ++queue_calls;
    if (queue_full) return pdFALSE;
    queued = *command;
    return pdTRUE;
}
void mosaic_ui_note_screen_activity(void) { ++activity_calls; }
'''
test += function(catalog, "mosaic_app_descriptor_for_name")
test += function(loader, "mosaic_loader_request")
test += function(ui, "mosaic_ui_open_app")
test += function(platform, "diagnostics_open_app")
test += r'''
/* Other platform callbacks are unrelated to app admission. Their signatures
 * match the real ops header, so the real start function's wiring is compiled. */
static esp_err_t diagnostics_status(void *ctx, mosaico_diagnostics_status_t *out) {
    (void)ctx; (void)out; return ESP_OK;
}
static esp_err_t diagnostics_tap(void *ctx, uint16_t x, uint16_t y) {
    (void)ctx; (void)x; (void)y; return ESP_OK;
}
static esp_err_t diagnostics_drag(void *ctx, uint16_t x0, uint16_t y0,
        uint16_t x1, uint16_t y1, uint32_t ms) {
    (void)ctx; (void)x0; (void)y0; (void)x1; (void)y1; (void)ms; return ESP_OK;
}
static esp_err_t diagnostics_back(void *ctx) { (void)ctx; return ESP_OK; }
static esp_err_t diagnostics_capture_begin(void *ctx,
        mosaico_diagnostics_frame_t *out, void **handle) {
    (void)ctx; (void)out; (void)handle; return ESP_OK;
}
static esp_err_t diagnostics_capture_read(void *ctx, void *handle,
        uint32_t offset, uint8_t *dst, size_t length) {
    (void)ctx; (void)handle; (void)offset; (void)dst; (void)length; return ESP_OK;
}
static void diagnostics_capture_end(void *ctx, void *handle) {
    (void)ctx; (void)handle;
}
#endif
'''
test += function(platform, "mosaico_diagnostics_platform_start")
test += r'''
int main(void) {
    int context = 7;
    assert(mosaico_diagnostics_platform_start(&context) == ESP_OK);
#if CONFIG_MOSAICO_USB_DIAGNOSTICS
    assert(starts == 1 && admitted_ops.ctx == &context);
    assert(admitted_ops.open_app == diagnostics_open_app);
    s_command_queue = &context;
    atomic_store(&s_started, true);
    atomic_store(&s_hub_presenter_active, true);
    const mosaico_diagnostics_app_t apps[] = {
        MOSAICO_DIAGNOSTICS_APP_SETTINGS, MOSAICO_DIAGNOSTICS_APP_WORKS,
        MOSAICO_DIAGNOSTICS_APP_ALBUM, MOSAICO_DIAGNOSTICS_APP_WEATHER,
    };
    for (unsigned i = 0; i < 4; ++i) {
        assert(admitted_ops.open_app(admitted_ops.ctx, apps[i]) == ESP_OK);
        assert(queue_calls == i + 1 && activity_calls == i + 1);
        assert(queued.type == MOSAIC_LOADER_COMMAND_REQUEST_APP);
        assert(queued.data.app == mosaic_app_registry[i]);
    }
    assert(admitted_ops.open_app(&context, (mosaico_diagnostics_app_t)-1) == ESP_ERR_INVALID_ARG);
    assert(admitted_ops.open_app(&context, (mosaico_diagnostics_app_t)4) == ESP_ERR_INVALID_ARG);
    assert(admitted_ops.open_app(&context, (mosaico_diagnostics_app_t)INT32_MAX) == ESP_ERR_INVALID_ARG);
    assert(queue_calls == 4 && activity_calls == 4);
    assert(mosaic_ui_open_app(NULL) == ESP_ERR_INVALID_ARG);
    assert(mosaic_ui_open_app("") == ESP_ERR_INVALID_ARG);
    assert(mosaic_ui_open_app("../settings") == ESP_ERR_NOT_FOUND);
    assert(mosaic_ui_open_app("/spiffs/arbitrary.lua") == ESP_ERR_NOT_FOUND);
    assert(queue_calls == 4 && activity_calls == 4);
    mosaic_app_registry_count = 0;
    assert(admitted_ops.open_app(&context, apps[0]) == ESP_ERR_NOT_FOUND);
    mosaic_app_registry_count = 4;
    atomic_store(&s_started, false);
    assert(admitted_ops.open_app(&context, apps[0]) == ESP_ERR_INVALID_STATE);
    atomic_store(&s_started, true);
    atomic_store(&s_hub_presenter_active, false);
    assert(admitted_ops.open_app(&context, apps[0]) == ESP_ERR_INVALID_STATE);
    atomic_store(&s_hub_presenter_active, true);
    assert(queue_calls == 4 && activity_calls == 4);
    queue_full = true;
    assert(admitted_ops.open_app(&context, apps[0]) == ESP_ERR_TIMEOUT);
    assert(queue_calls == 5); /* No queue-space retry or synchronous startup. */
    queue_full = false;
    atomic_store(&s_capture_in_progress, true);
    assert(admitted_ops.open_app(&context, apps[0]) == ESP_OK);
    assert(atomic_load(&s_capture_interrupted));
    assert(queue_calls == 6 && queued.data.app == &settings);
    s_command_queue = NULL;
    assert(admitted_ops.open_app(&context, apps[0]) == ESP_ERR_INVALID_STATE);
    assert(queue_calls == 6);
    start_result = ESP_ERR_NO_MEM;
    assert(mosaico_diagnostics_platform_start(&context) == ESP_ERR_NO_MEM);
    assert(starts == 2);
    puts("Diagnostics open: real enum adapter/ops/registry/UI/queue; fixed routes and nonblocking admission passed");
#else
    assert(starts == 0);
    puts("Diagnostics open: disabled configuration does not start bridge");
#endif
}
'''

# Supply only the settings-service handle to the actual platform public header.
# The diagnostic protocol header and function bodies are the production sources.
(output / "app_settings_service.h").write_text(
    "#pragma once\ntypedef void *app_settings_service_handle_t;\n")
test_source = output / "test-open.c"
test_source.write_text(test)
for enabled in (0, 1):
    executable = output / f"test-open-{enabled}"
    subprocess.run([
        os.environ.get("CC", "clang"), "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", f"-DCONFIG_MOSAICO_USB_DIAGNOSTICS={enabled}",
        "-I", str(output), "-I", str(project / "tests/diagnostics_stubs"),
        "-I", str(project / "overlay/components/mosaico_diagnostics/include"),
        "-I", str(project / "overlay/main/platform"), str(test_source),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)

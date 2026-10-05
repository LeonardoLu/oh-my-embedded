/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 * Liquid styles use the separately licensed liquidduck/ implementation.
 */

#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "liquid_actions.h"
#include "liquid_binds.h"
#include "liquid_objects.h"
#include "mosaic_app_catalog.h"
#include "mosaic_imu.h"
#include "mosaic_liquid.h"
#include "mosaic_runtime.h"

_Static_assert(GSP_ACT_ID_LIQUID_RETURN != 0 &&
               GSP_ACT_ID_LIQUID_RETURN != MOSAIC_APP_NO_LAUNCH_ACTION,
               "The runtime Back action must have a nonzero scene action ID");

typedef struct liquid_frames liquid_frames_t;
typedef struct {
    liquid_frames_t *owner;
    uint16_t *pixels;
    atomic_bool busy;
} liquid_frame_t;

struct liquid_frames {
    atomic_uint references;
    liquid_frame_t frames[2];
};

static liquid_engine_style_t s_requested_style;
static liquid_engine_style_t s_style;
static unsigned s_palette;
static int64_t s_last_tick;
static liquid_engine_t *s_liquid;
static liquid_frames_t *s_frames;

static void frames_release(liquid_frames_t *frames)
{
    if (frames != NULL && atomic_fetch_sub(&frames->references, 1) == 1) {
        free(frames->frames[0].pixels);
        free(frames->frames[1].pixels);
        free(frames);
    }
}

static void frame_returned(void *ctx)
{
    liquid_frame_t *frame = ctx;
    atomic_store(&frame->busy, false);
    frames_release(frame->owner);
}

static liquid_frames_t *frames_create(void)
{
    liquid_frames_t *frames = calloc(1, sizeof(*frames));
    if (frames == NULL) return NULL;
    atomic_init(&frames->references, 1);
    for (unsigned i = 0; i < 2; ++i) {
        frames->frames[i].owner = frames;
        atomic_init(&frames->frames[i].busy, false);
        frames->frames[i].pixels = malloc(LIQUID_WIDTH * LIQUID_HEIGHT * sizeof(uint16_t));
        if (frames->frames[i].pixels == NULL) {
            frames_release(frames);
            return NULL;
        }
    }
    return frames;
}

static void liquid_present(esp_gsp_handle_t ui)
{
    for (unsigned i = 0; i < 2; ++i) {
        liquid_frame_t *frame = &s_frames->frames[i];
        bool expected = false;
        if (!atomic_compare_exchange_strong(&frame->busy, &expected, true)) continue;
        atomic_fetch_add(&s_frames->references, 1);
        liquid_engine_render(s_liquid, s_palette, frame->pixels, LIQUID_WIDTH);
        const esp_gsp_err_t result = esp_gsp_canvas_try_push(ui, GSP_BIND_LIQUID_CANVAS,
            frame->pixels, LIQUID_WIDTH * sizeof(uint16_t), frame_returned, frame);
        if (result != ESP_GSP_OK) {
            frame_returned(frame);
            if (result != ESP_GSP_ERR_TIMEOUT) (void)esp_gsp_set_text(ui, GSP_BIND_LIQUID_STATUS, "Display unavailable");
        }
        return;
    }
    /* Both frames are borrowed. Skip presentation until GSP returns one. */
}

static const char *const s_style_names[] = {"Pixel", "Gradient", "Water"};

bool mosaic_liquid_select_style(liquid_engine_style_t style)
{
    if (style < LIQUID_PIXEL || style > LIQUID_WATER) return false;
    s_requested_style = style;
    return true;
}

static bool refill(esp_gsp_handle_t ui)
{
    liquid_engine_t *liquid = liquid_engine_create(s_style);
    liquid_frames_t *frames = s_frames != NULL ? s_frames : frames_create();
    if (liquid == NULL || frames == NULL) {
        liquid_engine_destroy(liquid);
        if (s_frames == NULL) frames_release(frames);
        (void)esp_gsp_set_text(ui, GSP_BIND_LIQUID_STATUS, "Not enough memory");
        return false;
    }
    liquid_engine_destroy(s_liquid);
    s_liquid = liquid;
    s_frames = frames;
    s_last_tick = 0;
    (void)esp_gsp_set_text(ui, GSP_BIND_LIQUID_STATUS, "");
    liquid_present(ui);
    return true;
}

static void liquid_started(esp_gsp_handle_t ui)
{
    s_style = s_requested_style;
    s_palette = 5;
    (void)esp_gsp_set_text(ui, GSP_BIND_LIQUID_TITLE, s_style_names[s_style]);
    (void)esp_gsp_set_text(ui, GSP_BIND_LIQUID_PALETTE_NAME, liquid_engine_palette_name(s_palette));
    (void)refill(ui);
}

static void liquid_stopping(esp_gsp_handle_t ui)
{
    (void)esp_gsp_canvas_stop(ui, GSP_BIND_LIQUID_CANVAS);
    liquid_engine_destroy(s_liquid);
    s_liquid = NULL;
    frames_release(s_frames);
    s_frames = NULL;
}

static void liquid_event(esp_gsp_handle_t ui, const struct mosaic_event *event)
{
    if (event == NULL) return;
    if (event->type == MOSAIC_EVENT_UI_CALL) {
        if (event->data.call.action_id == GSP_ACT_ID_LIQUID_THEME && s_liquid != NULL) {
            s_palette = (s_palette + 1) % LIQUID_PALETTES;
            (void)esp_gsp_set_text(ui, GSP_BIND_LIQUID_PALETTE_NAME, liquid_engine_palette_name(s_palette));
            liquid_present(ui);
        } else if (event->data.call.action_id == GSP_ACT_ID_LIQUID_RESET) {
            (void)refill(ui);
        }
    } else if (event->type == MOSAIC_EVENT_TIMER && s_liquid != NULL) {
        const float dt = s_last_tick > 0 ? (event->timestamp_us - s_last_tick) / 1000000.0f : 1.0f / 30.0f;
        s_last_tick = event->timestamp_us;
        float gx, gy;
        if (mosaic_imu_get_gravity(&gx, &gy) != ESP_OK || !isfinite(gx) || !isfinite(gy)) {
            (void)esp_gsp_set_text(ui, GSP_BIND_LIQUID_STATUS, "IMU unavailable");
            return;
        }
        (void)esp_gsp_set_text(ui, GSP_BIND_LIQUID_STATUS, "");
        liquid_engine_step(s_liquid, dt, gx, gy);
        liquid_present(ui);
    }
}

const mosaic_app_descriptor_t mosaic_liquid_app = {
    .id = 14,
    .launch_action = MOSAIC_APP_NO_LAUNCH_ACTION,
    .back_action = GSP_ACT_ID_LIQUID_RETURN,
    .name = "liquid",
    .title = "Liquid",
    .directory = &gsp_obj_directory_liquid,
    .disable_swipe = true,
    .root_header_in_stack = true,
    .on_started = liquid_started,
    .on_stopping = liquid_stopping,
    .on_event = liquid_event,
};

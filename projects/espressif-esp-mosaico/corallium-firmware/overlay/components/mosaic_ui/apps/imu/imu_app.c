/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 * Liquid styles use the separately licensed liquidduck/ implementation.
 */

#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imu_actions.h"
#include "imu_binds.h"
#include "imu_liquid.h"
#include "imu_objects.h"
#include "mosaic_app_catalog.h"
#include "mosaic_demo.h"
#include "mosaic_hub_actions.h"
#include "mosaic_imu.h"
#include "mosaic_runtime.h"

typedef struct imu_frames imu_frames_t;
typedef struct {
    imu_frames_t *owner;
    uint16_t *pixels;
    atomic_bool busy;
} imu_frame_t;

struct imu_frames {
    atomic_uint references;
    imu_frame_t frames[2];
};

static mosaic_imu_ops_t s_imu_ops;
static unsigned s_style;
static unsigned s_palette;
static int64_t s_last_tick;
static imu_liquid_t *s_liquid;
static imu_frames_t *s_frames;

static void frames_release(imu_frames_t *frames)
{
    if (frames != NULL && atomic_fetch_sub(&frames->references, 1) == 1) {
        free(frames->frames[0].pixels);
        free(frames->frames[1].pixels);
        free(frames);
    }
}

static void frame_returned(void *ctx)
{
    imu_frame_t *frame = ctx;
    atomic_store(&frame->busy, false);
    frames_release(frame->owner);
}

static imu_frames_t *frames_create(void)
{
    imu_frames_t *frames = calloc(1, sizeof(*frames));
    if (frames == NULL) return NULL;
    atomic_init(&frames->references, 1);
    for (unsigned i = 0; i < 2; ++i) {
        frames->frames[i].owner = frames;
        atomic_init(&frames->frames[i].busy, false);
        frames->frames[i].pixels = malloc(IMU_LIQUID_WIDTH * IMU_LIQUID_HEIGHT * sizeof(uint16_t));
        if (frames->frames[i].pixels == NULL) {
            frames_release(frames);
            return NULL;
        }
    }
    return frames;
}

static void fluid_present(esp_gsp_handle_t ui)
{
    for (unsigned i = 0; i < 2; ++i) {
        imu_frame_t *frame = &s_frames->frames[i];
        bool expected = false;
        if (!atomic_compare_exchange_strong(&frame->busy, &expected, true)) continue;
        atomic_fetch_add(&s_frames->references, 1);
        imu_liquid_render(s_liquid, s_palette, frame->pixels, IMU_LIQUID_WIDTH);
        const esp_gsp_err_t result = esp_gsp_canvas_try_push(ui, GSP_BIND_IMU_LIQUID_CANVAS,
            frame->pixels, IMU_LIQUID_WIDTH * sizeof(uint16_t), frame_returned, frame);
        if (result != ESP_GSP_OK) {
            frame_returned(frame);
            if (result != ESP_GSP_ERR_TIMEOUT) (void)esp_gsp_set_text(ui, GSP_BIND_IMU_STATUS, "Display unavailable");
        }
        return;
    }
    /* Both frames are borrowed. Skip presentation until GSP returns one. */
}

static int32_t imu_axis_position(float degrees, bool invert,
                                 int32_t minimum, int32_t maximum)
{
    float value = invert ? -degrees : degrees;
    if (value < -30.0f) value = -30.0f;
    if (value > 30.0f) value = 30.0f;
    return minimum + (int32_t)((value + 30.0f) * (maximum - minimum) / 60.0f + 0.5f);
}

esp_err_t mosaic_imu_configure(const mosaic_imu_ops_t *ops)
{
    if (ops != NULL && ops->read == NULL) return ESP_ERR_INVALID_ARG;
    if (ops == NULL) memset(&s_imu_ops, 0, sizeof(s_imu_ops));
    else s_imu_ops = *ops;
    return ESP_OK;
}

static void level_tick(esp_gsp_handle_t ui)
{
    if (s_imu_ops.read == NULL) {
        mosaic_demo_tick(ui, MOSAIC_DEMO_IMU);
        return;
    }
    mosaic_imu_sample_t sample = {0};
    if (s_imu_ops.read(&sample, s_imu_ops.user_ctx) != ESP_OK ||
        !isfinite(sample.pitch_deg) || !isfinite(sample.roll_deg) || !isfinite(sample.yaw_deg)) return;
    char angle[16], pitch[16], roll[16], yaw[16];
    snprintf(angle, sizeof(angle), "%.0f°", hypotf(sample.pitch_deg, sample.roll_deg));
    snprintf(pitch, sizeof(pitch), "%.0f", sample.pitch_deg);
    snprintf(roll, sizeof(roll), "%.0f", sample.roll_deg);
    snprintf(yaw, sizeof(yaw), "%.0f", sample.yaw_deg);
    (void)gsp_imu_imu_bubble_set_position(ui,
        imu_axis_position(sample.roll_deg, false, 130, 270),
        imu_axis_position(sample.pitch_deg, true, 90, 230));
    (void)esp_gsp_set_text(ui, GSP_BIND_IMU_ANGLE, angle);
    (void)esp_gsp_set_text(ui, GSP_BIND_IMU_PITCH, pitch);
    (void)esp_gsp_set_text(ui, GSP_BIND_IMU_ROLL, roll);
    (void)esp_gsp_set_text(ui, GSP_BIND_IMU_YAW, yaw);
}

static bool select_style(esp_gsp_handle_t ui, unsigned style)
{
    static const char *const names[] = {"Level", "Pixel", "Gradient", "Water"};
    if (style > 3) return false;
    if (style != 0) {
        imu_liquid_t *liquid = imu_liquid_create((imu_liquid_style_t)(style - 1));
        imu_frames_t *frames = s_frames != NULL ? s_frames : frames_create();
        if (liquid == NULL || frames == NULL) {
            imu_liquid_destroy(liquid);
            if (s_frames == NULL) frames_release(frames);
            (void)esp_gsp_set_text(ui, GSP_BIND_IMU_STATUS, "Not enough memory");
            return false;
        }
        imu_liquid_destroy(s_liquid);
        s_liquid = liquid;
        s_frames = frames;
    } else {
        (void)esp_gsp_canvas_stop(ui, GSP_BIND_IMU_LIQUID_CANVAS);
        imu_liquid_destroy(s_liquid);
        s_liquid = NULL;
        frames_release(s_frames);
        s_frames = NULL;
    }
    s_style = style;
    s_last_tick = 0;
    (void)esp_gsp_set_text(ui, GSP_BIND_IMU_STATUS, "");
    (void)esp_gsp_set_text(ui, GSP_BIND_IMU_STYLE_NAME, names[style]);
    (void)esp_gsp_set_text(ui, GSP_BIND_IMU_PALETTE_NAME, imu_liquid_palette_name(s_palette));
    (void)esp_gsp_set_visible(ui, GSP_BIND_IMU_LEVEL_VISIBLE, style == 0);
    (void)esp_gsp_set_visible(ui, GSP_BIND_IMU_FLUID_VISIBLE, style != 0);
    (void)esp_gsp_set_visible(ui, GSP_BIND_IMU_FLUID_CONTROLS_VISIBLE, style != 0);
    if (style == 0) level_tick(ui);
    else fluid_present(ui);
    return true;
}

static void imu_started(esp_gsp_handle_t ui)
{
    s_palette = 5;
    (void)select_style(ui, 0);
}

static void imu_stopping(esp_gsp_handle_t ui)
{
    (void)esp_gsp_canvas_stop(ui, GSP_BIND_IMU_LIQUID_CANVAS);
    imu_liquid_destroy(s_liquid);
    s_liquid = NULL;
    frames_release(s_frames);
    s_frames = NULL;
    s_style = 0;
}

static void imu_event(esp_gsp_handle_t ui, const struct mosaic_event *event)
{
    if (event == NULL) return;
    if (event->type == MOSAIC_EVENT_UI_CALL) {
        if (event->data.call.action_id == GSP_ACT_ID_IMU_STYLE) {
            (void)select_style(ui, (s_style + 1) % 4);
        } else if (s_style != 0 && event->data.call.action_id == GSP_ACT_ID_IMU_THEME) {
            s_palette = (s_palette + 1) % IMU_LIQUID_PALETTES;
            (void)esp_gsp_set_text(ui, GSP_BIND_IMU_PALETTE_NAME, imu_liquid_palette_name(s_palette));
            fluid_present(ui);
        } else if (s_style != 0 && event->data.call.action_id == GSP_ACT_ID_IMU_RESET) {
            (void)select_style(ui, s_style);
        }
    } else if (event->type == MOSAIC_EVENT_TIMER) {
        if (s_style == 0) {
            level_tick(ui);
            return;
        }
        const float dt = s_last_tick > 0 ? (event->timestamp_us - s_last_tick) / 1000000.0f : 1.0f / 30.0f;
        s_last_tick = event->timestamp_us;
        float gx = 0, gy = 1;
        if (s_imu_ops.read_accel != NULL) {
            if (s_imu_ops.read_accel(&gx, &gy, s_imu_ops.user_ctx) != ESP_OK ||
                !isfinite(gx) || !isfinite(gy)) {
                (void)esp_gsp_set_text(ui, GSP_BIND_IMU_STATUS, "IMU unavailable");
                return;
            }
        } else if (s_imu_ops.read != NULL) {
            mosaic_imu_sample_t sample = {0};
            if (s_imu_ops.read(&sample, s_imu_ops.user_ctx) != ESP_OK) return;
            gx = sinf(sample.roll_deg * 0.0174532925f);
            gy = -sinf(sample.pitch_deg * 0.0174532925f);
        }
        (void)esp_gsp_set_text(ui, GSP_BIND_IMU_STATUS, "");
        imu_liquid_step(s_liquid, dt, gx, gy);
        fluid_present(ui);
    }
}

const mosaic_app_descriptor_t mosaic_imu_app = {
    .id = 2,
    .launch_action = GSP_ACT_ID_APP_IMU,
    .back_action = MOSAIC_APP_SHELL_BACK_ACTION,
    .name = "imu",
    .title = "IMU",
    .directory = &gsp_obj_directory_imu,
    .disable_swipe = true,
    .on_started = imu_started,
    .on_stopping = imu_stopping,
    .on_event = imu_event,
};

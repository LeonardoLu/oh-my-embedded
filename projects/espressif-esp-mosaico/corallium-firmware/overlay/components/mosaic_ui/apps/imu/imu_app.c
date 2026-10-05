/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "imu_binds.h"
#include "imu_objects.h"
#include "mosaic_app_catalog.h"
#include "mosaic_demo.h"
#include "mosaic_hub_actions.h"
#include "mosaic_imu.h"
#include "mosaic_runtime.h"

static mosaic_imu_ops_t s_imu_ops;

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


esp_err_t mosaic_imu_get_gravity(float *x_g, float *y_g)
{
    if (s_imu_ops.read_accel != NULL) {
        return s_imu_ops.read_accel(x_g, y_g, s_imu_ops.user_ctx);
    }
    if (s_imu_ops.read != NULL) {
        mosaic_imu_sample_t sample = {0};
        esp_err_t err = s_imu_ops.read(&sample, s_imu_ops.user_ctx);
        if (err != ESP_OK) return err;
        *x_g = -sinf(sample.roll_deg * 0.0174532925f);
        *y_g = sinf(sample.pitch_deg * 0.0174532925f);
    } else {
        *x_g = 0;
        *y_g = 1;
    }
    return ESP_OK;
}

static void imu_started(esp_gsp_handle_t ui)
{
    level_tick(ui);
}

static void imu_event(esp_gsp_handle_t ui, const struct mosaic_event *event)
{
    if (event != NULL && event->type == MOSAIC_EVENT_TIMER) level_tick(ui);
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
    .on_event = imu_event,
};

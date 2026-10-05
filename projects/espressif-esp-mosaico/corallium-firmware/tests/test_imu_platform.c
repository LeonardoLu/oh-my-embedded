// SPDX-License-Identifier: MIT
/* Exercise the real adapter with a Board Manager-owned BMI270 sample. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bmi2.h"
#include "esp_board_manager.h"
#include "mosaic_imu.h"
#include "mosaic_imu_platform.h"

static mosaic_imu_ops_t ops;
static struct bmi2_sens_data raw;
static int device, init_calls, read_calls;
static int64_t now_us = 1000000;
static esp_err_t init_error = ESP_FAIL, read_error;

int64_t esp_timer_get_time(void) { return now_us; }
bool esp_board_manager_check_name(const char *name)
{
    assert(strcmp(name, "imu_sensor") == 0);
    return true;
}
esp_err_t esp_board_manager_init_device_by_name(const char *name)
{
    assert(strcmp(name, "imu_sensor") == 0);
    ++init_calls;
    return init_error;
}
esp_err_t esp_board_manager_get_device_handle(const char *name, void **handle)
{
    assert(strcmp(name, "imu_sensor") == 0);
    *handle = &device;
    return ESP_OK;
}
esp_err_t esp_mosaico_imu_read(void *handle, struct bmi2_sens_data *sample)
{
    assert(handle == &device);
    ++read_calls;
    if (read_error != ESP_OK) return read_error;
    *sample = raw;
    return ESP_OK;
}
esp_err_t mosaic_imu_configure(const mosaic_imu_ops_t *configured)
{
    ops = *configured;
    return ESP_OK;
}
static void near(float actual, float expected)
{
    assert(fabsf(actual - expected) < .0001f);
}

int main(void)
{
    assert(mosaic_imu_platform_init() == ESP_OK);
    assert(ops.read != NULL && ops.read_accel != NULL);
    float x = 9, y = 9;
    assert(ops.read_accel(&x, &y, ops.user_ctx) == ESP_FAIL);
    assert(init_calls == 1 && read_calls == 0);
    near(x, 9); near(y, 9);
    init_error = ESP_OK;
    now_us += 4999999;
    assert(ops.read_accel(&x, &y, ops.user_ctx) == ESP_FAIL);
    assert(init_calls == 1 && read_calls == 0);
    ++now_us;
    raw.acc.x = 8192;
    raw.acc.y = -16384;
    assert(ops.read_accel(&x, &y, ops.user_ctx) == ESP_OK);
    near(x, -.5f); near(y, -1);
    assert(init_calls == 2 && read_calls == 1);

    /* Right/down gravity opposes the BMI270 support force, USB down. */
    raw.acc.x = -16384;
    raw.acc.y = 8192;
    assert(ops.read_accel(&x, &y, ops.user_ctx) == ESP_OK);
    near(x, 1); near(y, .5f);
    raw.acc.x = 0;
    raw.acc.y = 16384; /* Vertical with USB down: liquid falls down. */
    assert(ops.read_accel(&x, &y, ops.user_ctx) == ESP_OK);
    near(x, 0); near(y, 1);
    raw.acc.x = INT16_MAX;
    raw.acc.y = INT16_MIN;
    assert(ops.read_accel(&x, &y, ops.user_ctx) == ESP_OK);
    near(x, -32767.0f / 16384.0f); near(y, -2);

    read_error = ESP_FAIL;
    x = 7; y = 8;
    assert(ops.read_accel(&x, &y, ops.user_ctx) == ESP_FAIL);
    near(x, 7); near(y, 8);
    read_error = ESP_OK;
    memset(&raw, 0, sizeof(raw));
    raw.acc.z = 16384;
    mosaic_imu_sample_t orientation;
    assert(ops.read(&orientation, ops.user_ctx) == ESP_OK);
    near(orientation.pitch_deg, 0);
    near(orientation.roll_deg, 0);
    assert(ops.read_accel(&x, &y, ops.user_ctx) == ESP_OK);
    near(x, 0); near(y, 0);
    assert(init_calls == 2);
    puts("IMU adapter: shared device, g scaling, axes, retry and read errors verified");
    return 0;
}

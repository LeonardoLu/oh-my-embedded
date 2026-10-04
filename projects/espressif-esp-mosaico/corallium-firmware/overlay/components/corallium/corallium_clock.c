// SPDX-License-Identifier: MIT
#include "corallium.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "esp_attr.h"
#include "esp_system.h"
#include "esp_sntp.h"
#include "nvs.h"

#define CLOCK_MAGIC 0xC07A1101U
/* IDF's RTC + high resolution time implementation preserves the boot offset
 * across resets except power-on. Never restore a wall-clock checkpoint from NVS. */
static RTC_NOINIT_ATTR uint32_t retained_magic;
static int offset_min;
static const char *source = "unset";
static bool sntp_started;

bool corallium_time_valid(void) {
    time_t now = time(NULL);
    return retained_magic == CLOCK_MAGIC && now >= 1577836800LL && now < 4102444800LL;
}
static void apply_offset(int minutes) {
    offset_min = minutes;
    char tz[24];
    int magnitude = abs(minutes);
    snprintf(tz, sizeof(tz), "UTC%s%d:%02d", minutes >= 0 ? "-" : "+", magnitude / 60, magnitude % 60);
    setenv("TZ", tz, 1);
    tzset();
}
static void network_time(struct timeval *tv) {
    if (tv->tv_sec >= 1577836800LL && tv->tv_sec < 4102444800LL) {
        retained_magic = CLOCK_MAGIC;
        source = "network";
    }
}
void corallium_clock_init(void) {
    esp_reset_reason_t reset = esp_reset_reason();
    if (reset == ESP_RST_POWERON || reset == ESP_RST_BROWNOUT || !corallium_time_valid()) {
        retained_magic = 0;
        source = "unset";
    } else {
        source = "rtc";
    }
    nvs_handle_t handle;
    int16_t saved_offset = 0;
    if (nvs_open("corallium", NVS_READONLY, &handle) == ESP_OK) {
        (void)nvs_get_i16(handle, "utc_offset", &saved_offset);
        nvs_close(handle);
    }
    apply_offset(saved_offset >= -720 && saved_offset <= 840 ? saved_offset : 0);
}
void corallium_network_changed(bool connected) {
    if (!connected || sntp_started) return;
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(network_time);
    esp_sntp_set_sync_interval(6 * 60 * 60 * 1000);
    esp_sntp_init();
    sntp_started = true;
}
cJSON *corallium_time_json(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    bool valid = corallium_time_valid();
    cJSON *result = cJSON_CreateObject();
    if (valid) cJSON_AddNumberToObject(result, "unix_ms", (double)tv.tv_sec * 1000.0 + tv.tv_usec / 1000);
    else cJSON_AddNullToObject(result, "unix_ms");
    cJSON_AddNumberToObject(result, "utc_offset_min", offset_min);
    cJSON_AddBoolToObject(result, "valid", valid);
    cJSON_AddStringToObject(result, "source", valid ? source : "unset");
    cJSON_AddStringToObject(result, "quality", !valid ? "unset" : source[0] == 'r' ? "estimated" : "synchronized");
    return result;
}
esp_err_t corallium_set_time(int64_t unix_ms, int minutes) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("corallium", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_i16(handle, "utc_offset", minutes);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) return err;
    struct timeval tv = {.tv_sec = unix_ms / 1000, .tv_usec = (unix_ms % 1000) * 1000};
    if (settimeofday(&tv, NULL) != 0) return ESP_FAIL;
    apply_offset(minutes);
    retained_magic = CLOCK_MAGIC;
    source = "app";
    return ESP_OK;
}

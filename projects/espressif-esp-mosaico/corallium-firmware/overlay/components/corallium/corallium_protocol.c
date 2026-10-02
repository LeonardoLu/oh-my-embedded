// SPDX-License-Identifier: MIT
#include "corallium.h"
#include "corallium_metrics.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "app_config.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "mosaic_settings.h"
#include "wifi_manager.h"

extern cJSON *corallium_time_json(void);
extern esp_err_t corallium_set_time(int64_t unix_ms, int offset);
static cJSON *error(cJSON *response, const char *code, const char *message) {
    cJSON_AddBoolToObject(response, "ok", false);
    cJSON *item = cJSON_AddObjectToObject(response, "error");
    cJSON_AddStringToObject(item, "code", code);
    cJSON_AddStringToObject(item, "message", message);
    return response;
}
static void nullable_string(cJSON *object, const char *key, const char *value) {
    if (value && value[0]) cJSON_AddStringToObject(object, key, value);
    else cJSON_AddNullToObject(object, key);
}
static const char *wifi_state(const wifi_manager_status_t *w) {
    if (w->sta_connected) return "connected";
    if (w->state == WIFI_MANAGER_STATE_CONNECTING || w->state == WIFI_MANAGER_STATE_RETRY_WAIT) return "connecting";
    if (w->state == WIFI_MANAGER_STATE_FAILED || w->state == WIFI_MANAGER_STATE_AUTH_FAILED || w->state == WIFI_MANAGER_STATE_AP_NOT_FOUND) return "failed";
    return "disconnected";
}
cJSON *corallium_status(void) {
    cJSON *result = cJSON_CreateObject();
    cJSON_AddItemToObject(result, "time", corallium_time_json());
    wifi_manager_status_t w = {0};
    wifi_manager_get_status(&w);
    cJSON *wifi = cJSON_AddObjectToObject(result, "wifi");
    cJSON_AddStringToObject(wifi, "state", wifi_state(&w));
    nullable_string(wifi, "ssid", w.sta_ssid);
    nullable_string(wifi, "ip", w.sta_connected ? w.sta_ip : NULL);
    if (w.sta_connected) cJSON_AddNumberToObject(wifi, "rssi", w.rssi);
    else cJSON_AddNullToObject(wifi, "rssi");
    cJSON *battery = cJSON_AddObjectToObject(result, "battery");
    mosaic_settings_battery_t b = {0};
    (void)mosaic_settings_get_battery(&b);
    if (b.available) {
        cJSON_AddNumberToObject(battery, "percent", b.state_of_charge);
        cJSON_AddBoolToObject(battery, "charging", b.charging);
        cJSON_AddNumberToObject(battery, "millivolts", b.voltage_mv);
        if (!b.charging && b.current_ma < -2) cJSON_AddNumberToObject(battery, "power_mw", corallium_cell_power_mw(b.voltage_mv, b.current_ma));
        else cJSON_AddNullToObject(battery, "power_mw");
    } else {
        cJSON_AddNullToObject(battery, "percent");
        cJSON_AddNullToObject(battery, "charging");
        cJSON_AddNullToObject(battery, "millivolts");
        cJSON_AddNullToObject(battery, "power_mw");
    }
    if (b.available && corallium_runtime_available(b.charging, b.current_ma, b.time_to_empty_min))
        cJSON_AddNumberToObject(battery, "runtime_min", b.time_to_empty_min);
    else cJSON_AddNullToObject(battery, "runtime_min");
    cJSON_AddNumberToObject(result, "uptime_ms", esp_timer_get_time() / 1000);
    return result;
}
static cJSON *info(void) {
    uint8_t mac[6] = {0};
    (void)esp_read_mac(mac, ESP_MAC_BT);
    char id[32];
    snprintf(id, sizeof(id), "mosaico-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "device_id", id);
    cJSON_AddStringToObject(result, "model", "espressif-esp-mosaico");
    cJSON_AddStringToObject(result, "name", "ESP-Mosaico");
    cJSON_AddStringToObject(result, "firmware", "factory-derivative/0.1.0");
    const char *caps[] = {"time.set", "wifi.set", "wifi.forget", "battery", "power", "runtime"};
    const char *channels[] = {"ble"};
    cJSON_AddItemToObject(result, "capabilities", cJSON_CreateStringArray(caps, 6));
    cJSON_AddItemToObject(result, "channels", cJSON_CreateStringArray(channels, 1));
    return result;
}
static bool integer(const cJSON *item, double low, double high) {
    return cJSON_IsNumber(item) && isfinite(item->valuedouble) && item->valuedouble >= low && item->valuedouble <= high && floor(item->valuedouble) == item->valuedouble;
}
static bool no_duplicates(const cJSON *obj) {
    const cJSON *a;
    cJSON_ArrayForEach(a, obj) {
        for (const cJSON *b = a->next; b; b = b->next)
            if (a->string && b->string && !strcmp(a->string, b->string)) return false;
    }
    return true;
}
static bool valid_id(const cJSON *id) {
    if (!cJSON_IsString(id) || !id->valuestring[0] || strlen(id->valuestring) > 64) return false;
    for (const unsigned char *p = (const unsigned char *)id->valuestring; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '.' || *p == '_' || *p == '-')) return false;
    return true;
}
cJSON *corallium_dispatch(const cJSON *request) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(request, "id");
    const cJSON *op = cJSON_GetObjectItemCaseSensitive(request, "op");
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(request, "v");
    const cJSON *payload = cJSON_GetObjectItemCaseSensitive(request, "payload");
    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "v", 1);
    cJSON_AddStringToObject(response, "id", valid_id(id) ? id->valuestring : "invalid");
    cJSON_AddStringToObject(response, "op", cJSON_IsString(op) && op->valuestring[0] && strlen(op->valuestring) <= 64 ? op->valuestring : "invalid");
    if (!cJSON_IsObject(request) || !no_duplicates(request) || !integer(v, 1, 1) || !valid_id(id) ||
        !cJSON_IsString(op) || !op->valuestring[0] || strlen(op->valuestring) > 64 ||
        !cJSON_IsObject(payload) || !no_duplicates(payload) ||
        cJSON_HasObjectItem(request, "ok") || cJSON_HasObjectItem(request, "error"))
        return error(response, "invalid_request", "Invalid v1 request");
    if ((!strcmp(op->valuestring, "device.info") || !strcmp(op->valuestring, "device.status") ||
         !strcmp(op->valuestring, "wifi.forget")) && payload->child)
        return error(response, "invalid_request", "Operation requires an empty payload");
    cJSON *result = NULL;
    if (!strcmp(op->valuestring, "device.info")) result = info();
    else if (!strcmp(op->valuestring, "device.status")) result = corallium_status();
    else if (!strcmp(op->valuestring, "time.set")) {
        const cJSON *t = cJSON_GetObjectItemCaseSensitive(payload, "unix_ms");
        const cJSON *z = cJSON_GetObjectItemCaseSensitive(payload, "utc_offset_min");
        if (!integer(t, 1577836800000LL, 4102444799999LL) || !integer(z, -720, 840))
            return error(response, "invalid_request", "Time or UTC offset is out of range");
        if (corallium_set_time((int64_t)t->valuedouble, (int)z->valuedouble) != ESP_OK)
            return error(response, "internal", "Unable to apply time");
        result = corallium_time_json();
    } else if (!strcmp(op->valuestring, "wifi.set") || !strcmp(op->valuestring, "wifi.forget")) {
        bool forget = !strcmp(op->valuestring, "wifi.forget");
        const cJSON *ssid = cJSON_GetObjectItemCaseSensitive(payload, "ssid");
        const cJSON *password = cJSON_GetObjectItemCaseSensitive(payload, "password");
        if (!forget && (!cJSON_IsString(ssid) || !cJSON_IsString(password) || strlen(ssid->valuestring) < 1 || strlen(ssid->valuestring) > 32 || (strlen(password->valuestring) != 0 && (strlen(password->valuestring) < 8 || strlen(password->valuestring) > 63))))
            return error(response, "invalid_request", "Invalid Wi-Fi credential lengths");
        app_config_t *config = calloc(1, sizeof(*config));
        if (!config) return error(response, "internal", "Out of memory");
        esp_err_t err = app_config_load(config);
        if (err == ESP_OK) {
            strlcpy(config->wifi_ssid, forget ? "" : ssid->valuestring, sizeof(config->wifi_ssid));
            strlcpy(config->wifi_password, forget ? "" : password->valuestring, sizeof(config->wifi_password));
            strlcpy(config->ap_behavior, "close_on_sta", sizeof(config->ap_behavior));
            err = app_config_save(config);
        }
        if (err == ESP_OK) {
            const wifi_manager_config_t next = {.sta_ssid = config->wifi_ssid, .sta_password = config->wifi_password, .ap_behavior = "close_on_sta"};
            err = wifi_manager_apply_sta_config(&next);
            if (err == ESP_OK) err = mosaic_settings_set_wifi_enabled(!forget);
        }
        memset(config, 0, sizeof(*config));
        free(config);
        if (err != ESP_OK) return error(response, "internal", "Unable to apply Wi-Fi settings");
        result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "state", forget ? "disconnected" : "connecting");
    } else return error(response, "unsupported", "Operation is not supported");
    cJSON_AddBoolToObject(response, "ok", true);
    cJSON_AddItemToObject(response, "payload", result);
    return response;
}

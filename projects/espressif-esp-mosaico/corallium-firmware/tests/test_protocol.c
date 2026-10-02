// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "corallium.h"
#include "app_config.h"
#include "wifi_manager.h"
#include "mosaic_settings.h"
static app_config_t saved;
static unsigned saves, sets;
static bool charging, wifi_persisted;
static int save_result;
int app_config_load(app_config_t *out) { *out = saved; return 0; }
int app_config_save(const app_config_t *value) { if (save_result) return save_result; saved=*value; ++saves; return 0; }
void wifi_manager_get_status(wifi_manager_status_t *out) { *out=(wifi_manager_status_t){.state=WIFI_MANAGER_STATE_IDLE}; }
int wifi_manager_apply_sta_config(const wifi_manager_config_t *value) { (void)value; return 0; }
int wifi_manager_set_enabled(bool enabled) { (void)enabled; return 0; }
int mosaic_settings_set_wifi_enabled(bool enabled) { wifi_persisted=enabled; return 0; }
int esp_read_mac(uint8_t *out,int kind) { (void)kind; memset(out,1,6); return 0; }
int64_t esp_timer_get_time(void) { return 1000000; }
int mosaic_settings_get_battery(mosaic_settings_battery_t *b) { *b=(mosaic_settings_battery_t){true,charging,50,3700,-20,100}; return 0; }
cJSON *corallium_time_json(void) { cJSON *p=cJSON_CreateObject(); cJSON_AddBoolToObject(p,"valid",true); return p; }
int corallium_set_time(int64_t unix_ms,int offset) { (void)unix_ms; (void)offset; ++sets; return 0; }
static cJSON *request(const char *text, bool ok) {
    cJSON *input=cJSON_Parse(text); cJSON *out=corallium_dispatch(input); cJSON_Delete(input);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(out,"ok")) == ok); return out;
}
int main(void) {
    cJSON *r=request("{\"v\":1,\"id\":\"t1\",\"op\":\"time.set\",\"payload\":{\"unix_ms\":1577836800000,\"utc_offset_min\":-720}}",true); cJSON_Delete(r);
    r=request("{\"v\":1,\"id\":\"t2\",\"op\":\"time.set\",\"payload\":{\"unix_ms\":4102444800000,\"utc_offset_min\":0}}",false); cJSON_Delete(r);
    r=request("{\"v\":1,\"id\":\"t3\",\"op\":\"time.set\",\"payload\":{\"unix_ms\":1800000000000,\"utc_offset_min\":1.5}}",false); cJSON_Delete(r);
    r=request("{\"v\":2,\"id\":\"t4\",\"op\":\"time.set\",\"payload\":{\"unix_ms\":1800000000000,\"utc_offset_min\":0}}",false); cJSON_Delete(r);
    assert(sets==1);
    r=request("{\"v\":1,\"id\":\"w1\",\"op\":\"wifi.set\",\"payload\":{\"ssid\":\"12345678901234567890123456789012\",\"password\":\"\"}}",true);cJSON_Delete(r); assert(strlen(saved.wifi_ssid)==32 && saves==1 && wifi_persisted);
    r=request("{\"v\":1,\"id\":\"response\",\"op\":\"wifi.forget\",\"ok\":true,\"payload\":{}}",false); cJSON_Delete(r);
    r=request("{\"v\":1,\"id\":\"bad id\",\"op\":\"wifi.forget\",\"payload\":{}}",false); cJSON_Delete(r);
    r=request("{\"v\":1,\"id\":\"error\",\"op\":\"wifi.forget\",\"error\":{},\"payload\":{}}",false); cJSON_Delete(r);
    r=request("{\"v\":1,\"id\":\"payload\",\"op\":\"wifi.forget\",\"payload\":{\"ssid\":\"no\"}}",false); cJSON_Delete(r);
    assert(saves==1 && wifi_persisted);
    r=request("{\"v\":1,\"id\":\"w2\",\"op\":\"wifi.set\",\"payload\":{\"ssid\":\"valid\",\"password\":\"short\"}}",false);cJSON_Delete(r); assert(saves==1);
    r=request("{\"v\":1,\"v\":2,\"id\":\"w3\",\"op\":\"wifi.forget\",\"payload\":{}}",false); cJSON_Delete(r); assert(saves==1);
    save_result=-1;
    r=request("{\"v\":1,\"id\":\"w4\",\"op\":\"wifi.forget\",\"payload\":{}}",false); cJSON_Delete(r); assert(saved.wifi_ssid[0]);
    save_result=0;
    r=request("{\"v\":1,\"id\":\"w5\",\"op\":\"wifi.forget\",\"payload\":{}}",true); cJSON_Delete(r); assert(!saved.wifi_ssid[0] && !saved.wifi_password[0] && !wifi_persisted);
    r=corallium_status(); cJSON *b=cJSON_GetObjectItem(r,"battery"); assert(cJSON_GetObjectItem(b,"power_mw")->valuedouble==74); cJSON_Delete(r);
    charging=true; r=corallium_status(); b=cJSON_GetObjectItem(r,"battery"); assert(cJSON_IsNull(cJSON_GetObjectItem(b,"power_mw")) && cJSON_IsNull(cJSON_GetObjectItem(b,"runtime_min"))); cJSON_Delete(r);
    r=request("{\"v\":1,\"id\":\"i1\",\"op\":\"device.info\",\"payload\":{}}",true); assert(cJSON_GetArraySize(cJSON_GetObjectItem(cJSON_GetObjectItem(r,"payload"),"capabilities"))==6); cJSON_Delete(r);
    puts("native protocol validation, persistence failure, Wi-Fi boundaries and charging telemetry passed");
}

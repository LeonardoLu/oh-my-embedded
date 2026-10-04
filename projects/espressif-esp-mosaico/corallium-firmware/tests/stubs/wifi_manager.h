#pragma once
#include <stdbool.h>
#include "esp_err.h"
enum { WIFI_MANAGER_STATE_CONNECTING, WIFI_MANAGER_STATE_RETRY_WAIT, WIFI_MANAGER_STATE_FAILED, WIFI_MANAGER_STATE_AUTH_FAILED, WIFI_MANAGER_STATE_AP_NOT_FOUND, WIFI_MANAGER_STATE_IDLE };
typedef struct { bool sta_connected; int state, rssi; const char *sta_ssid, *sta_ip; } wifi_manager_status_t;
typedef struct { const char *sta_ssid, *sta_password, *ap_behavior; } wifi_manager_config_t;
void wifi_manager_get_status(wifi_manager_status_t *);
esp_err_t wifi_manager_apply_sta_config(const wifi_manager_config_t *);
esp_err_t wifi_manager_set_enabled(bool);

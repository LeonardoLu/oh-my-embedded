#pragma once
#include "esp_err.h"
typedef struct { char wifi_ssid[33], wifi_password[64], ap_behavior[16]; } app_config_t;
esp_err_t app_config_load(app_config_t *);
esp_err_t app_config_save(const app_config_t *);

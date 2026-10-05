// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
typedef struct {
    bool sta_connected;
    bool sta_configured;
    bool ap_active;
    const char *sta_ssid;
    const char *sta_ip;
    const char *ap_ip;
} wifi_manager_status_t;
void wifi_manager_get_status(wifi_manager_status_t *status);

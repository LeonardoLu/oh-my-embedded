// SPDX-License-Identifier: MIT
#pragma once

#include "app_settings_service.h"
#include "esp_err.h"

/** Start the USB-only diagnostics bridge after the UI and local providers. */
esp_err_t mosaico_diagnostics_platform_start(
    app_settings_service_handle_t settings);

// SPDX-License-Identifier: MIT
#include <stdlib.h>
#include <string.h>
#include "app_config.h"
#include "app_fs.h"
#include "app_settings_service.h"
#include "app_factory_reset.h"
#include "claw_paths.h"
#include "corallium.h"
#include "mosaico_audio.h"
#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "mosaico_bsp_bmgr_bridge.h"
#include "mosaic_settings_platform.h"
#include "mosaic_button_platform.h"
#include "mosaic_imu_platform.h"
#include "mosaic_ui.h"
#include "network_provisioning_service.h"
#include "nvs_flash.h"
#include "wifi_manager.h"
#include "weather_service.h"

static network_provisioning_service_handle_t network;
static const char *TAG = "mosaico_boot";
static esp_err_t save_config(const app_config_t *config) {
    esp_err_t err = app_config_validate_wifi(config, NULL);
    if (err == ESP_OK) err = app_config_save(config);
    if (err == ESP_OK) err = network_provisioning_service_reload_and_apply(network);
    return err;
}
static void network_changed(const wifi_manager_event_t *event, void *context) {
    const bool connected = event->sta_connected;
    (void)context;
    mosaic_settings_platform_notify_network_changed();
    corallium_network_changed(connected);
    if (connected) (void)esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
}
void app_main(void) {
    /* Erasing all NVS on a recoverable error would silently discard credentials. */
    ESP_ERROR_CHECK(nvs_flash_init());
    corallium_clock_init();
    ESP_ERROR_CHECK(app_config_init());
    bool reset_pending = false;
    ESP_ERROR_CHECK(app_factory_reset_is_pending(&reset_pending));
    if (reset_pending) {
        ESP_ERROR_CHECK(corallium_close_pairing());
        ESP_ERROR_CHECK(app_config_reset_all());
    }
    ESP_ERROR_CHECK(mosaico_bsp_bmgr_init());
    ESP_ERROR_CHECK(esp_board_manager_init());
    /* Audio output uses the official DAC/PA driver. GPIO60 powers the display
     * and must remain enabled independently of speaker or expansion usage. */
    esp_err_t audio_err = mosaico_audio_init();
    if (audio_err != ESP_OK) ESP_LOGW(TAG, "Speaker unavailable: %s", esp_err_to_name(audio_err));
    ESP_ERROR_CHECK(app_fs_init());
    if (reset_pending) {
        ESP_ERROR_CHECK(app_fs_factory_reset());
        ESP_ERROR_CHECK(weather_service_erase_cache());
        ESP_ERROR_CHECK(app_factory_reset_complete());
    }
    ESP_ERROR_CHECK(claw_paths_set(CLAW_PATH_DATA, app_fs_storage_base_path()));
    ESP_ERROR_CHECK(claw_paths_set(CLAW_PATH_SYSTEM, app_fs_system_base_path()));
    ESP_ERROR_CHECK(claw_paths_set_space_provider(CLAW_PATH_DATA, app_fs_get_storage_space, NULL));
    app_settings_service_handle_t settings;
    ESP_ERROR_CHECK(app_settings_service_create(&settings));
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(network_provisioning_service_create(&network));
    /* Hub subscribes to weather during UI startup. */
    ESP_ERROR_CHECK(weather_service_init(&(weather_service_config_t){
        .user_agent = "ESP-Mosaico/0.1 https://github.com/esp-mosaico/esp-mosaico-claw", .refresh_interval_ms = 3600000, .stale_after_ms = 21600000}));
    ESP_ERROR_CHECK(mosaic_settings_platform_init(&(mosaic_settings_platform_config_t){
        .settings = settings, .network_provisioning = network, .save_config = save_config}));
    if (audio_err == ESP_OK) ESP_ERROR_CHECK(app_settings_service_restore_audio(settings));
    ESP_ERROR_CHECK(app_settings_service_restore_display(settings));
    ESP_ERROR_CHECK(app_settings_service_restore_brightness(settings));
    ESP_ERROR_CHECK(mosaic_ui_start());
    ESP_ERROR_CHECK(mosaic_settings_platform_start_battery_monitor());
    ESP_ERROR_CHECK(mosaic_button_platform_init());
    ESP_ERROR_CHECK(mosaic_imu_platform_init());
    ESP_ERROR_CHECK(corallium_start());
    ESP_ERROR_CHECK(wifi_manager_register_event_callback(network_changed, NULL));
    app_config_t *config = calloc(1, sizeof(*config));
    ESP_ERROR_CHECK(config ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(app_config_load(config));
    bool wifi_enabled = true;
    (void)app_settings_service_get_wifi_enabled(settings, &wifi_enabled);
    ESP_ERROR_CHECK(wifi_manager_start(&(wifi_manager_config_t){
        .sta_ssid = config->wifi_ssid, .sta_password = config->wifi_password,
        .ap_behavior = "close_on_sta", .start_disabled = !wifi_enabled || !config->wifi_ssid[0]}));
    memset(config, 0, sizeof(*config)); free(config);
    ESP_ERROR_CHECK(network_provisioning_service_start(network));
    ESP_ERROR_CHECK(weather_service_start());
#if CONFIG_PM_ENABLE
    /* DFS retains peak render throughput. Light sleep remains disabled until
     * USB, touch wake and panel behavior are measured on the actual revision. */
    const esp_pm_config_t power = {.max_freq_mhz = 320, .min_freq_mhz = 80, .light_sleep_enable = false};
    esp_err_t err = esp_pm_configure(&power);
    if (err != ESP_OK) ESP_LOGW(TAG, "DFS unavailable: %s", esp_err_to_name(err));
#endif
    ESP_LOGI(TAG, "Ready; Bluetooth switch restored; toggle in drawer or hold top key 500 ms");
}

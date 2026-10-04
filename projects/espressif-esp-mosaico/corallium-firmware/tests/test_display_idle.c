// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "app_settings_service.h"
#include "display_service.h"
#include "freertos/semphr.h"
#include "mosaic_idle_policy.h"
#include "mosaico_audio.h"
#include "settings_store.h"
#include "wifi_manager.h"

typedef struct {
    char key[16];
    char value[32];
} test_entry_t;
static test_entry_t s_entries[32];
static unsigned s_writes;
static unsigned s_commits;
static esp_err_t s_write_error;
static uint8_t s_hardware_brightness = 100;
static int s_volume = 80;
static app_settings_service_handle_t s_rotation_settings;

static test_entry_t *find_entry(const char *key, bool create)
{
    test_entry_t *empty = NULL;
    for (size_t i = 0; i < sizeof(s_entries) / sizeof(s_entries[0]); ++i) {
        if (strcmp(s_entries[i].key, key) == 0) return &s_entries[i];
        if (s_entries[i].key[0] == '\0' && empty == NULL) empty = &s_entries[i];
    }
    if (!create) return NULL;
    assert(empty != NULL && strlen(key) < sizeof(empty->key));
    snprintf(empty->key, sizeof(empty->key), "%s", key);
    return empty;
}

esp_err_t settings_store_get_string(const char *key, char *buf,
                                    size_t size, const char *fallback)
{
    test_entry_t *entry = find_entry(key, false);
    const char *value = entry != NULL ? entry->value : fallback;
    assert(strlen(value) < size);
    snprintf(buf, size, "%s", value);
    return ESP_OK;
}
esp_err_t settings_store_has_key(const char *key, bool *exists)
{
    *exists = find_entry(key, false) != NULL;
    return ESP_OK;
}
esp_err_t settings_store_set_string(const char *key, const char *value)
{
    if (s_write_error != ESP_OK) return s_write_error;
    test_entry_t *entry = find_entry(key, true);
    assert(strlen(value) < sizeof(entry->value));
    snprintf(entry->value, sizeof(entry->value), "%s", value);
    ++s_writes;
    return ESP_OK;
}
esp_err_t settings_store_erase_key(const char *key)
{
    test_entry_t *entry = find_entry(key, false);
    if (entry != NULL) memset(entry, 0, sizeof(*entry));
    return ESP_OK;
}
esp_err_t settings_store_commit(void) { ++s_commits; return ESP_OK; }

SemaphoreHandle_t xSemaphoreCreateMutex(void) { return calloc(1, sizeof(bool)); }
int xSemaphoreTake(SemaphoreHandle_t handle, uint32_t ticks)
{
    assert(handle != NULL && ticks != 0 && !*(bool *)handle);
    *(bool *)handle = true; return pdTRUE;
}
int xSemaphoreGive(SemaphoreHandle_t handle)
{
    assert(handle != NULL && *(bool *)handle);
    *(bool *)handle = false; return pdTRUE;
}
void vSemaphoreDelete(SemaphoreHandle_t handle) { free(handle); }

bool display_service_is_started(void) { return true; }
esp_err_t display_service_set_rotation(uint16_t degrees)
{
    (void)degrees;
    if (s_rotation_settings != NULL) {
        /* Renderer handoff calls brightness restore through the same service. */
        assert(app_settings_service_set_idle_dimmed(
            s_rotation_settings, false) == ESP_OK);
    }
    return ESP_OK;
}
esp_err_t display_service_get_rotation(uint16_t *degrees)
{
    *degrees = 0; return ESP_OK;
}
esp_err_t display_service_set_brightness(uint8_t brightness)
{
    s_hardware_brightness = brightness; return ESP_OK;
}
esp_err_t display_service_get_brightness(uint8_t *brightness)
{
    *brightness = s_hardware_brightness; return ESP_OK;
}
esp_err_t display_service_prepare_brightness_fade(
    uint8_t start, uint8_t target, uint32_t duration_ms)
{
    (void)start; (void)duration_ms;
    s_hardware_brightness = target; return ESP_OK;
}
bool mosaico_audio_available(void) { return true; }
esp_err_t mosaico_audio_set_volume(int volume)
{
    s_volume = volume; return ESP_OK;
}
int mosaico_audio_get_volume(void) { return s_volume; }
esp_err_t mosaico_audio_preview(void) { return ESP_OK; }
esp_err_t app_config_load(app_config_t *config)
{
    memset(config, 0, sizeof(*config)); return ESP_OK;
}
esp_err_t app_config_reset_all(void)
{
    memset(s_entries, 0, sizeof(s_entries)); return ESP_OK;
}
void wifi_manager_get_status(wifi_manager_status_t *status)
{
    memset(status, 0, sizeof(*status));
}
#ifndef __APPLE__
size_t strlcpy(char *dest, const char *src, size_t size)
{
    const size_t length = strlen(src);
    if (size > 0) {
        const size_t copied = length < size - 1 ? length : size - 1;
        memcpy(dest, src, copied); dest[copied] = '\0';
    }
    return length;
}
#endif

static void test_policy_stages(void)
{
    const mosaic_idle_power_policy_t policy = {
        .dim_timeout_ms = 10000,
        .screen_timeout_ms = 30000,
        .poweroff_timeout_ms = 300000,
    };
    mosaic_idle_power_decision_t result = mosaic_idle_policy_evaluate(
        &policy, 0, true, false, true);
    assert(!result.dimmed && !result.asleep && !result.poweroff_due);
    assert(result.next_timeout_ms == 10000);
    result = mosaic_idle_policy_evaluate(&policy, 9999, true, false, true);
    assert(!result.dimmed && result.next_timeout_ms == 1);
    result = mosaic_idle_policy_evaluate(&policy, 10000, true, false, true);
    assert(result.dimmed && !result.asleep && result.next_timeout_ms == 20000);
    result = mosaic_idle_policy_evaluate(&policy, 30000, true, false, true);
    assert(result.asleep && !result.poweroff_due);
    /* Panel-off cannot remove the outstanding whole-device timer. */
    assert(result.next_timeout_ms == 270000);
    result = mosaic_idle_policy_evaluate(&policy, 299999, true, false, true);
    assert(result.asleep && result.next_timeout_ms == 1);
    result = mosaic_idle_policy_evaluate(&policy, 300000, true, false, true);
    assert(result.poweroff_due && result.next_timeout_ms == 0);
    result = mosaic_idle_policy_evaluate(&policy, UINT64_MAX, true, false, true);
    assert(result.poweroff_due && result.next_timeout_ms == 0);
    /* A touch resets the shared age and therefore restores brightness. */
    result = mosaic_idle_policy_evaluate(&policy, 0, true, false, true);
    assert(!result.dimmed && !result.asleep && !result.poweroff_due);
}

static void test_charging_and_presenter(void)
{
    for (unsigned flags = 0; flags < 4; ++flags) {
        const mosaic_idle_power_policy_t policy = {
            .dim_timeout_ms = 10000,
            .screen_timeout_ms = 30000,
            .poweroff_timeout_ms = 300000,
            .dim_while_charging = (flags & 1U) != 0,
            .sleep_while_charging = (flags & 2U) != 0,
        };
        mosaic_idle_power_decision_t result = mosaic_idle_policy_evaluate(
            &policy, 300000, true, true, true);
        assert(result.dimmed == policy.dim_while_charging);
        assert(result.asleep == policy.sleep_while_charging);
        assert(!result.poweroff_due && result.next_timeout_ms == 0);
        result = mosaic_idle_policy_evaluate(&policy, 300000, false, false, true);
        assert(result.dimmed && result.asleep && !result.poweroff_due);
        result = mosaic_idle_policy_evaluate(&policy, 300000, true, false, false);
        assert(!result.dimmed && !result.asleep && !result.poweroff_due);
        assert(result.next_timeout_ms == 0);
    }
    const mosaic_idle_power_policy_t disabled = {0};
    mosaic_idle_power_decision_t result = mosaic_idle_policy_evaluate(
        &disabled, UINT64_MAX, true, false, true);
    assert(!result.dimmed && !result.asleep && !result.poweroff_due);
    assert(result.next_timeout_ms == 0);
    const mosaic_idle_power_policy_t shutdown_only = {.poweroff_timeout_ms = 300000};
    result = mosaic_idle_policy_evaluate(&shutdown_only, 10000, true, false, true);
    assert(!result.asleep && result.next_timeout_ms == 290000);
    result = mosaic_idle_policy_evaluate(&shutdown_only, 300000, true, false, true);
    assert(result.poweroff_due && !result.asleep);
}

static void test_persistence_validation(void)
{
    app_system_config_t config;
    assert(app_system_config_load(&config) == ESP_OK);
    assert(config.dim_timeout_ms == 10000 && config.screen_timeout_ms == 30000);
    assert(config.poweroff_timeout_ms == 0);
    assert(!config.dim_while_charging && !config.sleep_while_charging);
    config.dim_timeout_ms = 5000;
    config.screen_timeout_ms = 0;
    config.poweroff_timeout_ms = 3600000;
    config.dim_while_charging = true;
    config.sleep_while_charging = true;
    assert(app_system_config_save(&config) == ESP_OK);
    memset(&config, 0, sizeof(config));
    assert(app_system_config_load(&config) == ESP_OK);
    assert(config.dim_timeout_ms == 5000 && config.screen_timeout_ms == 0);
    assert(config.poweroff_timeout_ms == 3600000);
    assert(config.dim_while_charging && config.sleep_while_charging);
    const unsigned writes_before = s_writes;
    config.dim_timeout_ms = 12000;
    assert(app_system_config_save(&config) == ESP_ERR_INVALID_ARG);
    assert(s_writes == writes_before);
    config.dim_timeout_ms = 0;
    config.poweroff_timeout_ms = UINT32_MAX;
    assert(app_system_config_save(&config) == ESP_ERR_INVALID_ARG);
    assert(s_writes == writes_before);
    assert(settings_store_set_string("sys_dim_to", "12000") == ESP_OK);
    assert(settings_store_set_string("sys_poweroff_to", "99999999999999") == ESP_OK);
    assert(settings_store_set_string("sys_dim_charge", "2") == ESP_OK);
    assert(settings_store_set_string("sys_sleep_chrg", "true") == ESP_OK);
    assert(settings_store_set_string("sys_screen_to", "-1") == ESP_OK);
    assert(app_system_config_load(&config) == ESP_OK);
    assert(config.dim_timeout_ms == 10000 && config.screen_timeout_ms == 30000);
    assert(config.poweroff_timeout_ms == 0);
    assert(!config.dim_while_charging && !config.sleep_while_charging);
    assert(settings_store_set_string("ui_rotation", "90") == ESP_OK);
    assert(app_system_config_reset() == ESP_OK);
    assert(find_entry("sys_dim_to", false) == NULL);
    assert(find_entry("sys_poweroff_to", false) == NULL);
    assert(find_entry("sys_dim_charge", false) == NULL);
    assert(find_entry("sys_sleep_chrg", false) == NULL);
    assert(find_entry("ui_rotation", false) == NULL);
    assert(app_system_config_load(&config) == ESP_OK && config.rotation == 0);
    assert(s_commits != 0);
}

static void test_user_brightness_and_service(void)
{
    app_settings_service_handle_t handle = NULL;
    assert(app_settings_service_create(&handle) == ESP_OK && handle != NULL);
    assert(app_settings_service_restore_brightness(handle) == ESP_OK);
    assert(app_settings_service_set_brightness(handle, 80, true) == ESP_OK);
    assert(s_hardware_brightness == 82);
    const unsigned writes_before = s_writes;
    assert(app_settings_service_set_idle_dimmed(handle, true) == ESP_OK);
    assert(s_hardware_brightness == 15 && s_writes == writes_before);
    app_settings_snapshot_t snapshot;
    assert(app_settings_service_get_snapshot(handle, &snapshot) == ESP_OK);
    assert(snapshot.brightness == 80);
    assert(app_settings_service_set_idle_dimmed(handle, false) == ESP_OK);
    assert(s_hardware_brightness == 82 && s_writes == writes_before);
    assert(app_settings_service_set_idle_dimmed(handle, true) == ESP_OK);
    s_rotation_settings = handle;
    assert(app_settings_service_set_rotation(handle, 90) == ESP_OK);
    s_rotation_settings = NULL;
    assert(s_hardware_brightness == 82);
    app_system_config_t display_config;
    assert(app_settings_service_get_display_config(handle, &display_config) == ESP_OK);
    assert(display_config.rotation == 90);
    /* Live slider previews remain independent of persisted brightness. */
    assert(app_settings_service_set_brightness(handle, 40, false) == ESP_OK);
    assert(app_settings_service_set_idle_dimmed(handle, true) == ESP_OK);
    assert(app_settings_service_set_idle_dimmed(handle, false) == ESP_OK);
    assert(s_hardware_brightness == 46);
    assert(app_settings_service_get_snapshot(handle, &snapshot) == ESP_OK);
    assert(snapshot.brightness == 80);
    assert(app_settings_service_set_brightness(handle, 0, true) == ESP_OK);
    assert(s_hardware_brightness == 10);
    assert(app_settings_service_set_idle_dimmed(handle, true) == ESP_OK);
    assert(s_hardware_brightness == 10); /* Dimming never brightens a low setting. */
    assert(app_settings_service_set_idle_dimmed(handle, false) == ESP_OK);
    s_write_error = ESP_FAIL;
    assert(app_settings_service_set_brightness(handle, 100, true) == ESP_FAIL);
    assert(s_hardware_brightness == 10);
    s_write_error = ESP_OK;
    assert(app_settings_service_set_dim_timeout(handle, 15000) == ESP_OK);
    assert(app_settings_service_set_poweroff_timeout(handle, 900000) == ESP_OK);
    assert(app_settings_service_set_dim_while_charging(handle, true) == ESP_OK);
    assert(app_settings_service_set_sleep_while_charging(handle, true) == ESP_OK);
    assert(app_settings_service_set_dim_timeout(handle, 14000) == ESP_ERR_INVALID_ARG);
    assert(app_settings_service_set_poweroff_timeout(handle, 1000) == ESP_ERR_INVALID_ARG);
    assert(app_settings_service_get_snapshot(handle, &snapshot) == ESP_OK);
    assert(snapshot.dim_timeout_ms == 15000 && snapshot.poweroff_timeout_ms == 900000);
    assert(snapshot.dim_while_charging && snapshot.sleep_while_charging);
    app_settings_service_delete(handle);
}

int main(void)
{
    test_policy_stages();
    test_charging_and_presenter();
    test_persistence_validation();
    test_user_brightness_and_service();
    puts("Display idle: stage deadlines, charging/presenter guards, persistence and brightness restoration passed");
    return 0;
}

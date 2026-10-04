#!/usr/bin/env python3
"""Check boot sequencing and execute the actual Wi-Fi queue config helpers."""
import os
from pathlib import Path
import re
import subprocess
import sys

upstream = Path(sys.argv[1]).resolve()
project = Path(__file__).resolve().parents[1]
output = project.parents[2] / "tmp/mosaico/host-tests"
output.mkdir(parents=True, exist_ok=True)
source = (upstream / "components/wifi_manager/wifi_manager.c").read_text()
boot = (project / "overlay/main/main.c").read_text()
assert boot.index("wifi_manager_prepare_start(") < boot.index("mosaic_ui_start()")
assert boot.index("corallium_restore_switch()") < boot.index("mosaic_ui_start()")
assert boot.index("network_provisioning_service_start(network)") < boot.index("mosaic_ui_start()")
assert boot.index("mosaic_ui_start()") < boot.index("wifi_manager_start_prepared()")
assert boot.index("mosaic_ui_start()") < boot.index("weather_service_start()")
assert "wifi_manager_wait_connected(" not in boot
for name in ("wifi_manager_start", "wifi_manager_start_prepared"):
    body = source.split("esp_err_t " + name + "(", 1)[1].split("\n}", 1)[0]
    assert "submit_command(&command, false)" in body, name + " waits for radio"
assert "preserve_desired_enabled = true" in source
prepared = source.split("esp_err_t wifi_manager_start_prepared(", 1)[1].split("\n}", 1)[0]
assert "command_copy_config" not in prepared and "s_config" not in prepared, "Startup races worker-owned credentials"
prepared_worker = source.split("if (command.preserve_desired_enabled)", 1)[1].split("command_rebind_config", 1)[0]
assert "s_desired_enabled ? worker_start_radio() : ESP_OK" in prepared_worker
assert "worker_apply_config" not in prepared_worker, "Startup overwrites earlier credential commands"

def function(name):
    match = re.search(r"static void " + name + r"\([^)]*\)\s*\{.*?\n}\n", source, re.S)
    assert match, "Missing real Wi-Fi command helper: " + name
    return match.group(0)

test = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define strlcpy test_strlcpy
typedef struct {
    const char *sta_ssid, *sta_password, *ap_ssid_prefix, *ap_ssid, *ap_password, *ap_behavior;
    bool start_disabled;
} wifi_manager_config_t;
typedef struct {
    wifi_manager_config_t config;
    char sta_ssid[33], sta_password[65], ap_ssid_prefix[33], ap_ssid[33], ap_password[65], ap_behavior[16];
} wifi_manager_command_t;
static size_t strlcpy(char *dest, const char *src, size_t size) {
    const size_t length = strlen(src);
    if (size) { const size_t count = length < size - 1 ? length : size - 1; memcpy(dest, src, count); dest[count] = 0; }
    return length;
}
'''
test += function("command_copy_config") + function("command_rebind_config")
test += r'''
int main(void) {
    wifi_manager_config_t config = {
        .sta_ssid = "queue-owned-ssid", .sta_password = "test-credential",
        .ap_ssid_prefix = "ESP", .ap_ssid = "test-ap", .ap_password = "ap-password",
        .ap_behavior = "close_on_sta", .start_disabled = true,
    };
    wifi_manager_command_t original = {0}, worker = {0};
    command_copy_config(&original, &config);
    memcpy(&worker, &original, sizeof(worker)); /* Same value-copy contract as FreeRTOS. */
    memset(&original, 0, sizeof(original)); /* Caller returns / its stack is reused. */
    command_rebind_config(&worker);
    assert(worker.config.sta_ssid == worker.sta_ssid);
    assert(worker.config.sta_password == worker.sta_password);
    assert(worker.config.ap_ssid_prefix == worker.ap_ssid_prefix);
    assert(worker.config.ap_ssid == worker.ap_ssid);
    assert(worker.config.ap_password == worker.ap_password);
    assert(worker.config.ap_behavior == worker.ap_behavior);
    assert(strcmp(worker.config.sta_ssid, "queue-owned-ssid") == 0);
    assert(strcmp(worker.config.sta_password, "test-credential") == 0);
    assert(worker.config.start_disabled);
    command_copy_config(&original, &(wifi_manager_config_t){0});
    memcpy(&worker, &original, sizeof(worker));
    command_rebind_config(&worker);
    assert(worker.config.sta_ssid == NULL && worker.config.sta_password == NULL);
    assert(worker.config.ap_ssid == NULL && worker.config.ap_behavior == NULL);
    puts("Boot: saved state precedes UI, radio/HTTP start after UI; actual queued config owns its strings");
}
'''
test_source = output / "test-wifi-commands.c"
test_source.write_text(test)
executable = output / "test-wifi-commands"
subprocess.run([os.environ.get("CC", "clang"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", str(test_source), "-o", str(executable)], check=True)
subprocess.run([str(executable)], check=True)

#!/usr/bin/env python3
"""Verify USB CDC startup/reset and exclusion of the vendor update client."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument("--build", required=True, type=Path)
parser.add_argument("--nm", default="riscv32-esp-elf-nm")
args = parser.parse_args()
build = args.build.resolve()
description = json.loads((build / "project_description.json").read_text())
config = Path(description["config_file"]).read_text().splitlines()
config_values = dict(line.split("=", 1) for line in config if line.startswith("CONFIG_") and "=" in line)
for option in ("TINYUSB_CDC_ENABLED", "USB_HS_CONSOLE_USB_CDC_AUTO_DOWNLOAD", "USB_HS_CONSOLE_USB_CDC_AUTO_INIT",
               "MOSAICO_USB_DIAGNOSTICS", "FREERTOS_USE_TRACE_FACILITY", "FREERTOS_GENERATE_RUN_TIME_STATS",
               "FREERTOS_VTASKLIST_INCLUDE_COREID", "FREERTOS_RUN_TIME_COUNTER_TYPE_U32",
               "FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER"):
    assert f"CONFIG_{option}=y" in config, f"Required diagnostics configuration disabled: {option}"
assert "CONFIG_APP_CLAW_ENABLE_CLI=y" not in config, "Unused full Claw CLI must not replace USB diagnostics"
cdc_tx_bytes = config_values.get("CONFIG_TINYUSB_CDC_TX_BUFSIZE", "")
assert cdc_tx_bytes.isdecimal() and int(cdc_tx_bytes) >= 512, \
    "USB diagnostics require TINYUSB_CDC_TX_BUFSIZE >= 512 bytes for whole-packet writes"

project = description["project_name"]
elf = build / f"{project}.elf"
result = subprocess.run([args.nm, "--defined-only", str(elf)], check=True,
                        text=True, capture_output=True)
symbols = {line.split()[-1] for line in result.stdout.splitlines() if line.split()}
diagnostic_archive = build / "esp-idf/mosaico_diagnostics/libmosaico_diagnostics.a"
diagnostic_imports = subprocess.run([args.nm, "--undefined-only", str(diagnostic_archive)],
                                   check=True, text=True, capture_output=True)
imports = {line.split()[-1] for line in diagnostic_imports.stdout.splitlines() if line.split()}
assert not {"ftrylockfile", "flockfile", "funlockfile"} & imports, \
    "USB transport must not use target Picolibc's incompatible FILE locking pair"
assert not any(symbol.startswith("update_check_") for symbol in symbols), "Vendor update client remains linked"
assert "update_check_service" not in description["build_components"], "Vendor update component remains in the build"
assert b"esp-mosaico-stable.json" not in (build / f"{project}.bin").read_bytes(), "Official update manifest remains in firmware"
for symbol in ("__wrap_app_main", "app_main", "bsp_usb_console_init", "tinyusb_driver_install", "tinyusb_console_init",
               "cdc_line_state_changed_callback", "usb_console_before_restart",
               "mosaico_diagnostics_start", "mosaico_diagnostics_platform_start", "mosaico_diagnostics_tasks_snapshot",
               "mosaic_ui_get_diagnostics", "mosaic_ui_simulate_drag", "mosaic_ui_open_app",
               "mosaic_ui_capture_begin", "mosaic_ui_capture_read", "mosaic_ui_capture_end",
               "__wrap_esp_display_presenter_begin_next_frame", "__wrap_esp_display_presenter_submit_buffer",
               "__wrap_esp_display_presenter_commit_frame", "__wrap_esp_display_presenter_cancel_frame",
               "__wrap_esp_display_presenter_quiesce"):
    assert symbol in symbols, f"USB startup/reset symbol missing from linked ELF: {symbol}"

link_file = build / "CMakeFiles" / f"{project}.elf.dir" / "link.txt"
if not link_file.exists():
    link_file = build / "build.ninja"
link = link_file.read_text()
assert "--wrap=app_main" in link and "__wrap_app_main" in link, "Factory pre-app USB console wrapper is not linked"
link_wrappers = set(re.findall(r"--wrap=([A-Za-z_][A-Za-z_0-9]*)(?=[\s,\"']|$)", link))
for presenter_operation in ("begin_next_frame", "submit_buffer", "commit_frame", "cancel_frame", "quiesce"):
    wrapped_symbol = f"esp_display_presenter_{presenter_operation}"
    assert wrapped_symbol in link_wrappers, f"Real-frame capture link wrapper missing: {wrapped_symbol}"
print(f"PASS: {elf.name} includes factory USB CDC initialization, console and auto-download reset hooks")
print("PASS: bounded USB diagnostics, queued UI input and device frame capture are linked")
print("PASS: vendor update component, client symbols and official manifest URL are absent")
subprocess.run([sys.executable, str(Path(__file__).with_name("check_assets.py")),
                "--build", str(build)], check=True)

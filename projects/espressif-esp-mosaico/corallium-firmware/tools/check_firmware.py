#!/usr/bin/env python3
"""Verify USB CDC startup/reset and exclusion of the vendor update client."""
import argparse
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--build", required=True, type=Path)
parser.add_argument("--nm", default="riscv32-esp-elf-nm")
args = parser.parse_args()
build = args.build.resolve()
description = json.loads((build / "project_description.json").read_text())
config = Path(description["config_file"]).read_text().splitlines()
for option in ("TINYUSB_CDC_ENABLED", "USB_HS_CONSOLE_USB_CDC_AUTO_DOWNLOAD", "USB_HS_CONSOLE_USB_CDC_AUTO_INIT"):
    assert f"CONFIG_{option}=y" in config, f"USB application console disabled: {option}"

project = description["project_name"]
elf = build / f"{project}.elf"
result = subprocess.run([args.nm, "--defined-only", str(elf)], check=True,
                        text=True, capture_output=True)
symbols = {line.split()[-1] for line in result.stdout.splitlines() if line.split()}
assert not any(symbol.startswith("update_check_") for symbol in symbols), "Vendor update client remains linked"
assert "update_check_service" not in description["build_components"], "Vendor update component remains in the build"
assert b"esp-mosaico-stable.json" not in (build / f"{project}.bin").read_bytes(), "Official update manifest remains in firmware"
for symbol in ("__wrap_app_main", "app_main", "bsp_usb_console_init", "tinyusb_driver_install", "tinyusb_console_init",
               "cdc_line_state_changed_callback", "usb_console_before_restart"):
    assert symbol in symbols, f"USB startup/reset symbol missing from linked ELF: {symbol}"

link_file = build / "CMakeFiles" / f"{project}.elf.dir" / "link.txt"
if not link_file.exists():
    link_file = build / "build.ninja"
link = link_file.read_text()
assert "--wrap=app_main" in link and "__wrap_app_main" in link, "Factory pre-app USB console wrapper is not linked"
print(f"PASS: {elf.name} includes factory USB CDC initialization, console and auto-download reset hooks")
print("PASS: vendor update component, client symbols and official manifest URL are absent")

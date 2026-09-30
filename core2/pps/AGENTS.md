# core2/pps - official PPS demo adaptation

- Upstream: M5Stack `M5Module-PPS-UserDemo`, commit
  `6a3e83d0db9d7ed832627930c79ec45153b39f6c`. Preserve vendor license and headers.
- Keep the official LVGL interface. Local additions are serial diagnostics,
  output validation, and Core2 M-Bus input power configuration.
- PPS is at internal I2C address `0x35`, SDA 21 / SCL 22, 100 kHz.
- The LVGL task owns console commands and PPS I2C operations. Do not introduce
  a second task that accesses PPS independently.
- Boot with banana output off. Enabling output requires valid VIN / setpoints.
- This project flashes the Core2 ESP32 only. Do not write the PPS STM32 firmware,
  address, UID, or calibration sector as part of ordinary demo work.
- Supply DC 9-36V. The auxiliary isolated M-Bus supply is nominally 5V / 200mA,
  separate from the adjustable banana outputs. Keep Core2 bus boost disabled.
- Confirm that outputs are empty before running `tools/test_output.py`.
- Backups, flash binaries, raw logs, and schematic downloads belong in `tmp/`.

# Hardware contract

Official sources:

- [ESP-Mosaico hardware guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp-mosaico/index.html)
- [CoreBoard V1.2](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp-mosaico/user_guide.html)
- [CoreBoard V1.0](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp-mosaico/user_guide_v1.0.html)
- [Factory firmware source](https://github.com/esp-mosaico/esp-mosaico-claw)

The ESP32-S31 supports Wi-Fi and Bluetooth LE; Corallium uses BLE GATT, not the
factory Bluetooth audio profiles. The fuel gauge is BQ27220 at I2C 0x55. Official
specification lists a 3.7 V / 65 mAh cell, which does not establish this user's
cell condition, capacity calibration, battery life or possession of accessories.

The factory-derived board configuration is retained. V1.0 and V1.2 pin mappings
must not be interchanged without confirming the physical board revision. GPIO45
controls the speaker amplifier, GPIO57 requests complete shutdown, and GPIO60
shares display power and external rails. Cutting GPIO60 to save accessory power
would also affect the display.

No separately backed external RTC appears in the official component list or
published BSP interfaces. ESP-IDF's RTC + high-resolution system clock can retain
wall time through supported software resets/deep sleep while the RTC domain is
powered. The POWER button removes device power through SAM8108, so a cold start
cannot recover elapsed off time. App or network synchronization is required then;
NVS stores only the UTC display offset, never a stale timestamp presented as now.
RTC drift and reset-path retention still require measurement on the actual board.

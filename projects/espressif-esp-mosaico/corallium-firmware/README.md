# Corallium for ESP-Mosaico

Factory-firmware derivative with a simpler launcher/settings/control center,
Corallium BLE provisioning, offline app time synchronization, battery power/runtime
telemetry and reduced background/rendering work. Official update checking and
upgrade entries are removed; About still shows the installed version.
See [behavior and limits](specs/behavior.md). The connected board identifies as
CoreBoard 1.2 through eFuse. Final runtime acceptance remains pending.

## Source and build

The repository tracks a source lock, a reviewable patch and our overlay instead
of importing the vendor SDK and hundreds of megabytes of dependencies. Preparation
retains the upstream Apache-2.0 license and private dependencies. The component
lock and Python generator versions are included in the overlay. Generated source,
SDKs, logs and builds remain under ignored `tmp/`.

Run from the repository root:

```sh
python3 projects/espressif-esp-mosaico/corallium-firmware/tools/prepare.py
```

Use ESP-IDF commit `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe` (ESP32-S31). An
ordinary ESP32-S3/PlatformIO SDK cannot build this target. For a new dedicated SDK
checkout, run from the repository root:

```sh
git clone --filter=blob:none https://github.com/espressif/esp-idf.git tmp/mosaico/esp-idf
git -C tmp/mosaico/esp-idf checkout 7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe
git -C tmp/mosaico/esp-idf submodule update --init --recursive
tmp/mosaico/esp-idf/install.sh esp32s31
. tmp/mosaico/esp-idf/export.sh
```

With that environment activated, continue in the prepared project:

```sh
cd tmp/mosaico/corallium-firmware
python -m pip install -r requirements.txt
idf.py -D SDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.corallium' bmgr -c ./boards -b esp_mosaico
idf.py -D SDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.corallium' build
```

The overlay enables the factory `USB_HS_CONSOLE_USB_CDC_AUTO_DOWNLOAD` and
`USB_HS_CONSOLE_USB_CDC_AUTO_INIT` options. These initialize Type-C TinyUSB CDC
before `app_main` and retain the vendor DTR/RTS download-reset mechanism. When
updating an existing build, enable both options in `idf.py menuconfig`; saved
`sdkconfig` values take precedence over defaults. Verify the resulting ELF with
the activated ESP-IDF toolchain:

```sh
# Run from the repository root after the build.
python3 projects/espressif-esp-mosaico/corallium-firmware/tools/check_firmware.py --build tmp/mosaico/corallium-firmware/build
```

The pinned factory CMake applies its narrowly scoped IDF fixes to the selected
SDK. Use a dedicated checkout of that IDF revision rather than a shared SDK.
Flash only after confirming the physical board/revision and inspecting the
partition images; the factory project includes filesystem images. This README
does not authorize replacing an unidentified attached device.

## Checks and measurements

```sh
python3 projects/espressif-esp-mosaico/corallium-firmware/tools/check_host.py
# After dependency configuration; uses the same cJSON as the firmware:
python3 projects/espressif-esp-mosaico/corallium-firmware/tools/check_host.py --upstream tmp/mosaico/corallium-firmware
# Parse native device logs captured with CONFIG_CORALLIUM_PERF_LOG=y:
python3 projects/espressif-esp-mosaico/corallium-firmware/tools/analyze_performance.py tmp/mosaico/serial.log
```

Host tests use AddressSanitizer/UBSan and cover fragmented UTF-8, maximum line size,
overflow/NUL/timeout recovery, JSON recursion/UTF-8 guards, clock ranges, Wi-Fi credential boundaries, failed
persistence and reboot enable state, response-shaped mutation rejection, charging
telemetry, and renderer scheduling. Scene validation checks
removed controls have hidden ancestors and no touchable route.
Settings scene checks also require update navigation and actions to be absent;
the linked-firmware check rejects the vendor updater and official manifest URL.
It also parses actual compiled font references and the MMAP flash image, rejecting
wrong font ordinals, missing catalog members and stale staged resources.

For BLE, open the Bluetooth tile or hold the top key for 500 ms, then connect in
Corallium within 120 seconds (the device advertises as ESP-Mosaico). Connect while adjacent to the device; the GATT link
uses encrypted Just Works. [Validation scope](specs/validation.md) records actual
evidence and remaining hardware checks.

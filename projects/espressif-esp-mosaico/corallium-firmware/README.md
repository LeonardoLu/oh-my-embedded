# Corallium for ESP-Mosaico

Factory-firmware derivative with a compact control center, configurable display
idle policy, a dot-matrix weather widget and official local Works/Lab apps. Corallium BLE provisioning, offline app time synchronization
and battery power/runtime telemetry are retained. Official update checking and
upgrade entries are removed; About still shows the installed version.
See [behavior and limits](specs/behavior.md). The connected board identifies as
CoreBoard 1.2 through eFuse. [Validation evidence](specs/validation.md) separates
host/build checks, current device-frame evidence and remaining physical checks.

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
updating an existing build, enable both options and `MOSAICO_USB_DIAGNOSTICS`,
and disable the unused `APP_CLAW_ENABLE_CLI` in `idf.py menuconfig`; saved
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

Host checks require Python 3, CMake and Clang with AddressSanitizer/UBSan.
Scene generation uses the prepared project's `requirements.txt` dependencies.

```sh
python3 projects/espressif-esp-mosaico/corallium-firmware/tools/check_host.py
# After dependency configuration; uses the same cJSON as the firmware:
python3 projects/espressif-esp-mosaico/corallium-firmware/tools/check_host.py --upstream tmp/mosaico/corallium-firmware
# Parse native device logs captured with CONFIG_CORALLIUM_PERF_LOG=y:
python3 projects/espressif-esp-mosaico/corallium-firmware/tools/analyze_performance.py tmp/mosaico/serial.log
```

Host checks also exercise the native LiquidDuck solver under ASan/UBSan,
including rotating/shaking gravity, particle conservation, wall bounds and
RGB565 output bounds. The native IMU regression renders all 24 style/palette
combinations, exercises real touch routing, mode cycling and reset, and checks
Canvas backpressure, rejected submissions and delayed release after app exit.
Sensor inputs are fixtures; host previews do not establish hardware motion or FPS.

Host checks cover protocol framing/validation, failed persistence, saved local
states, asynchronous boot ordering, charging telemetry, display idle stages and
brightness restoration. Generated scene checks cover
safe icon padding, Display scrolling/choosers, the independent Bluetooth route,
compact control hit regions and Home navigation.
The native Settings regression executes the actual C row binder and state setters
through the pinned GSP simulator, checking RGBA uploads on a gray background,
row recycling, font-linked alpha, orange switch tracks and complete SVG chevrons.
Observer/callback input doubles also exercise lower-page scrolling, both
charging switches and Power-off chooser/save/return through the real C handlers.
Removed controls have hidden ancestors and no touchable route. Update navigation
and actions are absent; the linked-firmware check rejects the vendor updater and
official manifest URL.
It parses actual compiled font, bitmap and SVG references plus the MMAP flash
image, rejecting wrong font ordinals, missing resources and stale staged content. The
packed SYSTEM filesystem is checked against staging, including Works launchers
and the absence of removed simulation resources.

For BLE, use Settings → Bluetooth, toggle the Bluetooth tile or hold the top key
for 500 ms. The device advertises as ESP-Mosaico until switched off; its switch
survives restart and defaults off after factory reset. The GATT link uses no
pairing or encryption;
JSONL, including Wi-Fi credentials, is transmitted in plaintext. Passwords remain
excluded from responses and logs. [Validation scope](specs/validation.md) records actual
evidence and remaining hardware checks.

USB diagnostics reuse the initialized factory console and add its missing
command reader. With the ESP-IDF Python environment activated (providing
`pyserial`), run from the repository root:

```sh
python projects/espressif-esp-mosaico/corallium-firmware/tools/device_diagnostics.py --list
python projects/espressif-esp-mosaico/corallium-firmware/tools/device_diagnostics.py status
python projects/espressif-esp-mosaico/corallium-firmware/tools/device_diagnostics.py tasks
python projects/espressif-esp-mosaico/corallium-firmware/tools/device_diagnostics.py open settings
python projects/espressif-esp-mosaico/corallium-firmware/tools/device_diagnostics.py tap 71 352
python projects/espressif-esp-mosaico/corallium-firmware/tools/device_diagnostics.py drag 240 20 240 450 400
python projects/espressif-esp-mosaico/corallium-firmware/tools/device_diagnostics.py --output tmp/mosaico/device-diagnostics capture
python projects/espressif-esp-mosaico/corallium-firmware/tools/device_diagnostics.py back
```

The client selects only the identified application's USB topology, then verifies
its MAC through `ping` before sending UI commands. Other attached boards are not
probed. It preserves the factory's safe connected DTR/RTS state and does not
issue reset/download sequences. `status` returns local controls, panel state and
actual renderer/error counters without keeping the screen awake. `capture`
saves CRC-verified RGB565, PNG and metadata under ignored `tmp/`; it observes
the actual device's submitted frame, with physical panel appearance and sound
still requiring their own observations. Sleeping or exclusive-presenter screens
reject capture. The importable `DiagnosticClient` supports multiple operations
through one serial connection, including status polling during idle tests.
Capture refreshes visible native rows after resuming the renderer and retains
later committed updates, preserving the page, scroll position and control
values. Runtime list masks use QOI. Source PNGs compiled into bitmap banks do
not require PNG decoding on the device.
`tasks` reports a fixed list of live RTOS task states, stack high-water marks and
32-bit runtime counters independently of UI locks; counters wrap and this does
not provide a call stack. `open` accepts only Settings, Works, Album or Weather,
and acknowledges queue admission. Poll `status` to verify that startup completed.

The pull-down always covers the whole screen. It groups Wi-Fi/Bluetooth and
mute/vibration into a compact 2×2 block, with vertical volume and brightness
sliders beside it. Both widget backgrounds are 220 pixels high. Slider icons
and percentages remain; explanatory labels are removed. Volume release and unmute
play a short local confirmation; startup stays silent. Muting and the prior
volume survive restart. Settings has separate Bluetooth, Battery, Corallium
protocol and About entries. Display defaults to dimming after 10 seconds and
screen-off after 30 seconds, with both disabled while the gauge reports charging;
whole-device auto power-off defaults to Never. See the behavior spec for timeout
choices and presenter/charging limits.

Home's first page provides Settings, Works and Album. Its weather card opens
Weather and replaces the analog clock with Weather's exact dot illustrations
and shared condition selection. Saved controls appear
before asynchronous Wi-Fi association and weather fetching. Works provides Lab,
Recent and Installed views; Lab contains the official Dino and Flappy Bird games.
The local Lua runtime starts on first opening Works. Flappy sound effects use
the output-only speaker worker and follow the saved master volume and mute.

Open IMU from the launcher. Its lower-left button cycles **Level → Pixel →
Gradient → Water → Level**. Pixel, Gradient and Water implement styles 2, 3 and 4
from [LiquidDuck_ESP32](https://github.com/nongxl/LiquidDuck_ESP32): rounded LED
cells with a solid bright color, density-colored LED cells, and continuous water.
The middle button cycles Matrix, Cyberpunk, Amber, Mono, Red, Deep Sea, Toxic and
Gold; Deep Sea is the initial palette. Reset refills the current tank. Tilt or
shake the device to move the liquid; the factory Back gesture exits IMU.
Mode and palette selections are session-local.

The square-screen adaptation uses 14×10 pixel and 28×20 water grids in a native
456×320 RGB565 Canvas. The solver and palettes retain upstream's PolyForm
Noncommercial terms; see [source and license notice](overlay/components/mosaic_ui/apps/imu/liquidduck/NOTICE.md).

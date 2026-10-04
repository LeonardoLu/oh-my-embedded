# Validation evidence and limits

## Reproducible source

The official factory commit and its private esp-claw submodule are fixed in
`upstream.lock.json`. `tools/prepare.py` fetched both revisions into a fresh
checkout, checked and applied the product patch, and installed the overlay
successfully. Component resolution is recorded in `overlay/dependencies.lock`;
the Python scene and board generators have pinned versions. The factory's
Apache-2.0 license is retained.

## Automated checks

`tools/check_host.py --upstream <prepared-checkout>` passed with Clang,
AddressSanitizer and UndefinedBehaviorSanitizer. It compiles the actual protocol
dispatcher with the same cJSON dependency used by the firmware; hardware services
are replaced by explicit test doubles. See the README for reproducible commands
and Python/Clang/Lua prerequisites. Covered behavior includes:

- Fragmented UTF-8; exact 2048-byte frames; overlength, NUL and timed-out frame
  recovery; clearing receive storage; invalid UTF-8 and JSON nesting limits.
- Version, identifier, envelope and duplicate-field rejection before mutation;
  empty forget payload; integer time/offset limits; 32-byte SSID and password
  boundaries; failed saves; persistent Wi-Fi enable/disable delegation.
- Charging/unavailable power and runtime semantics and idle scheduling boundaries.
- The actual system configuration/settings service with explicit NVS/display
  doubles: timeout choices/defaults, persistence/reset and invalid-value recovery,
  charging/unknown-battery/foreign-presenter policy, shutdown deadlines after
  screen-off, dim/user-brightness separation, preview restoration and failed-save
  rollback. A nonrecursive mutex double checks rotation/quiesce reentry. These
  tests do not operate the physical panel or shutdown line.
- Startup ordering and queue ownership: saved controls and local provider/
  callback setup precede UI, association/HTTP starts follow UI, and the actual
  queued Wi-Fi configuration owns/rebinds every string. Prepared START consumes
  the worker's current desired configuration rather than a pre-UI copy.
- Weather symbol classification for unavailable data, MET condition families,
  day/night differences, bounded dot frames and precipitation phases. The Canvas
  uses two busy-guarded borrowed buffers retained for Hub reuse; physical frame
  release and live animation remain separate checks.
- The exact shipped Lua Fluid/Dot Fluid implementation: conservation and
  pressure/gravity behavior, finite numerical bounds, touch, pause, Reset, Quit,
  cancellation and eligible idle-exit cleanup against a fake display.
  `tests/check_fluid_runtime.py` also compiles the exact bundled Lua in its
  firmware float32/int32 mode with ASan/UBSan and checks millisecond-counter
  wrap; it runs automatically with `check_host.py --upstream`. These checks
  validate simulation/lifecycle logic rather than device performance.
- The actual output-only speaker worker against explicit codec/RTOS test doubles:
  silent startup/restore, no output while muted, bounded 100 ms local PCM, and
  draining both 2880-byte and 4096-byte DMA rings before mute/close. The test
  rejects the earlier immediate-close implementation because tone samples remain
  queued. Open/query/write/drain failures and mid-play mute close the codec.
  This checks control flow, not physical sound/power.
- Generated factory scene traversal: removed actions have hidden ancestors,
  retained apps keep their launcher entries and Home provides Settings, Works and
  Album shortcuts plus a Weather card route. The Home analog-clock objects are
  absent and the weather Canvas is present. The 2×2 button block stays below one
  quarter of the screen; adjacent vertical sliders match pointer hit regions.
  Compact Battery labels fit their compiled font. The compiled Bluetooth
  connection badge uses the enabled/connected visibility predicate;
  its non-interactive geometry stays
  within the button while the enabled tile retains its original color.
  About and the independent protocol detail have separate navigation routes.
- Settings scene generation: ten padded tint icon assets and 46 icon slots,
  inset selector chevrons, scroll bounds/chooser geometry and independent
  Bluetooth navigation pass. The Wi-Fi password page contains no Bluetooth
  action. The update page, update bindings and actions are absent. Firmware
  inspection rejects linked update-client symbols, the vendor update component
  and its official manifest URL.
- Further icon/layout review covers Works paging, Album arrows, the IMU bubble,
  Bricks shapes, forecast icons and common Back bounds. Album thumbnail cover
  cropping remains intentional. `tests/test_weather_scene.py` checks all six
  actual artwork alpha bounds with a resampling margin, verifying at least five
  pixels above the forecast cards, title presence and clear header condition
  text. Native SUNNY/SNOW/WINDY/THUNDER fixtures use the actual A8 title and QOI
  forecast masks and preserve those separations. These authored fixtures force
  visibility; they do not test the hardware or dynamic C backend state.
- Actual GSPB/GSB/GFB resources: CRC, bounds and every font reference's ordinal and
  content ID are checked before and after font externalization. The packed MMAP
  image must match staged assets byte for byte. The checker rejects the captured
  failing Hub bundle where shell glob order assigned `font10` to ordinal 2;
  numeric font ordering corrects that mismatch. The linked-build checks cover
  16 bundles, 124 font references and 17 packed MMAP assets.
- `tools/check_system_assets.py` mounts the built SYSTEM filesystem with the
  factory builder's own LittleFS environment without auto-formatting: all 154 packed files match
  staging, six new files match the overlay, and all five Works launchers/entries
  plus the local registry are present. This checks the flash image rather than
  only the source staging directory.

Native GSP screenshots render authored Home/control-center/Settings fixtures on
the host. They provide evidence for generated geometry and assets, not hardware
runtime, live Wi-Fi/weather state or animated Canvas/panel behavior.

The full ESP32-S31 firmware compiled and linked with the pinned ESP-IDF checkout,
RISC-V toolchain `esp-16.1.0_20260609`, and the official board generator. The build
disables performance logging. Image generation and partition-size checks passed. The application
uses approximately 3.98 MiB, with about 49% of the smallest app partition free.
Inactive vendor helpers can produce unused-function warnings; these are not
treated as hardware evidence.

`tools/check_firmware.py --build <prepared-checkout>/build` passed against the USB-enabled
firmware. It checks the generated configuration, actual linked ELF symbols and
linker wrapping flags. It requires TinyUSB CDC and both factory
auto-init/download options, the pre-application wrapper, console initialization
and reset handlers to remain linked. This catches a build that silently omits
USB startup even though the board declares the console device. Host-side USB
enumeration and DTR/RTS reset behavior are separate physical-device checks.

## Current physical evidence

The identified ESP32-S31 board reports CoreBoard version 1.2 through eFuse.
The UI/Works firmware was installed through the identified ROM loader without
creating a firmware backup. All six images were hash-verified; NVS was outside
the erase/write ranges. Application USB CDC enumerated, startup reached Ready,
stored Wi-Fi reconnected and weather refreshed without a startup abort or
missing-font error in the captured interval.

Device logs record Weather and Works opening, the local Lua runtime registering
eight modules and finding five apps, and both fluid entries acquiring RAW.
Dot Fluid's exit returned the presenter and closed its display session. These
logs establish execution and handoff, not the appearance, smoothness or touch
response of the simulations. The local tagged-lease registry initialization
correction is build/resource-checked; its device cleanup check remains pending.
Cold-start control appearance, live weather animation, panel wake, charging
policy, whole-device shutdown and achieved frame rate remain unaccepted on
hardware.

## Earlier-firmware hardware observations

These observations belong to earlier Corallium derivative firmware and preserve
the BLE/audio/USB evidence boundary. They do not accept the current Home,
Settings, Works, weather or idle-policy changes.

Earlier firmware reached Ready, initialized the native UI and application USB
CDC, and rejoined the device's stored Wi-Fi configuration without a startup abort
or missing-font failure.

The Apple Development-signed macOS Corallium app discovered ESP-Mosaico and
completed ordinary GATT connections without pairing. The user confirmed that the
macOS device-pairing dialog no longer appeared. Device logs confirmed notification
subscription; the app received device.info, device.status and periodic status updates. App time synchronization returned success and displayed
UTC+08:00 with source Corallium. A Wi-Fi configuration request returned success,
progressed through connecting, and reported connected with a DHCP address.
The app's visible activity log contains operation/result metadata, not Wi-Fi
credentials. After more than two minutes enabled, disconnection and a new scan
still allowed a successful connection. The plain-GATT build also completed an
explicit disconnect/reconnect without a pairing step. Some scans found no device,
and an early connection ended after its initial replies; subsequent connections
remained active during configuration and state updates. These observations do not
establish a radio range, discovery-time or long-term link-stability guarantee.

The official CDC normal-reset sequence completed a powered software restart.
The saved enabled Bluetooth state was followed by discovery and another successful
app connection; Wi-Fi rejoined and UTC+08:00 remained saved. Network time was
already synchronized when the app read status, so this is not offline RTC-retention
evidence. The saved-off and factory-reset BLE paths remain host-tested only.
The factory CDC download-reset sequence reached ROM USB; the official ROM loader
wrote and verified the partition images without a stub or manual button sequence.
A warm RAM-stub launch had not responded, so that path is not accepted as the
automatic-update workflow.

The user confirmed audible feedback after raising volume and unmuting on the
DMA-draining speaker implementation. The earlier Home shortcuts, Weather card
navigation and centered icon-only controls were also confirmed on the device.
The connection-badge refinement booted and reconnected to the Mac; its physical
visual appearance was not separately confirmed.
Mute/previous-volume restoration across reboot and battery-detail layout remain
separate checks; codec-open logs alone are not acoustic acceptance.
Compilation and these observations do not establish battery accuracy, calibrated
power savings, RTC retention, long-term stability or achieved interactive FPS.

## Remaining physical checks

Use the same identified device for comparisons:

| Area | Procedure and observable result |
| --- | --- |
| USB | After leaving ROM download mode, application Type-C CDC must enumerate and expose startup logs. Reopen the console and verify the official DTR/RTS download-reset sequence returns to the ROM loader. |
| BLE | Fresh/reset device stays off. Enable locally: ESP-Mosaico appears and stays discoverable beyond two minutes. Ordinary unpaired subscription/RX succeeds without a system pairing dialog. Verify saved on/off after reboot, Connected state, local close, disconnect/reconnect framing and no status event before device.info completes. |
| Cold start | With saved controls and unavailable/slow Wi-Fi/weather, open the pull-down immediately: switches and slider values must reflect local saved state. The first page must remain usable while association/fetching progresses. |
| UI/icons | Confirm Settings/Works/Album on Home and Weather-card navigation. Inspect Network/Corallium/About tint masks, all reviewed icon slots, Display selector arrows, scrolling and chooser return. The Wi-Fi password page must omit Bluetooth; its separate Settings page must work. Confirm the compact 2×2 control block, adjacent sliders and shortened Battery values at their visible hit regions. |
| Weather | Exercise clear/cloud/precipitation and day/night symbols with real provider data. Confirm unavailable/stale states, animation only on visible Home and safe swipes/drawer/lock/app transitions. |
| Works | Open Lab/Recent/Installed and launch Dino/Flappy. For both fluid jobs, exercise touch, Reset, Pause/Play, Quit and physical Back. Confirm repeated launches release the RAW presenter and return to Works. Check local Recent persistence. |
| Audio | Releasing volume/unmuting sounds once; mute produces silence and restores the previous level after reboot. Confirm silent startup/background and no microphone task. |
| Wi-Fi | Set from app after local Wi-Fi off, join, inspect local SSID/IP, restart and confirm enabled state; wrong password reports failed; forget erases credentials and remains off after restart. Include a 32-byte SSID. |
| Time | Set without internet; powered software reset should retain plausible RTC time with estimated quality. Full power removal must show invalid/--:-- until app or NTP sync. Measure drift against an external reference. |
| Display/power | Exercise all timeout choices, Never, saved/restored defaults, dim/brightness restoration, ordinary child-app screen-off and touch/button wake. Confirm shutdown still follows an already-off screen on battery and GPIO57 permits subsequent power-button restart. Test charging switches while observing gauge state; unavailable samples must block shutdown. New fluids must close safely at eligible screen-off/shutdown deadlines; legacy RAW/LVGL owners defer effects until return. Record independent supply measurements separately from gauge discharge telemetry; charging runtime must be unavailable. |
| FPS | Enable native statistics, replay identical launcher swipes/settings scrolling for at least 30 seconds, and separately collect 60 seconds idle. Parse with `tools/analyze_performance.py`; report observed frames and render time rather than the 16 ms target interval. |

The implemented status/AOD text cache reduces stable minute/date write batches from
60 to 1 per minute by inspection. Quiet Hub dispatcher waits change from 16 to
50 ms, and paused waits to 100 ms. These are verified scheduling changes, not
measured battery-life or interactive-FPS improvements.

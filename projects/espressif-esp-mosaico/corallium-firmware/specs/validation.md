# Validation evidence and limits

## Reproducible source

The official factory commit and its private esp-claw submodule are fixed in
`upstream.lock.json`. `tools/prepare.py` fetched both revisions into a fresh
checkout, checked and applied the product patch, and installed the overlay
successfully. Component resolution is recorded in `overlay/dependencies.lock`;
the Python scene and board generators have pinned versions. The factory's
Apache-2.0 license is retained.

## Automated checks

`tools/check_host.py --upstream tmp/mosaico/claw` passed with Clang,
AddressSanitizer and UndefinedBehaviorSanitizer. It compiles the actual protocol
dispatcher with the same cJSON dependency used by the firmware; hardware services
are replaced by explicit test doubles. Covered behavior includes:

- Fragmented UTF-8; exact 2048-byte frames; overlength, NUL and timed-out frame
  recovery; clearing receive storage; invalid UTF-8 and JSON nesting limits.
- Version, identifier, envelope and duplicate-field rejection before mutation;
  empty forget payload; integer time/offset limits; 32-byte SSID and password
  boundaries; failed saves; persistent Wi-Fi enable/disable delegation.
- Charging/unavailable power and runtime semantics and idle scheduling boundaries.
- The actual output-only speaker worker against explicit codec/RTOS test doubles:
  silent startup/restore, no output while muted, bounded 100 ms local PCM, and
  codec close after feedback or a partially opened codec failure. This checks
  control flow, not physical sound/power.
- Generated factory scene traversal: removed actions have hidden ancestors,
  retained apps keep their launcher entries and Home provides its three requested
  shortcuts plus a Weather card route. Icon-only buttons are centered in two
  columns; controls/sliders match pointer hit regions and compact Battery labels
  fit their compiled font.
  About and the independent protocol detail have separate navigation routes.
- Settings scene generation: the update page, update bindings and actions are
  absent. Firmware inspection rejects linked update-client symbols, the vendor
  update component and its official manifest URL.
- Actual GSPB/GSB/GFB resources: CRC, bounds and every font reference's ordinal and
  content ID are checked before and after font externalization. The packed MMAP
  image must match staged assets byte for byte. The checker rejects the captured
  failing Hub bundle where shell glob order assigned `font10` to ordinal 2;
  numeric font ordering corrects that mismatch.

The full ESP32-S31 firmware compiled and linked with the pinned ESP-IDF checkout,
RISC-V toolchain `esp-16.1.0_20260609`, and the official board generator. The build
also enabled `CONFIG_CORALLIUM_PERF_LOG=y`, validating the optional native renderer
statistics path. Image generation and partition-size checks passed. The application
uses approximately 3.3 MiB, with about 58% of the smallest app partition free.
Inactive vendor helpers can produce unused-function warnings; these are not
treated as hardware evidence.

`tools/check_firmware.py --build tmp/mosaico/claw/build` passed against the USB-enabled
firmware. It checks the generated configuration, actual linked ELF symbols and
linker wrapping flags. It requires TinyUSB CDC and both factory
auto-init/download options, the pre-application wrapper, console initialization
and reset handlers to remain linked. This catches a build that silently omits
USB startup even though the board declares the console device. Host-side USB
enumeration and DTR/RTS reset behavior are separate physical-device checks.

## Observed hardware behavior and remaining checks

The identified ESP32-S31 board reports CoreBoard version 1.2 through eFuse. A
complete 16 MiB backup was captured before writing. All six installed images were
hash-verified while preserving NVS. The current firmware reaches Ready, initializes
the native UI, exposes application USB CDC and remained running throughout the
observed UI and BLE tests. No startup abort or missing-font failure recurred.
The device's existing Wi-Fi configuration reconnects successfully.

The Apple Development-signed macOS Corallium app discovered ESP-Mosaico and
completed an encrypted connection. Device logs confirmed authentication success
and notification subscription; the app received device.info, device.status and
periodic status updates. App time synchronization returned success and displayed
UTC+08:00 with source Corallium. A Wi-Fi configuration request returned success,
progressed through connecting, and reported connected with a DHCP address.
The app's visible activity log contains operation/result metadata, not Wi-Fi
credentials. After more than two minutes enabled, disconnection and a new scan
still allowed a successful connection. One intervening scan found no device;
this observation does not establish a radio range or discovery-time guarantee.

The official CDC normal-reset sequence completed a powered software restart.
The saved enabled Bluetooth state was followed by discovery and another successful
app connection; Wi-Fi rejoined and UTC+08:00 remained saved. Network time was
already synchronized when the app read status, so this is not offline RTC-retention
evidence. The saved-off and factory-reset BLE paths remain host-tested only.
The CDC download-reset sequence also reached ROM USB, but its warm RAM-stub start
did not respond; a cold BOOT/POWER entry was required for reliable flashing.

Local volume interaction opened the official output codec on the physical board.
Audible output, the final page layout and mute restoration still require user
confirmation; a successful codec-open log alone is not acoustic acceptance.
Compilation and these observations do not establish battery accuracy, calibrated
power savings, RTC retention, long-term stability or achieved interactive FPS.

After confirming the CoreBoard revision, use the same device for comparisons:

| Area | Procedure and observable result |
| --- | --- |
| USB | After leaving ROM download mode, application Type-C CDC must enumerate and expose startup logs. Reopen the console and verify the official DTR/RTS download-reset sequence returns to the ROM loader. |
| BLE | Fresh/reset device stays off. Enable locally: ESP-Mosaico appears and stays discoverable beyond two minutes. Encrypted subscription/RX succeeds; unencrypted RX fails. Verify saved on/off after reboot, Connected state, local close, disconnect/reconnect framing and no status event before device.info completes. |
| Audio/UI | Confirm two button columns and both sliders respond at their visible positions. Releasing volume/unmuting sounds once; mute produces silence and restores the previous level after reboot. No startup/background sound or microphone task. Confirm shortened Battery values and independent protocol page, with the three Home shortcuts and a working Weather card. |
| Wi-Fi | Set from app after local Wi-Fi off, join, inspect local SSID/IP, restart and confirm enabled state; wrong password reports failed; forget erases credentials and remains off after restart. Include a 32-byte SSID. |
| Time | Set without internet; powered software reset should retain plausible RTC time with estimated quality. Full power removal must show invalid/--:-- until app or NTP sync. Measure drift against an external reference. |
| Power | Compare stable brightness/network/load on the same supply. Record independent supply measurements and gauge discharge telemetry separately; charging runtime must be unavailable. Exercise screen timeout and touch/button wake. |
| FPS | Enable native statistics, replay identical launcher swipes/settings scrolling for at least 30 seconds, and separately collect 60 seconds idle. Parse with `tools/analyze_performance.py`; report observed frames and render time rather than the 16 ms target interval. |

The implemented clock-text cache reduces stable minute/date write batches from
60 to 1 per minute by inspection. Quiet Hub dispatcher waits change from 16 to
50 ms, and paused waits to 100 ms. These are verified scheduling changes, not
measured battery-life or interactive-FPS improvements.

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
- Generated factory scene traversal: removed actions have hidden ancestors and
  cannot become visible through their original child visibility bindings.

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
enumeration and DTR/RTS reset behavior still require the physical device.

## Hardware verification still required

No identified ESP-Mosaico board was available to this firmware subtask. No serial
port was opened and no image was flashed. Compilation does not establish display,
BLE interoperability, sleep/wake, battery accuracy, RTC retention or achieved FPS.

After confirming the CoreBoard revision, use the same device for comparisons:

| Area | Procedure and observable result |
| --- | --- |
| USB | After leaving ROM download mode, application Type-C CDC must enumerate and expose startup logs. Reopen the console and verify the official DTR/RTS download-reset sequence returns to the ROM loader. |
| BLE | Boot: no advertisement. Open local Bluetooth: ESP-Mosaico appears. Encrypted subscription/RX succeeds; unencrypted RX fails. Verify Connected state, local close, expiry, disconnect and reconnect framing. |
| Wi-Fi | Set from app after local Wi-Fi off, join, inspect local SSID/IP, restart and confirm enabled state; wrong password reports failed; forget erases credentials and remains off after restart. Include a 32-byte SSID. |
| Time | Set without internet; powered software reset should retain plausible RTC time with estimated quality. Full power removal must show invalid/--:-- until app or NTP sync. Measure drift against an external reference. |
| Power | Compare stable brightness/network/load on the same supply. Record independent supply measurements and gauge discharge telemetry separately; charging runtime must be unavailable. Exercise screen timeout and touch/button wake. |
| FPS | Enable native statistics, replay identical launcher swipes/settings scrolling for at least 30 seconds, and separately collect 60 seconds idle. Parse with `tools/analyze_performance.py`; report observed frames and render time rather than the 16 ms target interval. |

The implemented clock-text cache reduces stable minute/date write batches from
60 to 1 per minute by inspection. Quiet Hub dispatcher waits change from 16 to
50 ms, and paused waits to 100 ms. These are verified scheduling changes, not
measured battery-life or interactive-FPS improvements.

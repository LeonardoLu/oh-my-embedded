# Connected controller and expressive watch iteration

This iteration follows the user's September 6 request and supersedes the older
Core2 local-simulation-only constraint. The earlier implementation is the baseline,
not the acceptance target.

## Required outcome

- Core2 behaves as a Codex Micro-compatible HID controller that connects to the
  installed Codex app. Actual host input and host lighting feedback must be
  distinguished from local animation previews.
- StopWatch remains a watch without a stopwatch. Enlarge and center the bot;
  study the factory firmware's battery, charging, and settings presentation.
- Expand shared expressions and behaviors, direct selection and transitions,
  motion-sensor interaction, and persisted app controls for animation.
- Improve text legibility at each device's native resolution and measure rendering
  cost/FPS on the connected devices instead of relying on frame-rate constants.
- Use parallel implementation and independent cross review by Sol agents at high
  reasoning, as requested.

## Core2 integration evidence

The installed Codex app is version `26.901.41600`. Its bundled Micro service and
Work Louder device kit provide the current interoperability evidence. Only protocol
facts and original implementation belong in this repository; extracted application
code stays in ignored research scratch space.

Core2 uses an original ESP32 with a USB-to-UART bridge. Its USB port cannot become
a firmware-defined HID interface. The implementation therefore targets Bluetooth
HID. The current app explicitly supports both USB and Bluetooth Micro transports.

Discovery requires VID `0x303a`, PID `0x8360`, and usage page `0xff00`. These IDs
are compatibility identifiers; the device's product name must identify it as a
Core2 emulator. They do not establish certification or an official product.

The vendor report has ID 6 and 63 payload bytes: channel (2 for RPC), UTF-8 chunk
length (up to 61), and chunk bytes. Long messages span reports. Host requests are
complete JSON values **without a newline**; firmware responses and notifications
must end with a newline. Host RPC requests include `method`, `params`, and `id`.
Responses echo `id` and contain `result` or `error`.

Required host calls:

| Method | Payload/response |
| --- | --- |
| `sys.version` | Result contains a string `version` |
| `device.status` | Result: `version`, `profile_index`, `layer_index`, `battery`, `is_charging` |
| `v.oai.thstatus` | Per-slot lighting entries: `id`, packed RGB `c`, brightness `b`, effect `e`, speed `s`, optional `sk`, `sa` |
| `v.oai.rgbcfg` | `keys` and `ambient` lighting configuration |

Input notifications use method `v.oai.hid` and params `k` (key), `act` (1 press,
0 release, 2 encoder step), and optional `ag`. Agent keys are `AG00`–`AG05`.
The current app's default action mapping is:

| Key | Default action |
| --- | --- |
| `ACT06` | Toggle fast mode |
| `ACT07` | Approve |
| `ACT08` | Decline |
| `ACT09` | Fork task |
| `ACT10` | Push to talk; audio is captured by the Mac app |
| `ACT11` | Unassigned unless separate microphone keys are enabled |
| `ACT12` | Submit composer |
| `ENC_CW`, `ENC_CC` | Encoder navigation; configurable in the app |

The app can remap actions. Firmware must not present a fixed Review/Debug/New
label while emitting a different default action. Joystick notifications use
`v.oai.rad` with normalized `a` (turns, 0..1) and `d` (distance, 0..1).
Angle 0 points right, 0.25 down, 0.5 left, and 0.75 up. Lighting feedback does not include
task titles or complete task state, so such information must not be invented.

Firmware update/DFU commands must be rejected: official Micro firmware is not
compatible with Core2 hardware.

## Completion evidence to collect

- Successful builds and focused host regressions for all changed modules.
- Source-derived previews at 466×466 and 320×240, checked for clipping/readability.
- On-device frame timing, boot health, and memory evidence after flashing.
- OS HID enumeration, real request/response traffic, Codex connected state,
  a harmless input action, and returned lighting state.
- Independent review of touch cancellation, BLE callbacks/concurrency,
  frame pacing, motion filtering, and saved settings.

### Measured baseline

The previous committed firmware was built in isolated ignored scratch projects
with serial timing instrumentation only, then flashed to the connected devices.
Steady-state windows of five seconds measured:

| Device | Actual FPS | Average full-screen push |
| --- | --- | --- |
| StopWatch | 17.79 | 31.23 ms |
| Core2 | 18.11 | 32.09 ms |

These are observed rates, despite the old 30/25 FPS frame caps. Scratch logs:
`tmp/baseline-watch-timing.log` and `tmp/baseline-core2-timing.log`. The new
implementation must be measured with equivalent windows before claiming gains.

Official background references:
[ESP32 HID example](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/esp_hid_device),
[StopWatch hardware](https://docs.m5stack.com/en/core/StopWatch),
[Core2 hardware](https://docs.m5stack.com/en/core/Core2_v1.3).

# September 6 interaction iteration — validation

Status: the requirement audit is complete. Implementation, native rendering,
real Codex connection, physical HID input and host lighting feedback are verified.
The final touch fix passed host checks/review and was flashed with automatic
reconnection confirmed. See [requirements](interaction-iteration.md).

## Verified implementation

- Shared BotUx has 10 selectable expression modes and eight animation modes,
  eased switching, motion input, shake reaction, amplitude/speed controls and
  reduced motion. Existing moods remain available. The host preview compiles the
  actual renderer, including 60-frame sequences for each animation/transition.
- StopWatch has a centered 310×310 companion sprite (about 242 px orb diameter),
  native FreeSans clock/settings text, direct expression switching, animation and
  wrist settings, battery overlay and automatic charging acknowledgment. It has
  no stopwatch. Preferences persist in NVS; time/date use the hardware RTC.
- Core2 replaces the local task simulation with Bluetooth HID report 6, bounded
  JSON framing, real Micro input notifications and host RGB feedback. Host data
  supplies colors, not task titles. Push to talk controls Mac audio capture.
- Independent cross review covered watch gesture origin/cancellation, settings
  rollback, low-battery/doze expression precedence, clipping, and reduced motion.
  Core2 review covered fragmentation, no-newline requests, exact 61-byte chunks,
  malformed recovery, MTU/CCCD readiness, and callback/main-loop ownership.

## Checks and measured hardware performance

Watch calendar/input/timed-state tests, Core2 HID framing/analog-direction tests, BotUx renderer
regressions, and both PlatformIO builds passed. C++ host checks use warnings as
errors. Temporary diagnostics and reference extraction stay in ignored `tmp/`.

| Device | Previous measured FPS | New measured FPS | Evidence |
| --- | ---: | ---: | --- |
| StopWatch | 17.79 | 39.0 | Five-second physical-device timing windows |
| Core2 | 18.11 | 30.30 | Bluetooth connected to Codex, initialized RPC transport |

StopWatch steady averages: update 0.208 ms, drawing 8.7 ms, partial display push
14.464 ms, maximum frame 25.7 ms. The orb's visible area is approximately 2.25×
the prior size. Free internal heap was 288,412 bytes, largest block 253,940 bytes,
and free PSRAM 7,321,435 bytes. Baseline full-screen push averaged 31.226 ms.

Native framebuffer captures were inspected at 466×466 for face, settings,
expression, appearance and motion. An arrow-pill radius artifact found in the
capture was fixed and recaptured. These are actual M5GFX font pixels from the
firmware canvas, not panel photographs; they establish raster layout, not outdoor
readability or physical panel color accuracy.

Core2 connected steady drawing maximum was 3.77 ms and partial push maximum
was 5.53 ms, with 114,468 bytes free heap and 4,030,639 bytes free PSRAM. The
firmware reported BLE connected, RPC ready, MTU 67, and three initialization RPCs.
Steady FPS remained 30.30 after initialization; the reconnect window was 29.78.
The final native 320×240 Agents, Control and Settings page captures have correct
RGB565 byte order and readable native Font2 text. Both devices were flashed successfully with image
hash verification and booted without reset loops in the observed timing windows.

Final build sizes: StopWatch RAM 24,096 bytes, flash 596,429 bytes; Core2 RAM
46,696 bytes, flash 1,395,737 bytes.

## Connection and interaction evidence

- macOS Bluetooth discovers `Core2 Codex Micro` with keyboard appearance.
- The HID descriptor has an idle keyboard collection and a vendor collection;
  application commands use only the vendor report.
- DIS PnP values account for the pinned BLE library's byte-order implementation.
- macOS HID enumeration reports VID 0x303A, PID 0x8360, usage page 0xFF00,
  release 257 and Bluetooth transport for `Core2 Codex Micro`.
- Actual CodexMicroService logs on September 6 at 03:52 UTC show successful
  `v.oai.rgbcfg`, `v.oai.thstatus` and `device.status` responses. Status contains
  firmware version `core2-emulator-0.1.0`, numeric battery and boolean charging.
  No diagnostic status probe was used to establish this handshake evidence.
- After a device reset, Bluetooth and Codex automatically reconnect. Firmware
  transitions from BLE-only to ready with MTU 67 and receives three initialization
  RPCs again. The connected native capture shows the CODEX badge and host colors.
- macOS initially denied HID open with error 0xE00002E2 even though Codex Input
  Monitoring was enabled. Fully restarting Codex applied the permission; the
  next process established the connection successfully.
- In a physical interaction window, firmware event count increased from 0 to 63
  and host RPC count from 3 to 12. No diagnostic key command was sent by the
  assistant. The SDK does not log each handled HID notification, so handler
  registration messages were not used as input-delivery evidence.
- A final independent Sol/high read-only audit matched RPC types, light payloads,
  AG/ACT/encoder events and normalized analog coordinates against the installed
  SDK. No protocol-shape mismatch was found.
- Continuous dragging exposed redundant full-screen rendering (20.78 FPS with
  49 full transfers in a five-second window). A focused fix removes unchanged
  drag redraws and sends a neutral joystick event immediately on center return;
  the final Core2 build, framing/analog host tests, and an independent touch-state
  path review pass. Center return preserves the drag flag, so lifting afterward
  cannot emit an accidental encoder press. The final image was flashed with hash
  verification; after reset it reconnected to Codex, initialized three RPCs and
  sustained 30.30 FPS. The final drag path was checked by code review and host
  tests; a separate post-fix continuous-drag FPS value is not claimed.
- Official Micro DFU commands are rejected; its firmware cannot run on Core2.

## Design references

The StopWatch factory firmware was studied at commit
`6b4aa125288b6fe9dca661f10159f6e1e5ee785c`:
[M5StopWatch-UserDemo](https://github.com/m5stack/M5StopWatch-UserDemo/tree/6b4aa125288b6fe9dca661f10159f6e1e5ee785c).
Its top-edge battery gesture, automatic charging view, native text, direct settings
preview, 7:1 battery filtering and partial display updates informed this iteration.

BotUx uses original geometry and animation code, informed by
[xAI's design discussion](https://x.ai/news/designing-grok-bot),
[ngocdevv's reconstruction](https://github.com/ngocdevv/grok-bot-emoji) and
[nasawz's reconstruction](https://github.com/nasawz/GrokBot). These are behavior and
design references; proprietary application code and image assets are not included.

## Reproduce device captures and HID discovery

Install `pyserial` for capture and `hidapi` for discovery in a Python environment:

```sh
python tools/capture_device_frame.py <StopWatch-port> tmp/watch.png --screen 0
python tools/capture_device_frame.py <Core2-port> tmp/core.png --boot-wait 8
python tools/micro_hid_probe.py
```

Capture is an explicit diagnostic operation and pauses the device during transfer.
`micro_hid_probe.py` enumerates by default. Its optional `--status` requests only
`device.status`; it sends no key action. A successful probe response alone does
not establish that Codex is connected.

## Requirement audit

| Requested outcome | Evidence |
| --- | --- |
| Core2 connects to Codex as Micro HID | OS vendor-interface enumeration; actual Codex status/lighting RPC responses; automatic reconnect; physical input counter and subsequent host updates |
| Larger centered watch companion; no stopwatch | 310×310 centered sprite, approximately 242 px orb; watch-only RTC/date/settings implementation and native captures |
| Borrow factory battery/charging/settings behavior | Pinned factory-source review; top-edge battery overlay, charge transition view, native text and saved settings |
| More expressions and behaviors | Shared renderer's 10 expression and eight animation modes; actual-renderer sequence/regression checks |
| Direct interaction, transitions, IMU and settings | Watch touch/button selectors and live editors; both apps' motion input, reduced-motion and NVS settings; cross review |
| Readable fonts | Native 466×466 watch and 320×240 Core2 framebuffer inspection, including both settings pages |
| Better rendering performance | Physical baseline and final timing windows; partial rendering/static caching; removal of redundant drag redraws |

Verification limits: framebuffer captures are not photographs or outdoor panel
measurements. Destructive command actions and microphone recording were not
triggered by the assistant; their mappings and press/release semantics were
reviewed against the installed app protocol.

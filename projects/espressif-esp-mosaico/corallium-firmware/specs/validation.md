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
and Python/Clang prerequisites. Covered behavior includes:

- The actual LiquidDuck FLIP solver and native IMU renderer under ASan/UBSan:
  all eight original palettes, solid/density color rules, 720 steps of rotating
  and shaking gravity per style, constant particle counts, finite positions and
  velocities, wall bounds, invalid-input rejection and padded output guards.
  Pixel particles contribute to exactly one cell. Native GSP renders all 24
  style/palette combinations through the actual IMU C implementation with an
  explicit acceleration fixture. Real simulator taps exercise style cycling,
  palette changes and reset. The actual app catalog keeps all three controls
  local and routes the distinct shared Back action to Home, preventing Reset's
  action zero from matching the descriptor's default Back value.
  The Canvas lifetime fixture rejects one submission,
  withholds both frame releases, checks whole-frame hashes remain unchanged under
  backpressure, then delivers delayed callbacks after app teardown. The compiled
  Canvas is one opaque RGB565 RAW resource in the actual IMU bank.
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
- Extracted real Back/activity functions execute with queue/navigation doubles:
  an awake Back press queues activity before exit or loader dispatch, including
  both dispatch error paths. This establishes command ordering; the display
  service regression separately covers brightness restoration.
- The actual bounded USB parser and service task execute with CDC/UI boundary
  doubles under ASan/UBSan. Tests cover malformed/overlength input, rate limits,
  queued input, read-only status, full output slots, bounded CDC writes,
  short writes, slow frame-copy time, cancellation, disconnect/stall cleanup and
  the disabled configuration. Independent zlib checks validate maximum-sized
  frames, padding and Base64 tails. The actual C formatter's four status sections
  and frames also decode through the real host client with fragmented reads.
  Twenty-three client tests cover identity gates, whitelist output, CRC/PNG integrity,
  delayed USB reception, fixed task records/application admission and safe connected/closing DTR/RTS ordering. A simulated
  receive-delay negative control reproduces the old client's rate-limit failure.
  These use explicit host doubles;
  they do not open serial ports or produce physical-device screenshots.
  The MAC check compiles against the pinned SDK's actual address types and checks
  its factory MAC-48 length contract. A negative control reproduces the S31
  default getter's eight-byte EUI-64 write; ASan rejects its six-byte buffer.
  A target-libc negative control models the reviewed Picolibc ELF: the old sender
  pairs global-lock acquire with FILE-lock release and retains the global lock
  after three packets. The current real service task executes with FILE-lock
  APIs forbidden and makes zero calls to them. This is an ABI-aware host model;
  the linked diagnostic archive is also checked for absence of FILE-lock imports.
  The actual task collector runs with 32/64-bit counter doubles, expired name/
  stack pointers and 64-slot growth/overflow. It publishes only copied scalar
  values and fixed names. Fourteen streamed task records, one-second admission
  limits, total/stall deadlines, clock rollback and disabled statistics are
  checked; the actual C transcript decodes through the real client.
- The actual loader and frame-capture wrappers execute with SDK/RTOS/presenter
  doubles. Forty-eight reused 480x10 tiles assemble a complete 480x480 RGB565LE
  frame; missing coverage, failed commit/cancel, invalid bounds, absent fences,
  PSRAM failures, concurrent detach and timeouts reject capture. Unarmed wrappers
  leave the real call and buffer unchanged. Verbatim public UI functions cover
  asleep status without activity, sleeping/foreign capture rejection, dim-state
  preservation, lock release before USB reads and failed-resume recovery through
  the next tap or Back. Verbatim public application-open code and the actual
  catalog/loader path admit all four diagnostic routes without waiting for a
  stalled runtime lock, reject a full queue immediately, and preserve capture
  interruption/wake ordering. The transparent quiesce probe distinguishes
  command acknowledgement from presenter-fence failures with stage fault doubles.
  Later committed tiles overwrite a complete base; later incomplete, cancelled
  or failed rasters reject the copy. Capture refresh admission and restoration
  failures preserve cleanup and screen state. Verbatim native capture handlers
  and platform admission run under ASan/UBSan with explicit SDK boundary doubles:
  inactive/paused/stopping guards, NULL callbacks, native event timestamp, root
  page gates and fixed enqueue-error logs are checked. The native Settings
  fixture also compares complete state/provider/write counters and initial and
  scrolled pixels across the real descriptor callback.
  They verify lifecycle/control flow, not PPA/cache or the
  physical panel.
- Startup ordering and queue ownership: saved controls and local provider/
  callback setup precede UI, association/HTTP starts follow UI, and the actual
  queued Wi-Fi configuration owns/rebinds every string. Prepared START consumes
  the worker's current desired configuration rather than a pre-UI copy.
- Weather illustration classification for unavailable data, MET condition families,
  night fallback and combined precipitation selecting one layer. Home and Weather
  resolve the same six source images at the same scale; real alpha bounds with a
  resampling margin fit both card regions without covering Home text.
- The actual output-only speaker worker against explicit codec/RTOS test doubles:
  silent startup/restore, no output while muted, bounded 100 ms local PCM, and
  draining both 2880-byte and 4096-byte DMA rings before mute/close. The test
  rejects the earlier immediate-close implementation because tone samples remain
  queued. Open/query/write/drain failures and mid-play mute close the codec.
  Works PCM tests check owned copies, clip limits, immediate busy returns,
  preview/PCM serialization, stream gain without master-volume changes, queued
  sound discarded on mute, cancellation before/during playback, stale-finalizer
  isolation and normal DMA draining. This checks control flow with explicit
  RTOS/codec doubles, not physical sound/power or measured cancellation latency.
- `tests/check_works_audio.py` compiles the actual bundled Lua source list with
  its float32/int32 configuration and current stack limit. The real Lua audio
  facade and speaker worker execute the unchanged official Flappy script with
  display/delay/input doubles, exercising ready/flap/crash PCM, busy retry,
  userdata collection, error followed by `lua_close` and stream reuse. This
  checks script compatibility and cleanup, not game rendering or physical sound.
- Generated factory scene traversal: removed actions have hidden ancestors,
  retained apps keep their launcher entries and Home provides Settings, Works and
  Album shortcuts plus a Weather card route. The Home analog-clock objects are
  absent and the six original Weather illustrations are present. The 2×2 button block stays below one
  quarter of the screen; adjacent vertical sliders match pointer hit regions.
  Compact Battery labels fit their compiled font. The compiled Bluetooth
  connection badge uses the enabled/connected visibility predicate;
  its non-interactive geometry stays
  within the button while the enabled tile retains its original color.
  About and the independent protocol detail have separate navigation routes.
- `tests/test_hub_drawer.py` verifies an opaque full-screen drawer independent
  of its compact contents, the original control hit regions, percentage-only
  slider captions, equal 220px widget backgrounds and the bottom collapse icon. Native off/on fixtures check
  all four corners and the lower empty band for exposed Home pixels, plus white
  button glyphs in both states. These
  fixtures force drawer/state properties; they do not exercise the Hub C
  backend, saved-state restoration or physical open/close gestures.
- Settings scene generation: ten padded tint icon assets and 50 icon slots,
  continuous padded selector chevrons, scroll bounds/chooser geometry and independent
  Bluetooth navigation pass. The Wi-Fi password page contains no Bluetooth
  action. The update page, update bindings and actions are absent. Firmware
  inspection rejects linked update-client symbols, the vendor update component
  and its official manifest URL.
- `tests/test_settings_native.py` compiles the shipped Settings C backend and
  actual List templates through the pinned ESP-GSP native bridge. Initial,
  refreshed and touch-scrolled/recycled rows match their RGBA masks on a gray
  background within RGB565 quantization tolerance. The placeholder compiles as
  `rgb565_a8`; font externalization/materialization preserves the image bank
  byte for byte. Runtime off/on tracks render gray/orange, and all four native
  SVG arrows remain a single connected stroke with four pixels of margin.
  Registered observer/callback input doubles exercise actual C scrolling to
  both charging switches and the Power-off chooser/save/return path, preserving
  the scroll position and removing the chooser overlay. Providers, StackView
  navigation and common shell APIs are test doubles; List binding, image
  decoding/upload, scrolling and state-property rendering execute the real C
  path. GSP hit routing and physical touch are outside this check.
- Further icon/layout review covers Works paging, Album arrows, the IMU bubble,
  Bricks shapes, forecast icons and common Back bounds. Album thumbnail cover
  cropping remains intentional. `tests/test_weather_scene.py` checks all six
  actual artwork alpha bounds with a resampling margin, verifying at least five
  pixels above the forecast cards, title presence and clear header condition
  text. Native SUNNY/SNOW/WINDY/THUNDER fixtures use the actual A8 title and QOI
  forecast masks and preserve those separations. These authored fixtures force
  visibility; they do not test the hardware or dynamic C backend state.
- Actual GSPB/GSB/GFB/GRB resources: CRC, bounds and every font reference's ordinal
  and content ID are checked before and after font externalization. Every bitmap
  and SVG reference must exist in the shared GRB. Settings uses its complete main
  bank, including the authored chevron variant; the checker rejects the captured
  failed bundle at asset_ref 50 / resource_id 44. Six resource-bank regressions
  cover missing IDs, shared multi-scene banks, ambiguous banks and malformed data.
  The packed MMAP image must match staged assets byte for byte. The checker rejects the captured
  failing Hub bundle where shell glob order assigned `font10` to ordinal 2;
  numeric font ordering corrects that mismatch. The linked-build checks cover
  16 bundles, 124 font references, 264 bitmap/vector references and 17 packed
  MMAP assets.
- `tools/check_system_assets.py` mounts the built SYSTEM filesystem with the
  factory builder's own LittleFS environment without auto-formatting: packed files
  match staging (148 files), overlay files match staged content, and the three official
  Works launchers/entries plus the local registry are present. Removed Fluid/Toy
  directories and libraries must be absent from staging and the packed image.
  This checks the flash image rather than only the source staging directory.

A separate native audit ran the full Hub C startup and periodic saved-state
refresh with explicit provider doubles. Off/on button glyphs rendered white and
slider glyphs orange before and after switching to raw images; all 14 relevant
crop comparisons were pixel-identical. Both compressed and raw native GRB
directories/payloads matched their actual firmware banks. The ten deduplicated raw resources
preserve RGB565+A8, adding about 46 KiB to the Hub bundle. This audit supports
state/color behavior and the encoding change; it does not establish why the
previous device showed black glyphs or whether the workaround resolves them.

Native GSP screenshots render authored Home/control-center/Settings fixtures and
the actual IMU C app on the host. They provide evidence for generated geometry,
assets and the stated simulated inputs, not hardware runtime, BMI270 direction or
shake response, live Wi-Fi/weather state or physical panel behavior. Liquid-style
sensor response, achieved FPS and repeated entry/exit on the board remain unmeasured.

The full ESP32-S31 firmware compiled and linked with the pinned ESP-IDF checkout,
RISC-V toolchain `esp-16.1.0_20260609`, and the official board generator. The build
disables performance logging. Image generation and partition-size checks passed.
The application uses approximately 4.02 MiB, with about 49% of the smallest app partition free.
Inactive vendor helpers can produce unused-function warnings; these are not
treated as hardware evidence.

`tools/check_firmware.py --build <prepared-checkout>/build` passed against the USB-enabled
firmware. It checks the generated configuration, actual linked ELF symbols and
linker wrapping flags. It requires TinyUSB CDC and both factory
auto-init/download options, the pre-application wrapper, console initialization
and reset handlers to remain linked. It also requires the separate diagnostic
service, independent RTOS task statistics, queued UI APIs and five device-frame presenter wrappers, while rejecting
the unused Claw CLI configuration and incompatible FILE-lock imports from
the diagnostic archive. Runtime statistics must use the ESP timer microsecond
source, and the CDC TX FIFO must hold a complete 512-byte output slot. This catches a build that silently omits
USB startup or the input reader even though the board declares the console device. Host-side USB
enumeration and DTR/RTS reset behavior are separate physical-device checks.

## Current physical evidence

The identified ESP32-S31 board reports CoreBoard version 1.2 through eFuse.
The application/SYSTEM images were written and hash-verified through its ROM
loader without a firmware backup. Retained boot, partition, OTA-initialization
and UI images match their verified baseline; NVS is outside the write ranges.

Factory CDC initialization and startup logs were already present. The local
Corallium startup did not consume console commands because it did not start the
Claw application; its unused UART CLI option did not supply the missing USB
consumer. The added diagnostics now verify the typed factory MAC, report live
UI/control/task state, admit fixed application routes and return complete
480x480 RGB565 frames with matching block and whole-frame CRCs. The first
available status showed the native Home active with restored brightness, volume,
Bluetooth and idle choices before Wi-Fi association completed. This establishes
local-state ordering for a powered software restart, not an unplugged cold-start
acceptance test.

The initial diagnostic sender introduced a separate target-libc regression.
Actual Picolibc ELF disassembly showed its manual FILE lock acquire/release pair
operated on different mutexes. On the device, the USB task retained inherited
priority while UI work stopped. After removing that pair, Settings became the
active application, frames advanced through navigation and captures, error
counters remained zero and USB task priority matched its base priority. This
explains that diagnostic regression; it does not explain every earlier product
UI fault. The earlier missing Settings GRB resource remains a distinct
bank-assembly fault rejected by the resource checks.

Captured device frames show Settings/Works/Album in the intended Home order,
an orange Bluetooth switch, complete Display selector arrows and the same large
Cloudy dot pattern on Home and Weather. The pull-down covers the 480x480 screen,
has white control glyphs, omits slider explanations and has two equal-height
220-pixel top plates. Works Lab contains Dino and Flappy Bird; rejected Fluid
apps are absent from the packed SYSTEM image. These are the device's submitted
pixels, not optical panel photographs or GRAM readback.

First-resume captures omitted Settings list icons and showed black Weather
forecast rectangles while the user still saw Settings icons on the panel.
Pause releases decoded caches, and the first resumed raster precedes the
ordinary app step. Re-decoding appended runtime resources did not itself dirty
their existing list rows. A same-pixel QOI/PNG comparison reproduced the
snapshot discrepancy, so it did not establish a codec failure.

Native capture refresh now rebinds those visible rows through the supported
List API and the mirror retains their subsequent committed tiles. Device
captures show all six QOI Settings icons across the initial and scrolled lists,
and all five Weather forecast icons with transparent surroundings. Each sample
included a second presentation commit; settings values remained unchanged and
render/runtime error counters remained zero. Repeated scrolled Settings
captures match pixel for pixel. Runtime images retain QOI. The
independent async-decode worker is disabled in the linked configuration and
absent from the live task inventory; fallback media work still runs during
ordinary UI steps. Captures remain submitted-pixel evidence rather than optical
panel acceptance.
Panel wake, charging policy, whole-device shutdown, measured frame rate,
repeated local-job cleanup, Flappy sound and Back brightness restoration still
need their separate physical checks. Host coverage of these paths is not
hardware acceptance.

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
| UI/icons | Confirm Settings/Works/Album on Home and Weather-card navigation. Inspect Network/Corallium/About tint masks, Display selector arrows, scrolling and chooser return. Bluetooth's checked track must match the other orange switches. The Wi-Fi password page must omit Bluetooth. The pull-down must cover all four screen corners with its compact 2×2 controls, icon/percentage sliders and bottom collapse arrow; slider explanations must be absent and both top widget backgrounds must have equal height. Icons must remain white in both toggle states. Check shortened Battery values at their visible hit regions. |
| Weather | Exercise clear/cloud/precipitation and day/night symbols with real provider data. Confirm unavailable/stale states, identical art between Home and Weather, and safe swipes/drawer/lock/app transitions. |
| Works | Open Lab/Recent/Installed; Lab has Dino and Flappy, Installed also has Album. No Fluid/Toy entries remain. Flappy ready/flap/crash sound must follow master volume/mute and stop on exit. Repeated game launches and physical Back must release RAW and return to Works. Check local Recent persistence. |
| Audio | Releasing volume/unmuting sounds once; mute produces silence and restores the previous level after reboot. Confirm silent startup/background and no microphone task. |
| Wi-Fi | Set from app after local Wi-Fi off, join, inspect local SSID/IP, restart and confirm enabled state; wrong password reports failed; forget erases credentials and remains off after restart. Include a 32-byte SSID. |
| Time | Set without internet; powered software reset should retain plausible RTC time with estimated quality. Full power removal must show invalid/--:-- until app or NTP sync. Measure drift against an external reference. |
| Display/power | Exercise all timeout choices, Never, saved/restored defaults, dim/brightness restoration, ordinary child-app screen-off and touch/button wake. Confirm shutdown still follows an already-off screen on battery and GPIO57 permits subsequent power-button restart. Test charging switches while observing gauge state; unavailable samples must block shutdown. RAW/LVGL owners defer idle effects until return. Record independent supply measurements separately from gauge discharge telemetry; charging runtime must be unavailable. |
| FPS | Enable native statistics, replay identical launcher swipes/settings scrolling for at least 30 seconds, and separately collect 60 seconds idle. Parse with `tools/analyze_performance.py`; report observed frames and render time rather than the 16 ms target interval. |

The implemented status/AOD text cache reduces stable minute/date write batches from
60 to 1 per minute by inspection. Quiet Hub dispatcher waits change from 16 to
50 ms, and paused waits to 100 ms. These are verified scheduling changes, not
measured battery-life or interactive-FPS improvements.

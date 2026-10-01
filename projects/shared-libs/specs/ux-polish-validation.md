# UX refinement validation — 2026-09-06

Scope and ownership are recorded in [ux-polish.md](ux-polish.md). GPT-6 medium
owned BotUx research, design and implementation; Sol high agents owned StopWatch
and Core2 and independently reviewed the other application's interaction flow.
The parent integrated, built, flashed and inspected native raster output.

## Automated checks

All seven standalone C++11 checks compile with `-Wall -Wextra -Werror` and pass:

- StopWatch calendar math, input semantics, timed state and watch interaction.
- Core2 HID framing, analog input and battery double-tap.

Coverage includes both buttons held continuously for 3 seconds, consuming both
releases, scroll versus tap, long-press release, HSV primary/secondary colors and
sector continuity, motion dead zone/hysteresis/cooldown, ambient dwell/no immediate
repeat, and double-tap timeout/cancel/timestamp wrap. The existing calendar test
explicitly checks that 2026-09-06 is Sunday. The watch now calculates weekday from
the RTC numeric date instead of trusting its stale weekday register.

The real BotUx host renderer also passes all preview assertions, including
40/72/200/310 px bounds, expression placement and edge coverage, continuous
transitions, normalized IMU input, reduced motion and sparse update intervals.
Explicit Neutral reopens the eyes even when the underlying mood is Sleepy or
Waiting; Auto continues to follow mood. The 40 px preview uses the same Joy/Wink/
Alarmed eye morphs as the larger character.

`tools/capture_device_frame.py` passes Python compilation and accepts numeric
Core2 pages or `vNN` StopWatch diagnostic pages. `git diff --check` is clean.

## Firmware and raster evidence

Both applications were built and uploaded with PlatformIO and explicit device
ports. The upload tool verified the written flash hashes and reset each device.
StopWatch uses `/dev/cu.usbmodem214201`; Core2 uses
`/dev/cu.usbserial-5C9A0591461`. No HID action was injected for this iteration.

| Final firmware | Static RAM | Flash |
|---|---:|---:|
| StopWatch | 24,200 B | 600,941 B |
| Core2 | 46,728 B | 1,397,701 B |

Native RGB565BE captures cover StopWatch face, first/scrolled settings,
first/scrolled personality, expression, appearance, motion, HSV color,
display/sound, format and battery panel. Core2 captures cover both controller
pages and all four settings pages. These are actual firmware canvases, not
photographs of the panels. The capture transfer deliberately pauses rendering;
its low FPS samples are excluded from ordinary animation measurements.
An ignored multi-page StopWatch capture helper intermittently timed out during
transfer. A fresh single-page capture completed successfully; the final firmware
was rebooted and checked separately from capture traffic.

The raster review corrected the invisible Core2 close icon (line drawing needs
an explicit color), the redundant page-count dot and StopWatch editor footer
corners outside the circular panel. Footer geometry and matching hit targets
moved up 18 px; final footer pixel checks found no visible pixels outside the
466 px circle. Seconds and AM/PM share the main time line, keeping the full date
short enough for the narrower lower edge. Direct PNG pixel measurement confirms watch title ink begins at
y=37 and ends at y=54; an initial visual-tool impression of cropped titles was
incorrect and did not result in a title-layout change.

The watch battery panel adapts the factory top-edge gesture and compact sliding
panel. Current connected hardware reported a nearly full battery without active
charging; the green fill/bolt path was checked in source, not claimed as an
observed charging-state photograph.

## Rendering tradeoff

The parent first flashed an isolated benchmark: the previous committed watch
application with only the new shared renderer. This separates antialiasing cost
from the new settings UI. Its steady measurements were:

| Watch renderer | FPS | Average draw | Average display push |
|---|---:|---:|---:|
| Previous renderer | 39.0 | 8.7 ms | 14.46 ms |
| Antialiased renderer | 30.4 | 16.73 ms | 14.44 ms |

The old 310 px bot region contained only three distinct RGB colors; the native
AA benchmark contained 137, including intermediate boundary blends. This is
raster evidence of antialiasing in addition to source/SVG inspection. Ellipse
coverage and capsule boundary blending require no extra canvas or frame-time
allocation. Alternate square/hex silhouettes and discrete status glyphs retain
native M5GFX primitives; the AA claim applies to the default orb and eye capsules.

The integrated watch held approximately 30.4 FPS, with about 288 KB free heap
and 7.32 MB free PSRAM. Core2 held 30.30 FPS after capture completed, with roughly
114 KB free heap and 4.03 MB free PSRAM; steady partial redraws took at most
4.23 ms to draw and 5.54 ms to push. Static settings only redraw when necessary.

## Protocol and review boundaries

The verified Micro identity, framing and analog implementation are unchanged.
The actual Codex desktop log recorded replies to `v.oai.rgbcfg`, `v.oai.thstatus`
and `device.status` after the new Core2 firmware booted, including a complete
exchange at 05:22:01 UTC. Final firmware telemetry confirmed
`ble=1 ready=1 mtu=67 rpc=8 events=0`. Bluetooth and the app-ready
icon therefore represent distinct real connection stages. A serial frame dump
can temporarily interrupt the link; free-running reconnect and steady telemetry
are checked after diagnostics finish.

Cross review checked entry gestures, cancellation/release paths, scrolling,
hierarchical returns, live preview/save/rollback, motion/ambient priority,
color-picker continuity, and retained Core2 control routing. It also corrected
Googly pupil contrast and the full-versus-partial preview origin. Host checks,
source review and native captures establish this iteration's acceptance; they
do not assert that an unseen physical touch sequence or charging transition was
performed by a person.

Generated previews, serial/build logs and PNG captures remain ignored under
`tmp/ux-polish/`; the requirements, implementation contracts and this evidence
record are durable in `specs/` and component documentation.

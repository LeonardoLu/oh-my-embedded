# StopWatch settings carousel

2026-09-16 implementation and host acceptance for the reorganized StopWatch
settings experience. This document supersedes the old top-level vertical menu,
the combined Display & Sound editor, the separate top-level Date, Format and
Layout entries, and the face hold shortcut to Bot Personality.

## Hierarchy

The top level contains five pages in this order: Time, Bot, Display, Sound and
Power. Each page presents one centered 200 x 200 RGB565 icon on pure black, its
localized white name, five position dots, and fixed left/right chevrons. There
is no card surface, frame or current-value caption. The layout follows the
factory StopWatch launcher while retaining this app's Settings title and Done
path. The five symbols come from one Phosphor Duotone family (clock, robot,
sun, speaker-high and battery-charging-vertical). A deterministic host tool
applies the category gradients, sheen and halo before RGB565 quantization;
firmware never parses SVG or computes those effects.

- Time opens one vertical submenu containing Time, Date and Format. Time and
  Date retain `RtcClock`'s paired draft behavior: when RTC state is lost, saving
  either half advances to the other half, and the draft is committed only after
  both are valid. Done returns to the carousel. Long A and Power/Home preserve
  their existing cancel behavior.
- Bot opens the complete Bot Personality list.
- Display contains brightness, theme, indicator LED, physical-button effect,
  Bot description visibility and the Time/Bot-description top placement. This
  preserves both controls from the former Layout editor.
- Sound contains the master sound switch plus startup, button and alert sound
  switches. Keeping the master switch visible lets a migrated `sound=false`
  preference be re-enabled before its channel switches have any effect.
- Power contains power saving, dim brightness, dim delay, automatic screen-off
  delay, wake method, charging keep-awake, forced screen-off, and the start and
  end hours. The editor caps dim brightness at normal brightness; lowering
  normal brightness also lowers an incompatible dim setting. Runtime policy,
  forced-hour semantics and the 60-second user wake window are specified in
  `projects/m5stack-stopwatch/bot-ux-watch/specs/watch-power-settings-iteration.md`.

The fixed round-screen Done target remains available on every non-face screen.
At the top level it is restrained gray/white text on black; submenu and editor
footers retain the filled segment. Settings Done saves and returns to the face.
A submenu Done returns to its parent, and editor Done saves to its parent. The
face still opens Settings by a double tap on the clock or the A+B three-second
chord. A long stationary press on the Bot now only completes the temporary gaze
interaction; it does not open Bot Personality.

## Carousel input

Horizontal touch motion promotes at 14 pixels. Before that threshold the
original icon/name target stays captured, so a small drift can still activate
it. A vertical-dominant movement of 20 pixels owns and consumes the gesture;
after horizontal promotion the sequence cannot click an item. Motion follows
the finger in pixels within the first/last bounds. Release projects recent
velocity, chooses one page and eases to its exact 466-pixel snap position. A tap
that interrupts an in-progress snap resumes the same target. The fixed screen
chevrons select the previous/next page. B advances through all five pages and
wraps from Power to Time; A opens the selected page. Long A and the fixed Done
target return to the face. The item hit region ends above the dots, so tapping a
page indicator does not open a setting.

Diagnostics keep the established Settings pages: `v1` is the first item, `v8`
is the last item, `v18` is a fractional position and `v19` animates the complete
horizontal range. `v23`, `v24` and `v25` expose Time, Sound and the lower Power
rows respectively. Direct `td`/`tm`/`tu` replay still passes through the same
production pointer path; it is not physical touch-controller acceptance.

## Memory and rendering

The existing 466 x 466 full canvas remains the only Settings framebuffer. The
carousel redraws and submits only its 466 x 280 moving-content band while
dragging or snapping. That band contains the full icon, arrows, name and dots;
the static title and Done footer remain outside it. Option
editors continue to use the existing 350 x 278 clipped list viewport. The five
200 x 200 RGB565 source icons would occupy 400,000 decoded bytes. A lossless,
row-local PackBits representation stores them in 97,977 bytes of flash and
decodes through one fixed 200-pixel scanline (400 stack bytes). The numeric
RGB565 row is written directly into the existing canvas as RGB565BE, avoiding
one M5GFX image transaction per decoded row. It adds no framebuffer or heap
allocation. An 8-bit indexed
version was rejected because it would reduce the gradients that define the
factory-inspired visual; sharing the live Bot preview sprite would couple two
independent render paths. The established buffers remain: full canvas 434,312
bytes, Bot 163,592 bytes, preview 63,368 bytes, HUD 83,880 bytes, button scratch
59,840 bytes and button masks 754,785 bytes.

A bounded implementation comparison also evaluated storing symbol alpha masks
and composing color gradients and radial glows at runtime. A single compressed
mask can reduce flash while retaining the same 400-byte RGB565 output row, but
multiple masks are needed to preserve anti-aliased edges, independent glow and
highlight layers. Even a favorable host lower-bound benchmark, using an
uncompressed mask and a precomputed glow, took 1.4--1.85 times the exact RGB565
decoder per icon. This is not an ESP32 performance measurement; it establishes
only that runtime composition adds meaningful per-pixel work before mask decode
and display submission. The production path therefore composites the source
symbol, gradients and glow offline, then preserves those pixels with lossless
RGB565 PackBits. The on-device `v19` diagnostic remains the frame-time check.

## Host acceptance

`test_ui_controls` covers all five item centers, fixed chevrons, the dots'
non-interactive band, a fractional two-item position, vertical-drag rejection,
Time submenu targets, all Display/Sound/Power rows, a projected flick, settling,
interrupted settling and B wrap. Localization and typography tests cover the
new English/Chinese titles, launcher labels, option pairs and glyphs.

`test_settings_carousel_render` uses the same production carousel, icon and
arrow-row renderers with the native 24/28 px generated fonts. It produces full
466 x 466 RGB565 screens under `tmp/host-checks/settings-carousel/` for all five
English launcher pages, Chinese Power and the Chinese forced-schedule rows. A separate
asset test losslessly decodes all 40,000 pixels of each icon, checks their FNV
hashes, clipped drawing at both horizontal edges, typed RGB565 submission, zero
heap use and the 400-byte working bound. Visual inspection confirmed that icon,
name, title, chevrons, dots and Done do not collide, and that the lower Power
values do not overlap or clip.

Durable copies of the native host rasters are retained as
[all five English pages](assets/watch-settings-en-five.png),
[English Time](assets/watch-settings-en-time.png),
[English Display](assets/watch-settings-en-display.png),
[Chinese Power](assets/watch-settings-zh-power.png) and
[Chinese lower Power rows](assets/watch-power-zh-bottom.png). They are generated
from the production geometry and renderer; they are not an HTML approximation.

The CJK corpus added exactly eight glyphs: `充始常强提束省结`. A parser compared
each pre-existing glyph's metrics and packed four-bit coverage with `HEAD` after
regeneration using the repository-pinned Noto Sans SC font and Pillow 12.3.0;
all 350 prior glyphs were byte-identical in all four CJK sizes. Each face now
contains 358 glyphs.

The complete `tools/check_host.sh` suite passed for the settings hierarchy,
input, power and font implementation in `a0de199`. The later icon-only change
in `33741a5` passed focused `test_ui_controls`, `test_watch_strings`, exact icon
asset and production carousel renderer checks. A clean StopWatch PlatformIO
build then passed from the final source. Commit `67738ca` added the bounded
rendering optimization and capture hardening. Focused `test_ui_controls`, exact
icon/direct-write boundary and byte-order tests, and the production carousel
renderer passed again; the direct-write test covers both clipped edges, fully
offscreen neighbors, adjacent guard bytes, zero heap use and all five source
hashes. Independent static review also confirmed the fixed 466-pixel stride and
16-bit canvas assumptions. The final build used 50,236 bytes static RAM (15.3%)
and 1,210,793 bytes flash (18.5%). The upload artifact is 1,211,200 bytes with
SHA256
`221451da6421f89c50ce36a2d299e75ec965aff737ab7c35c9aa7220be1abf01`;
the complete build log is retained at
`tmp/deployment/watch-settings/pio-build-optimized-final.log`. These checks establish host
geometry, interaction, raster and firmware integration. Physical deployment
and bounded serial acceptance are recorded separately below.

## Deployment state

The final optimized Phosphor build was written to the explicitly inventoried
`/dev/cu.usbmodem214201`. USB serial `28:84:85:44:5B:8C` and esptool both
identified the same ESP32-S3 revision 0.2 target and MAC. Esptool hash-verified
every segment; the write covered the bootloader, partition table, boot-app and
application ranges and did not erase NVS. The upload log is
`tmp/deployment/watch-settings/pio-upload-optimized.log`.

The bounded serial run booted without a panic and reported `RTC boot state=1
valid=1 hold=1`. Persisted UI telemetry contained the four sound switches, all
power fields and `screen_power=0`; `rtc` remained Ready/valid with advancing
time, and `power` reported `key_cfg=0x2b` with the download-lock bit clear. The
current `_charging` sample was 0; this field is battery charging state and is
not the external-power input used by the charging keep-awake policy. Detailed
policy limits are recorded in `projects/m5stack-stopwatch/bot-ux-watch/specs/watch-power-settings-iteration.md`.

On-device `v19` measured 23.9--24.1 fps with 19.16--19.23 ms draw and
18.82--18.92 ms panel push. The preceding 466 x 330 implementation measured
20.6--20.8 fps, 22.49--22.62 ms draw and 22.10--22.19 ms push on the same
diagnostic. The minimal 280-pixel content band and direct row write therefore
improved the measured rate by about 16% without another cache or framebuffer;
it did not reach 30 fps, and no broader dirty-region system was added.

The same continuous session ran `v19`, captured Time, selected Power and
captured again. Both 466 x 466 RGB565BE frames contain complete title,
chevrons, label, dots and Done chrome. Time raw SHA256 is
`9d57095fbc2552ae1e3a5024a1e73eaceafc3d1f9a0f4d083aec3db25aa28281`;
Power raw SHA256 is
`710f4817fba80bee1252c933c81a10eecd3f92c4353b23ce41769abb54a85e28`.
The capture entry clears any inherited clip and requests an independent full
redraw; raw-region checks also confirm that both frames carry identical title,
arrow and footer pixels. Durable renderings are retained as
[device Time](assets/watch-settings-device-time-final.png) and
[device Power](assets/watch-settings-device-power-final.png). The complete run
is `tmp/deployment/watch-settings/serial-acceptance-optimized.log`.

Finally, `v0`, `home`, `physical` and `keys off` restored the face, physical
input and normal carousel state. A final `ui` read showed `screen=0` and
`keys_diag=0`; RTC and PMIC telemetry remained stable, and the serial port was
closed. Earlier bounded auto-reset failures are retained as historical logs,
but the later identified upload and acceptance completed; the PMIC bit readback
does not establish the cause of the earlier transient failure.

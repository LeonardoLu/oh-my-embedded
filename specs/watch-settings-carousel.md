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
  `specs/watch-power-settings-iteration.md`.

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
carousel redraws the 466 x 330 launcher band while dragging or snapping. Option
editors continue to use the existing 350 x 278 clipped list viewport. The five
200 x 200 RGB565 source icons would occupy 400,000 decoded bytes. A lossless,
row-local PackBits representation stores them in 97,977 bytes of flash and
decodes directly into the existing canvas through one fixed 200-pixel scanline
(400 stack bytes). It adds no framebuffer or heap allocation. An 8-bit indexed
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

The complete `tools/check_host.sh` suite passed, including the native carousel
screens, and the StopWatch PlatformIO target built successfully. The build used
50,236 bytes static RAM (15.3%) and 1,210,729 bytes flash (18.5%). The upload
artifact is 1,211,136 bytes with SHA256
`ad8f186ccc582c6adb9f1873852a16787dd86dfb52b5f48be5a1cbad933a0294`;
the complete build log is retained at
`tmp/deployment/watch-settings/pio-build-final-phosphor.log`. These checks establish host
geometry, interaction, raster and firmware integration. Physical deployment
and bounded serial acceptance are recorded separately below.

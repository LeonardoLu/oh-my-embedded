# Watch ambient gaze experience

2026-09-09. This contract replaces the older 26–48 second random mood shuffle and
60 second interaction postponement in the StopWatch app.

## Automatic sequence

Automatic mode always enters visibly as Idle. Idle uses BotUx's canonical Auto gaze,
which is the UpRight pose. It remains for a randomized 5–15 seconds, then changes to
Looking around for 30–60 seconds. Looking around uses BotUx's autonomous full-range
gaze. It then changes to one of Idle, Listening, Thinking, Happy, Working, Waiting or
Done for 5–15 seconds before returning to Looking around. This alternating sequence
continues while automatic mode remains eligible.

Only one transition can occur in one host update. A late frame schedules the next
complete phase from that frame rather than catching up through invisible states. All
deadline comparisons support unsigned `millis()` rollover.

## Precedence and interaction

Manual mood selection, a non-Auto expression, settings and editor screens, doze, low
battery, transient poke/happy reactions, temporary directed gaze, and an active input
gesture take precedence over the automatic sequence. The sequence cannot advance behind
those presentations. When automatic behavior becomes eligible again, it restarts at Idle
with a new 5–15 second interval. B double performs the same restart and clears the saved
expression/action overrides as before.

A or B may manually select all 14 moods, including Looking around. Manual choices stay
held until B double; the Watch scheduler never replaces them. A face contact temporarily
presents Idle and follows the contact for 2,200 ms. A point inside the actual bot body
requests Front. A point outside supplies the normalized center-to-contact direction.
When the temporary gaze expires, a manual mood is restored; automatic mode begins its
new Idle dwell. Settings gaze remains an independent persistent field and resumes after
the temporary target.

## Host validation

`test_watch_interaction` checks both duration ranges, repeated phase alternation, safe
mood bounds, unsigned rollover, a late update, and pause/resume entry through Idle.
`test_companion_presets` checks the complete 14-mood manual deck, the safe ambient set,
and temporary touch presentation/restoration including manually selected Looking around.
These are deterministic state and presentation checks; they do not claim physical touch
or display acceptance.

The native CJK atlases were regenerated from the pinned Noto Sans SC source with the
repository's existing Python 3.14/Pillow 12.3 environment. A glyph-by-glyph parser check
against the prior Cjk18, Cjk22, Cjk24 and Cjk28 files found zero changes to the metrics or
coverage bytes of all 340 existing glyphs. The only additions are `处` (U+5904) and `看`
(U+770B); `到` was already present. Sorted insertion changes later byte offsets and the
generated source line wrapping, without changing existing rendered glyphs.

## Validation result

The focused Watch interaction, companion preset, string and native typography checks
pass under C++11 with `-Wall -Wextra -Werror`. The final `m5stack-stopwatch` PlatformIO
build against the frozen shared BotUx source uses 48,928 B RAM and 1,061,825 B flash;
the generated firmware image is 1,062,192 B. This validation includes no serial upload
or physical hardware acceptance.

## Authorized device deployment

The user later explicitly authorized flashing the StopWatch. Commit `7d29faa` was built
and uploaded with PlatformIO environment `m5stack-stopwatch` to the explicit port
`/dev/cu.usbmodem214201`. Esptool identified an ESP32-S3 revision 0.2 with 8 MB embedded
PSRAM, USB Serial/JTAG and MAC `28:84:85:44:5b:8c`. The normal application upload did
not erase NVS. It wrote the bootloader, partition table, boot application and the
1,062,192 B firmware image; esptool reported successful data-hash verification for all
four regions and performed a hard reset. The firmware SHA-256 is
`134ecc71d77fa9b9b63bd29680d4ae5512da88bd31354451a5a28d61cadf9c53`.

The bounded production boot reported `IMU=1`, `keys_mask_ms=24` and the expected
`keys_mask_bytes=754785`, with no sprite, HUD or mask allocation failure. A read-only
`ui` query reported Face, automatic Idle, Auto expression/action/gaze, diagnostics off,
a valid unpressed power input, and the persisted Chinese language, sound-off and button
effect-off settings. The safe framebuffer tool sent only the `c` command and captured a
466×466 native Face canvas showing the canonical upper-right Idle gaze, 98% battery and
the live clock/date. The serial port was closed afterward.

Full chip, upload and boot logs plus the native capture remain ignored under
`tmp/watch-ambient-deploy/`. The capture validates the firmware render target rather
than panel optics. No injected contact, physical touch, button, IMU-motion, charging or
long-duration ambient-timing acceptance is claimed by this deployment.

## Orb-speed diagnosis and redeployment

A later read-only NVS capture found the device's saved motion profile unchanged at
wrist response Off, motion amount 2/5 and animation speed 2/5. The Watch maps those
levels to 0.55 motion amount and 0.73 animation speed. The wrist-response setting was
also incorrectly passed to BotUx as reduced motion, adding a 0.20 phase multiplier
when wrist sensing was Off. With the former 3,600 ms Thinking and 4,400 ms Working
base periods, the resulting wall-clock loops were approximately 44.83 seconds and
54.79 seconds (`base / (0.55 * 0.73 * 0.20)`). Device telemetry showed this was a
phase-scaling problem rather than a stopped renderer: the former implementation held
41.4 fps for Thinking and 38.7 fps for Working in steady five-second windows.

The Watch now uses wrist response only to gate IMU input. It no longer changes BotUx
reduced-motion state, and the saved Off value remains Off. Thinking and Working use
1,600 ms and 1,400 ms base periods, a dot radius of `max(0.70, 0.014 * bodyRadius)`,
and 32, 48 or 72 points by render size instead of 48, 72 or 96. With the device's
unchanged amount and speed levels, the effective loops are approximately 3.99 seconds
for Thinking and 3.49 seconds for Working (`base / (0.55 * 0.73)`). The shared BotUx
reduced-motion API remains available to hosts that explicitly offer that behavior.

The integrated firmware was built with PlatformIO environment `m5stack-stopwatch` and
uploaded to the same explicit `/dev/cu.usbmodem214201` ESP32-S3 target without erasing
NVS. The build uses 48,928 B RAM and 1,061,789 B flash; `firmware.bin` is 1,062,160 B
with SHA-256 `8af0d34570b6578bbe4bcea7a760e06598d24d71aac984bca578ff28914ab930`.
Esptool again identified revision 0.2, 8 MB embedded PSRAM and MAC
`28:84:85:44:5b:8c`, verified the data hash for all four written regions and performed
a hard reset.

The bounded production boot reported `IMU=1`, `keys_mask_ms=24` and
`keys_mask_bytes=754785`, with no sprite, HUD or mask allocation failure. Separate
steady telemetry windows, collected before each framebuffer transfer, held 40.3–40.4
fps for Thinking (8,668 us draw and 12,720–12,721 us push) and 39.1 fps for Working
(9,442–9,448 us draw and 12,720–12,721 us push). Both remain above the 30 fps device
acceptance threshold. Read-only `c` captures at the native 466×466 canvas showed the
larger sparse depth-scaled points and the intended dark band through the Working
vortex. A final production reset restored Face in automatic Idle (`manual=0`,
`mood=0`, `effective=0`) and the serial port was closed.

The NVS image, build and upload logs, telemetry, boot output and native captures remain
ignored under `tmp/orb-speed-audit/` and `tmp/orb-speed-deploy/`. These observations do
not claim panel optics, physical controls, IMU response or long-duration acceptance.

## Larger orb-scale deployment

The subsequent readability pass expands both dot-shell moods to the visual area of the
ordinary anthropomorphic body. Large canvases use a `0.95 * bodyRadius` shell, a dot
base of `max(0.90, 0.030 * bodyRadius)`, and 32, 64 or 96 points by render size. The
Thinking breath amount is 0.11; depth opacity starts at 0.12 with a 1.35 exponent.
Small canvases retain a 0.85 shell scale, and translated shells constrain their center
to keep the complete point cloud inside the existing safety margin. The 1,600 ms and
1,400 ms base periods and the Watch's wrist-sensing fix are unchanged.

Native 466×466 captures of Idle, Thinking and Working on the saved wrist-Off,
amount-2/5, speed-2/5 profile show the expanded shells at approximately the ordinary
body footprint, with materially larger depth-scaled points and the intended open dark
band through Working. The renderer optimization preserves the approved images: a
320-frame corpus spanning both moods, five sizes, multiple motion profiles and eight
phases remained byte-identical. The optimized disc path retains per-pixel background
and overlap compositing while avoiding repeated blend work for flat background
interiors.

Before that optimization, both larger shells sustained 27.6 fps on the device, with
approximately 19.93 ms draw time. The final firmware sustains 30.1 fps for Thinking
(17.01 ms draw, 12.80 ms push) and 29.7 fps for Working (17.42 ms draw, 12.81 ms push)
in separate steady five-second windows collected before any framebuffer transfer.
Working therefore remains slightly below the internal 30 fps target; no higher result
or physical panel acceptance is claimed.

The final `m5stack-stopwatch` build uses 48,928 B RAM and 1,062,785 B flash.
`firmware.bin` is 1,063,152 B with SHA-256
`a25f5b2390208a88a982e8ed8cacb99fbf7f53f714634895a173cd2f59757d7f`. It was
uploaded to the same identified ESP32-S3 at `/dev/cu.usbmodem214201` without erasing
NVS; esptool verified the data hash for all four regions and reset the device. The
production boot again reported `IMU=1`, `keys_mask_ms=24` and
`keys_mask_bytes=754785` without allocation failures. A final reset and read-only UI
query confirmed automatic Face with `manual=0`, `mood=0` and `effective=0`, and the
serial port was closed. Build, upload, boot, telemetry and capture evidence remains
ignored under `tmp/orb-scale-deploy/` and `tmp/orb-scale-*-deploy/`.

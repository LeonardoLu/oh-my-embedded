# Touch, gaze and feedback acceptance — 2026-09-06

Scope: [touch-gaze-iteration.md](touch-gaze-iteration.md). Implemented jointly by
GPT-6 medium Bot/shared-UX agents, a Sol high Core2/review agent, and the parent
Watch/integration agent. Final firmware uploads, host checks and the 72-command replay used the final
code. The earlier page gallery was followed by a final Motion-page capture.

## Results and reproducible checks

`bash tools/check_host.sh` passes 18 standalone contracts and the Bot raster/temporal
suite: eight Watch, six Core2, and four shared UI/pointer/sound/sender contracts.
The new contact tests cover release coordinates, immediate recontact after scroll,
1,000 ms inclusion, expired taps, leave/re-enter, persistent scroll ownership,
2,000 ms long press and double-tap bounds. Translation tests scan real UI source,
including all Watch string mappings, enum labels and keyboard action widths.
Shared Cjk18 has 339 glyphs and 43,097 coverage bytes. Sound and font suites also
passed ASan/UBSan in the shared agent's checks.

The [actual SDK probe](../../../shared-libs/ux-components/test/sdk_touch/README.md) compiles pinned
M5Unified `Touch_Class.cpp`, not a duplicate of its state machine. It reproduces
lost recontact following flick_end (10→8) and drag_end (14→12) without an intervening
idle update, and coordinate freezing below the default 8 px threshold. Watch now
ends the SDK classifier and samples `M5.Display.getTouch` at 8 ms intervals into
`TouchContact`, preserving the last valid release point. The diagnostic `contact`
command enters this same host edge/input/UI path. Low-level CST touch acquisition
still belongs to M5GFX.

Native Watch replay passed **72 correlated commands**, acknowledgement RTT **2–98 ms**:

- Immediate scroll-release/recontact; last scrolled row and editor Back.
- Fixed Done at the root list and Back at the personality list bottom.
- 800 ms release accepted; 1,100 ms release rejected.
- Leave/re-enter Done accepted; release outside rejected.
- Bot held 1.7 s remains home; 2.2 s opens personality.
- Double-tap both upper and lower time/date layouts opens settings.
- A first 37 px edge movement opens the status panel. Closing the overlapping
  upper panel does not seed the clock double-tap.
- Home cancels name editing. Indicator toggle changes the actual PM1 register;
  Back restores its original value. Face/physical sampling restored afterwards.

Serial diagnostic syntax: `contact 1 x y`, `contact 0 0 0`, `ui`, `home`,
`locale 0|1`, `power`, `physical`; append `@N` for correlated UI replies.
Contact injection replaces physical samples until `physical`; use it only during
explicit diagnostics. `home` calls the same cancel/return callback as BtnPWR.

## Power and language

PM1 readback while disabled:

```
POWER read=1 led_ready=1 led=0 home_ready=1 key_cfg=2f boot_key=2f off_cfg=00 boot_off=00
```

Enabling Indicator produced `led=1`; Back restored `led=0`. The status LED is
active high through LED_EN; preference defaults off. Only single-reset-disable
0x49 bit0 is enabled; double-off 0x4A is unchanged. BtnPWR's single-click callback
returns home and restores unsaved editor settings. See the official hardware
sources linked in the requirement document. This verifies register state and the
callback path, not an optical measurement or physical power-button press.

Eighteen native Watch RGB565 captures covered settings/personality, format,
expression, appearance, motion, color, display, name, thinking, layout, language,
gaze, date/time and English/pressed-button variants. Titles, values and translated
keyboard actions render with covered glyphs. Names retain user text. Date periods
use 上午/下午 in Chinese. Centered rows and fixed Back/Done remain separate from
scrolling content. A final nineteenth capture confirms the smaller antialiased
Motion preview clears the first row instead of being covered by it.

## Bot and Core2

Bot exposes nine explicit directions plus compatible Auto. The
[native nine-direction raster](../../../shared-libs/bot-ux/docs/nine-directions-raster.png) and
host tests verify near/far eye proportions, mirrored left/right geometry,
up/down openness and diagonal tilt. The suite checks 3,200 directional cases,
continuous touch-target transitions, bounds and all 960 mood/expression/animation
combinations. In a 200 px render Thinking center travel is 47 px at full motion,
9.5 px reduced and zero at zero amount; adjacent 16 ms steps remain ≤3.5 px.

Core2 native settings checks cycle all six saved volume levels and verify actual
synth gains `0,64,120,180,220,255`. Mute keeps volume, and toggling back restores
sound. The test restored its initial volume 3/audio on. Later gallery captures
observed volume 5, preserved it and restored the initial Dark theme after theme
comparison. Native General, Paper, Warm and Dark frames were inspected. The
contextual LED frame tests cover bounded breathing/flow, reduced/off modes and
preservation of authoritative host hues. No HID key actions or fake host-state
messages were injected.

## Firmware and runtime

Both explicit-port PlatformIO uploads succeeded:

| Target | Static RAM | Flash | Port |
|---|---:|---:|---|
| StopWatch | 37,696 B | 724,489 B | `/dev/cu.usbmodem214201` |
| Core2 | 60,392 B | 1,505,849 B | `/dev/cu.usbserial-5C9A0591461` |

Watch live sound stress: scrolling list 21.2–21.3 fps, Chinese Thinking preview
15.3–15.5 fps, home 34.2 fps. Steady home average draw 14.7–14.9 ms and push
12.7–12.8 ms; heap settled at 176,492 B, largest block 147,444 B, PSRAM 7,349,939 B.
All samples report `sound_fail=0`. Preview redraws the full translated editor,
so it runs slower than the partially updated home. These timings exclude native
image transfer, which deliberately blocks rendering.

Core2 volume validation reported steady 30.30 fps, draw maximum 8.27 ms, push
maximum 5.62 ms, heap 62,404 B, PSRAM 4,030,639 B, `sound_fail=0`, BLE/application
ready and MTU 67. Real Codex logs after upload show replies to `v.oai.rgbcfg`,
`v.oai.thstatus` and `device.status` at 11:48:38–39 UTC. A separate hidapi enumeration
at 11:49 did not detect the interface; that snapshot is not a successful HID
acceptance result and does not replace the observed real app RPC evidence.
Subsequent sound stress again reports BLE/application ready, MTU 67, zero sender
failures, steady 30.27–30.30 fps, draw maximum 8.70 ms and push maximum 5.62 ms.
Heap remains 62,360 B and PSRAM 4,030,639 B. Real app RPC replies resumed at
11:50:33–34 UTC, with another status reply at 11:51:34 UTC.

Ignored raw logs/captures are under `tmp/touch-gaze/`; this document records the
durable results. Native captures are firmware canvases, not panel photographs.
Injected contacts bypass the physical touch IC. Optical LED output, acoustic
quality and finger-on-glass behavior were not measured; no manual-only acceptance
is required to complete this code goal under the repository startup instructions.

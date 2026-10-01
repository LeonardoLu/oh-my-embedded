# Shared UX and companion validation — 2026-09-06

Scope: [ux-components-iteration.md](ux-components-iteration.md). GPT-6 medium
agents implemented the shared Bot and UX products, the native HUD, Core2 settings,
agent-signal projection and LED envelopes. The parent integrated both apps and
performed final cross-review, builds, explicit-port uploads and native validation.
The requested Sol Watch worker could not start because of the active thread limit;
the parent completed that integration. No new user task was created.

## Requirement acceptance

| Area | Implemented behavior | Evidence |
| --- | --- | --- |
| UI edges and fonts | RGB565 coverage shapes and native 4-bit Latin/Chinese fonts on both devices | Native device canvases, intermediate-color raster assertions, font corpus scan |
| Watch scrolling/touch | Linear pixel offsets, drag suppression, final release coordinates, recent-velocity inertia, matching drawn/hit offsets | Input/scroll contracts and fractional-offset native capture; continuous scroll benchmark |
| Watch information | `yyyy/mm/dd {weekday}`, hideable natural Bot caption, top/bottom swap | English, Chinese and swapped native face captures |
| Watch shortcuts | A random preset; B single next; B double idle; idle tap gaze | Click timing/wrap/cancel tests and independent integration review |
| Existing entries | A+B continuously held 3 seconds; long Bot personalization; top-edge battery panel | Existing interaction checks retained; source review and native panel |
| Bot eye quality | One coverage union per swept Joy eye, smooth scalar morphs, no segment seam accumulation | 480 geometry frames, before/after source-derived raster examples, native Joy preview |
| Naming and descriptions | Milo default, English keyboard up to 16 characters, English/Chinese natural phrases | Name/UTF-8 truncation checks; native naming and Chinese face captures |
| All combinations | Independent 12 moods × 10 expressions × 8 animations; private preview Bot | 960-combination host checks and native settings on both devices |
| Idle gaze | Gentle time-based wandering and expiring tap-directed gaze | Geometry/state checks including expiration; no fixed upper-right target |
| Core2 header/cards | Right-aligned colored glyphs, battery number inside icon, host-colored cards with corner index and state | Native live host-connected capture; exact RGB24 projection tests |
| Selected Bot | Color and mood follow selected host signal, including physical A/C navigation | Integration review and native Working/blue card and Bot |
| Critical feedback | Bounded color/sound notices, deduplication, cooldown, silent connection baseline | Signal-model tests; optional audio/notification wiring review |
| Bottom2 | Off/Host/Alive, host-hue preservation, reduced-motion static mode, immediate Off | Pure frame tests; hardware initialization and final telemetry below |
| Shared packaging | Separate text/shapes/input/keyboard/tokens; individual font archive objects | Both firmware dependency graphs; unused-font linkage check; licensed corpus subsets |

No persistent settings are written by diagnostic page selection. Chinese/layout
Watch diagnostic overrides restore the saved in-memory data on exit. Preview
selection does not modify NVS or Core2 host state. English naming does not add an
IME. Existing RTC and NVS behavior remains in place.

## Reproducible automated checks

Run `sh tools/check_host.sh` from the repository root. All 11 standalone C++11
contracts and the Bot preview pass with `-Wall -Wextra -Werror`:

- Five Watch checks: calendar, input, timed state, interaction and companion controls.
- Five Core2 checks: HID framing, analog input, battery double tap, agent signals
  and Bottom2 frame generation.
- Shared UX coverage, font corpus, clipping, scrolling/inertia and keyboard checks.
- Bot renderer assertions: complete 960 combinations, 480 eye transition frames,
  40/72/200 px geometry, bounded naming, natural descriptions/UTF-8 truncation,
  preset stepping, temporary gaze and reduced motion.

Shared raster checks compare 80 fractional/clipped round rectangles to the
reference coverage oracle pixel-for-pixel. A 350×58 panel uses 58 solid spans and
fewer than 200 boundary pixel writes. Cjk18 covers all current Chinese Bot/UI
copy (265 glyphs, 31,811 coverage bytes). Linking an archive containing every font
from a Latin14-only program excludes Latin18/24/Clock36/Cjk18 symbols. The final
Core2 ELF contains only Latin14, Latin18 and Cjk18; Latin24 and Clock36 are absent.

Core2 incremental preview validation compares every pixel outside the 112×112
animation rectangle (64,256 pixels) before/after an animated frame, and compares
the composed result against a same-time full-page draw. Both are identical.

An additional integration harness for the real Core2 Settings.cpp checked all
960 preview choices without changing Data, name Done/Cancel, preference writes,
RGB rollback, languages, LED modes and 33 ms preview pacing. Eight native host
raster pages and the Chinese variant were inspected. These are C++ raster checks,
not substitutes for physical device timing.

Cross-review found and fixed duplicate-release inertia loss; restoring manual
presets when an editor is cancelled; stale preset animation after direct expression
editing; physical button selection not refreshing the selected Bot; notification
cancellation; and stale cached lighting across reconnect. Name keys now redraw
on press/release. The selected-card text chooses dark/white ink from relative
luminance, including sufficient contrast on the red host error color.

## Native raster and firmware

Both projects build and upload with the pinned PlatformIO platform/libraries and
explicit hardware ports. Uploads verify flash hashes and reset the devices:
StopWatch `/dev/cu.usbmodem214201`; Core2 `/dev/cu.usbserial-5C9A0591461`.

| Release firmware | Static RAM | Flash |
| --- | ---: | ---: |
| StopWatch | 24,944 B | 681,177 B |
| Core2 | 47,792 B | 1,472,285 B |

Steady Watch face: **34.4 FPS**, approximately 14.63 ms draw and 12.74 ms
panel push, with 203,520 B free heap (largest block 167,924 B) and 7,350,043 B
free PSRAM. Continuous list motion: **22.1–22.3 FPS**, approximately 23.56–23.93 ms
draw and 19.42 ms push. These are measured native limits, not a 60 FPS claim.
The prior iteration's 310 px AA face was about 30.4 FPS; the current smaller
286 px hero and revised renderer recover some frame time while adding the HUD.
After the legacy LED driver and partial-preview update, Core2 preview holds
**30.30 FPS**, with ≤7.86 ms draw and ≤5.48 ms clipped push. Agents holds
**30.07–30.30 FPS**, with ≤7.18 ms steady draw and ≤5.57 ms push. Entry/capture
full frames are excluded from these maxima. Final free heap is 85,164 B and
free PSRAM 4,030,639 B. Consecutive steady telemetry records
`ble=1 ready=1 mtu=67 rpc=3 events=0 led=1 mode=2`, with no RMT write errors.
This verifies the hardware driver initialized and ran alongside the live link;
it does not substitute for an optical measurement of the LED strip.

The native captures are direct RGB565BE firmware canvases, **not photographs of
physical panels**. Watch covers face, English name keyboard, full combination
selectors, Happy/Joy/Wave, Chinese description/date, swapped layout, battery and
fractional scroll. Core2 covers live Agents, Identity, Preview, Lights and Name.
The final scroll panel was inset 4 px after an 8-pixel fringe was found outside
the round panel at a partially clipped row. Final fractional-scroll and battery
PNGs contain zero colored pixels outside the 466 px circular boundary. Raw PNG checks measure actual ink;
Watch editor title ink occupies y=41–58, despite a misleading cropped-margin
impression in the visual tool.

The Watch uses a 286×286 hero and a fixed 466×90 HUD (83,880 bytes) so antialiased
HUD rendering reads the canvas, never the AMOLED. The 178×178 editor preview is
retained. Continuous lists push only the 466×288 band; static chrome stays cached.
Core2 uses its existing 40 px toolbar Bot and a separately checked 112×112 preview
(25,088 bytes). Native timing exposed an 11.21 FPS preview caused by full-page
AA text and selector redraw (55.35 ms draw + 32.4 ms push). The final loop redraws
only `{6,40,112,112}` for ordinary preview frames and clips display transfer to
that region; first entry, interaction and diagnostic capture retain full draw.
No new allocation occurs in draw/input/frame generation.

## Protocol and observation boundaries

[AgentSignal](../../m5stack-core2/bot-ux-codex-core2/specs/agent-signal-contract.md) records the inspected official app source
and exact mapping. Green means unread/new reply, not completion. Orange cannot
distinguish approval from a response request. The wire supplies lighting without
task identities/titles; unknown colors/effects remain Unknown. Slot assignment,
inactivity and host UI overrides limit what lighting alone can prove.

Read-only hidapi enumeration found Bluetooth VID 0x303A / PID 0x8360, usage page
0xFF00. After the release firmware and diagnostics, actual Codex app logs recorded
answers to `v.oai.rgbcfg`, `v.oai.thstatus` and `device.status`, including the
final release exchange at 08:11:25–08:11:39 UTC. No HID user action,
synthetic status RPC or Codex UI automation was injected for acceptance.

Serial frame transfer deliberately pauses rendering and may temporarily interrupt
BLE. Steady frame measurements are taken separately after transfer and reconnect.
Final LED validation uncovered an actual initialization failure in the old
M5Unified IDF4 RMT backend. A local legacy peripheral adapter now selects free
channel/memory blocks, installs the real driver, sends fixed GRB symbols and
280 µs reset, and cleans up failed initialization. The existing strip's quadratic
brightness curve is inverted to keep Alive's default subtle glow above RGB
quantization; details are in [the lighting contract](../../m5stack-core2/bot-ux-codex-core2/specs/core2-alive-lighting.md).
Hardware driver readiness is distinguished from observing actual emitted light;
no physical LED color or speaker sound measurement is claimed. Likewise, host
input contracts and native captures do not claim a person performed a physical
touch sequence or a charging transition.

Logs/PNGs and host artifacts stay ignored under `tmp/ux-components-dev/` and
`tmp/host-checks/`. Durable requirements, contracts, font provenance and validation
are retained in specs/component documentation; no tmp artifact is tracked.

# Living motion, responsive controls and sound — validation

Date: 2026-09-06. Requirements: [interaction-dynamics.md](interaction-dynamics.md).
This supplements the earlier [shared UX validation](ux-components-validation.md).

## Implementation and automated evidence

| Request | Result and evidence |
| --- | --- |
| A presets remain alive | Shared Bot renderer maintains slow translation, breathing, eye motion and appropriate animation. All eight presets at the Watch's 0.55 motion amount change across late 10/40/80 s windows. |
| Back/Done and settings taps | One captured PointerSession replaces competing tap/long-press and scroll recognizers. Buttons tolerate slow holds and small drift; vertical list travel at 14 px becomes scrolling and can never click on release. |
| Consistent targets and pressed feedback | WatchControls resolves the same clipped rows, keyboard rectangles, picker and editor controls used by dispatch. Pressed overlays and a short accepted-click flash distinguish response; page changes reset ownership. |
| Configurable gaze | Auto plus Center/Left/Right/Up/Down/Up Left/Up Right/Down Left/Down Right are additive shared APIs and persisted on both hosts. Watch has a dedicated live editor; Core2 exposes Gaze on Motion. Temporary gaze returns to the selected preference. Explicit directions suppress the expression's global eye-position bias so Listening/Curious cannot cancel a selected direction. |
| Avoid rigid expression combinations | All 960 combinations produce at least four distinct RGB565 frames in late 10/30/50 s windows. Stable faces receive small gaze motion and a stronger Calm body cycle. Explicit motion zero remains static; reduced motion scales movement. |
| Thinking dot edges | All three dots use subpixel ellipse coverage. Renderer assertions require intermediate edge colors; native RGB565 canvas was inspected. |
| Core2 function controls/themes | Semantic colored idle surfaces, high contrast pressed surfaces, HOLD/CLICK/TOGGLE hints and successful PTT held state. Paper/Warm/Dark share one higher-contrast, more saturated palette across deck/settings. Native canvases cover all three. Audio enable and a persisted 0–5 volume level remain independent. |
| Contextual LEDs | Alive adds disconnected gentle flow, bounded accepted-control flow and held-PTT breathing while preserving host hues. Host mode, Off and reduced-motion semantics remain intact. Pure frame tests cover transitions. |
| Shared sound | Six timbres, sixteen cues, envelopes, glides, scales, priority/preemption and an optional signed/unsigned 8-bit PCM product. No large audio assets. Both hosts use the same fixed-buffer M5 adapter. |
| UI independent of playback submission | SDK `playRaw` may wait on a published slot even when `isPlaying()==1`. A dedicated sender owns all playback calls; the UI only renders Source samples. Atomic buffer ownership plus an acquire fence after SDK completion observation protects reuse. |

`sh tools/check_host.sh` runs **15 standalone C++ contracts plus the real BotUx
renderer checks**, with warnings treated as errors. Six Watch contracts include
all editor Back/Done targets, 30 keyboard keys/gaps, fractional list positions,
long holds, slop, cancel/re-entry, scroll reversal and inertia interruption. Five
Core2 contracts retain HID framing, analog input, battery double-tap, exact host
signal projection and LED behavior. Shared checks cover native text/shapes,
scrolling, pointer ownership, synthesis/PCM and sender concurrency.

The sound test exports all cues and an 8-bit PCM demonstration. Endpoints are
zero, default peaks are 2,649–6,227/32,767 without clipping, and output is invariant
to render block size. Tests cover priority, release/mute, rejected submissions,
borrowed buffers and seven queue phases with 90/100 ms producer pauses. A separate
published/adoption test deliberately blocks the sender: 10,000 UI updates return
without SDK access; FIFO PCM and source ownership remain correct. Concurrent
producer/sender/consumer stress covers 200 chunks. ASan/UBSan and sender TSan are additional host
checks, not proofs of acoustic fidelity or actual RTOS scheduling.

Each M5 adapter reserves three 2,048-sample buffers (12,288 B), a 2,048-byte sender
stack and small bookkeeping. Only two chunks may be prepared/submitted ahead,
so normal queued cancellation/preemption latency is at most 256 ms plus release.
The sender never calls Source methods. A stalled driver cannot block UI updates;
`failures()` only counts returned submission failures, not a still-waiting SDK
call. Details and source lifetime rules are in [SOUND.md](../ux-components/SOUND.md).

Bot and shared sound were implemented by separate GPT-6 medium agents. Sol high
implemented Core2 controls/themes/LEDs and independently reviewed Watch gestures
and sender memory ownership. Parent integrated Watch controls, host sound wiring,
diagnostics and device acceptance. Cross-review added the completion acquire
fence. Native review also caught Listening's eye-position bias cancelling Left;
the Bot fix makes explicit direction authoritative while preserving local eye
shape and eased transitions. An additional 1,600 raster cases cover 10 eyed moods
× 10 expressions × 4 eye styles × 4 directions, measuring displacement relative
to the rendered body. Auto retains its existing placement. No physical HID action
is required for these checks.

## Native results

Both builds and explicit-port uploads passed with flash hash verification.

| Device firmware | Static RAM | Flash |
| --- | ---: | ---: |
| StopWatch | 37,640 B | 702,545 B |
| Core2 | 60,392 B | 1,493,493 B |

The final Watch replay ran **40 correlated commands with sound enabled**, all
passing. Round-trip replies were 1–51 ms, median 4 ms. Sequences covered an 11 px
row-tap drift, 800 ms holds plus drift on Done/Back, scrolled menu exits, scrolling
without clicking, color-pad drag across Done without saving, name-key/Back,
Gaze changes/Back and return to face. A pending diagnostic reply had previously
waited for later USB telemetry; explicitly draining each UI reply removed that
transport delay. These reply timings include serial handling and are not touch
sensor latency measurements. Final pressed Done, pressed keyboard, Gaze Left
and Thinking native canvases were inspected. Name/color/gaze test edits were
cancelled, and normal face was restored.

Continuous sound previews were requested every 650 ms through animated scroll,
Thinking preview and face on Watch; through Preview, Controls and Agents on Core2.
Both report `sound=1 sound_fail=0`. Watch list motion measures 21.5 FPS with about
24.3 ms draw / 19.4–19.5 ms push; its full-screen combination preview is
15.1–15.3 FPS, about 31.5 ms draw / 31.3 ms push. Face measured 40.4 FPS in this
sample, which depends on the current mood (earlier Idle sample 34.1 FPS). This
iteration does not claim 60 FPS or improve full-screen preview transfer cost.
Watch free heap was 176,548 B, largest internal block 147,444 B, PSRAM 7,349,939 B.

Core2 steady preview/control/agent windows measured 30.17–30.30 FPS alongside
sound, with steady maximum draw 10.62 ms and clipped push 5.60 ms. Page-entry full
frames measured up to about 88.5 ms combined and are reported separately. Free
heap remained 62,360 B; PSRAM 4,030,639 B. Telemetry reports
`ble=1 ready=1 mtu=67 rpc=3 events=0 led=1 mode=2 sound=1 sound_fail=0`.
Paper/Warm/Dark native captures were reviewed and the original Paper theme was
restored. Optional PcmPlayer and unused Latin24/Clock36 fonts are absent from the
final Core2 ELF.

Read-only HID enumeration found Bluetooth VID 0x303A / PID 0x8360, vendor usage
page 0xFF00. Actual Codex logs after firmware upload record answers to
`v.oai.rgbcfg`, `v.oai.thstatus` and `device.status` at 09:35:16 UTC, followed by
normal status traffic. Firmware driver readiness and real app RPC acceptance
are distinct from optical LED or acoustic speaker measurements.

## Device validation boundaries

Builds use `espressif32@6.13.0`; Core2 pins M5Unified 0.2.21/M5GFX 0.2.28. Watch
pins M5Unified `8530f5377d782e4a25a6c482de2e71c3f75ca8eb` and its GFX/power-driver
revisions in platformio.ini. Explicit upload ports are StopWatch
`/dev/cu.usbmodem214201` and Core2 `/dev/cu.usbserial-5C9A0591461`.

Native PNGs are RGB565 firmware canvases, not photographs of physical displays.
Serial `td/tm/tu` exercises the same Watch handler as hardware touch, but bypasses
the touch-controller IC. `UI seq=N` acknowledgements distinguish commands from
buffered older USB output. Binary captures and pointer sequences are run
separately; frame transfer deliberately pauses rendering and can interrupt BLE.
Steady timing therefore excludes capture transfer and reconnect periods.

No synthetic HID user action, fabricated host status RPC, Codex UI automation or
message to another person is injected. Fast's actual on/off value is unavailable
in the protocol; the UI labels the toggle action without inventing its state.
Real host colors/ready state and successful PTT press/release remain authoritative.
Physical touch accuracy, emitted LED colors and speaker acoustic fidelity are
not measured by firmware captures, waveforms or initialization telemetry.

Logs and native artifacts remain ignored in `tmp/interaction-dynamics/`,
`tmp/host-checks/` and `tmp/ux-sound/`; requirements and contracts are durable here.

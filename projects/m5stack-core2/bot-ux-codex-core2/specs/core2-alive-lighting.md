# Core2 Bottom2 Alive lighting

The Bottom2 driver still uses GPIO 25, ten SK6812 LEDs, three-byte GRB ordering,
and M5Unified's LED strip/color packing. The pinned Arduino toolchain uses
ESP-IDF 4.4.7; M5Unified 0.2.21 leaves its legacy RMT bus init/write branches
unimplemented. BottomLeds supplies a real IDF4 bus using rmt_config,
rmt_driver_install and synchronous rmt_write_items, selecting an unoccupied
channel. It preallocates 240 data symbols plus a 280 µs reset and reports actual
initialization/write failures. IDF5 builds continue to use M5Unified's bus. Allocation occurs only in `begin()`;
frame generation uses fixed stack arrays. Normal updates are limited to 20 FPS
with a 50 ms minimum interval. Explicit Off/brightness-zero actions clear the
strip immediately instead of waiting for the next scheduled animation frame.

## Modes and API

- `setMode(0)`: **Off**, all ten LEDs black in every connection/motion state.
- `setMode(1)`: **Status**. Slots map to physical LEDs 0,1,2,7,8,9 and use
  the same fixed identity RGB24 as their six cards. The host's exact status color
  is decoded but not copied to the strip; status is represented by the rhythm
  table below. Ambient LEDs 3–6 stay black, and local interaction events do not
  alter this mode.
- `setMode(2)`: **Alive**, the default. It starts with the same identity colors
  and status rhythms, then adds selected-agent, accepted-interaction,
  notification and fresh-reply emphasis. Ambient LEDs 3–6 use the selected or
  fresh-reply agent's identity color, so they remain attributable to a real slot.
- `setBrightness(level)`: independent global strip limit: 0/1/2/3 maps to the
  linear output caps near 0/16/38/72 of 255. M5Unified applies the squared gain
  `(brightness+1)^2/65536`, so the adapter uses driver values 0/64/98/135.
  Full-intensity channels quantize to 0/16/38/71 after this curve.
  Passing 16/38/72 directly would severely dim the strip and quantize Alive's
  quiet presence to black at the default level. Level zero overrides every mode, including
  Alive's disconnected presence. Re-enabling brightness preserves the chosen mode.
- `interact(selected, nowMs)`: 900 ms soft traveling highlight around the chosen
  slot, useful when selecting another card or acknowledging local interaction.
- `notify(mask, nowMs)`: one 1800 ms raised-and-settled brightness pulse on the
  six-bit slot mask. The UI supplies the AgentSignal model's deduplicated mask;
  no task state or completion is inferred inside this driver. A zero mask cancels
  active notification feedback immediately.
- The main UI separately derives a **fresh reply attraction** only from a
  deduplicated Working → NewReply event. This is the complete-like presentation
  requested by the product, but its protocol name remains `NewReply`: green does
  not prove that a task is semantically done. It remains active for up to 30
  seconds, is dismissed by screen or physical-button interaction, and is cleared
  sooner if the slot stops reporting NewReply or the app-ready link is lost.
  Persistent green frames cannot renew the deadline, and reconnect baselines do
  not create one. The active mask is passed into `update`; BottomLeds does not
  infer it from RGB values. The card uses a static brightened fill and strong
  outline while active; only the 20 FPS LED renderer animates for the full window,
  avoiding a 30-second full-screen redraw loop.
- `control(nowMs)`: one 900 ms traveling response to a locally sent control.
  It modulates the focused agent identity color and does not introduce an action
  color unrelated to that agent.
- `hold(active, nowMs)`: a breathing ambient response while a real local
  press is held. Releasing the hold finishes with the same bounded control flow.
  It describes only the known press lifecycle and does not infer a host toggle.
- `update(lighting, nowMs, reducedMotion, ready, selectedAgent, freshReplyMask,
  theme)` receives full app readiness and the current theme. Main gates `ready`
  on both the control-plane handshake and a fresh thread-lighting snapshot, so
  reconnect cannot illuminate stale slots.

## Motion language

The projected host signal selects a fixed motion vocabulary. Values below are
per-zone software levels before host brightness and the global strip cap:

| Signal | Normal motion | Reduced Motion level |
| --- | --- | --- |
| Idle | steady 76 | 76 |
| Working | smooth 2.6 s breath, 104–220 | 156 |
| Needs input | peaked 1.25 s pulse, 112–255 | 196 |
| New reply | quick 0.85 s pulse, 130–255 | 220 |
| Error | paired beats over 1.7 s, 82–255 | 242 |
| Unknown / Off | black | black |

Each recognized slot's level is multiplied by its host zone brightness. A host
brightness of zero therefore keeps that slot and any ambient focus derived from
it black even though its status color can still decode successfully. The Core2
LED brightness setting then supplies the independent device-wide cap. Alive adds
a small selected-slot emphasis, a broad 8 second spatial variation, and smooth
bounded event envelopes. The 30 second fresh-reply window strengthens the quick
NewReply rhythm and carries that slot's identity RGB through ambient LEDs; it
does not turn the LEDs host-green or claim semantic completion. The input
`LightingState` is never modified or sent back to the desktop.

Unknown, Off, zero-host-brightness and pre-fresh-handshake agent positions stay
black. Disconnected Alive may show a bounded cool blue-violet breath only on
ambient LEDs 3–6; all six agent positions remain black so controller presence
cannot resemble an active agent. Status mode is entirely black until fresh
app-ready lighting exists.

Reduced Motion fixes each status at the table's level, keeps a static selected-slot
emphasis, and suppresses the moving interaction and notification envelopes. A
fresh reply keeps a stronger static emphasis so it remains perceivable without
motion.
Disconnected presence becomes static too. No repeated blink or abrupt strong
flash is used. Switching an actual host status still changes that host color;
local animation does not conceal a status update.

## Pure host checks

`include/BottomLedFrame.h` contains RGB generation without Arduino/M5GFX or
hardware dependencies. Run:

```sh
c++ -std=c++11 -Wall -Wextra -Werror \
  -Iprojects/m5stack-core2/bot-ux-codex-core2/include \
  projects/m5stack-core2/bot-ux-codex-core2/test/bottom_led_frame_test.cpp \
  -o /tmp/bottom-led-frame-test
/tmp/bottom-led-frame-test
```

Checks cover Off and device-brightness-zero black output, all six physical slot
positions, all three theme identity palettes, exact RGB scaling, every projected
state rhythm, host-brightness-zero and Unknown/Off darkness, Status ignoring local
events, Alive identity-preserving feedback, fresh-reply focus, static Reduced
Motion, disconnected ambient-only presence, time wrap, and bounded changes
between 50 ms Working samples. They also verify that frame generation leaves the
authoritative `LightingState` byte-for-byte unchanged.
`test/fresh_reply_attention_test.cpp` checks event-only start, exact 30 second
expiry, wraparound, interaction dismissal, current-state invalidation, filtering
of NeedsInput, and silent reconnect baselines.
The root integration owns firmware build and actual Bottom2 hardware acceptance.

## Audio and theme feedback

Core2 sets M5Unified's speaker master from its default 64 to 128. The pinned
Core2 configuration uses magnification 16, the reserved channel remains at 255,
and the source is mono. After M5Unified's rate normalization, mono duplication
and final 8-bit shift, the worst-case single-channel multiplier is
`2 * 16 * 128^2 * 255^2 / 2^36 = 0.4961`. The shared synth is limited to
±15,000, so its theoretical app-only output is bounded near ±7,442 before the
int16 clamp. Linear 16 kHz to 48 kHz interpolation cannot exceed its endpoints.
This is software headroom evidence; it is not an acoustic loudness, distortion,
or speaker-safety measurement.

The six synth steps are 0, 16, 30, 45, 96 and 255. Because the speaker master is
squared, steps 1–3 preserve their former effective software drive while step 5
has four times the former maximum sample amplitude. Changing Volume first applies
the new synth gain and mute state, then plays Select at that gain. Step zero and
Audio Feedback Off stay silent. Paper, Warm and Dark palettes use stronger
surface separation, outlines, text contrast and more saturated blue, vermilion
and cyan-blue accents respectively.

Native settings diagnostics on `/dev/cu.usbserial-5C9A0591461` cycled all six
levels from the saved level 5 and observed gains `0,16,30,45,96,255` with
speaker master 128. Each nonzero change reported `busy=1` after applying its new
gain; zero reported `busy=0`. While muted, all six changes stayed `busy=0`, then
the script restored audio, volume and theme preferences. Eight maximum-level
Confirm cues completed with `sound_fail=0`; the following steady sample reported
29.87 FPS, 61,784 B heap, 4,030,639 B PSRAM, app-ready MTU 67 and no sender
failure. This is runtime transport evidence, not acoustic measurement. Native
320×240 General, Paper, Warm and Dark canvases under `tmp/core-iteration/` were
inspected for separation, text readability and saturated accents; they are
firmware framebuffer captures rather than panel photographs.

The final identity-light integration build used 60,456 B RAM and 1,508,577 B
flash. Its 1,515,152-byte image had SHA-256
`9c37d6c3988ecee55db7a1bfc46ab88ece00ac5a9e40da24cc3b5a17fa4054f7`.
The authorized upload wrote that image, verified its flash hash, preserved NVS,
and completed a hard reset. After reset the Core2 restored the app-ready BLE link
at MTU 67 with `events=0`, `mode=2`, `sound_fail=0`, and the Bottom2 strip ready
on GPIO 25. The unified host runner passed every Watch, Core2, shared UX, sound
and BotUx check, including the identity/state LED frame suite.

Read-only `leds` diagnostics sampled four actual generated frames in Dark/Alive
with fresh app-ready lighting. Slot 1 was Working and moved through
`14416B`, `174A7A`, `154471`, and `0F3151`, all scaled forms of its `174A7A`
identity. Slots 2–6 were Idle and reported distinct scaled green, amber, purple,
rose and cyan identities on physical LEDs 1,2,7,8,9; ambient LEDs 3–6 used the
selected slot 1 identity. Every source zone reported host brightness 255, and
the diagnostic sent no HID or fake host lighting. Native Lights-page captures
under `tmp/core-slot-led-status/` show the renamed Status mode and Alive copy
without clipping. These are firmware framebuffer and generated-RGB observations,
not optical measurements of emitted LED color or brightness.

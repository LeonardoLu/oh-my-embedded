# oh-my-m5stack

M5Stack companion firmware built around one shared **bot-ux** animation component,
with a Grok Bot-inspired orb and state-driven motion.

| App | Path | Device | What it does |
|---|---|---|---|
| **watch** | `stopwatch/bot-ux-watch/` | M5Stack StopWatch (ESP32-S3) | RTC watch, date, battery/charging, companion and settings; no stopwatch |
| **codex** | `core2/bot-ux-codex-core2/` | M5Stack Core2 + M5GO Battery Bottom2 v1.3 | Codex Micro-compatible Bluetooth HID controller with host RGB feedback |
| **ux-components** | `lib/ux-components/` | (shared) | Native text/shapes, pointer gestures, scrolling, keyboard and synthesized/PCM sound; products link selectively |
| **bot-ux** | `lib/bot-ux/` | (shared) | The bot animation component — expressions, behaviors, eased transitions, IMU motion and personalization |

## Layout

```
lib/bot-ux/                 shared bot component (PlatformIO library)
lib/ux-components/          reusable native UI products and subset fonts
stopwatch/bot-ux-watch/      watch app (PlatformIO project)
core2/bot-ux-codex-core2/    core2 app (PlatformIO project)
specs/                       task specs
wiki/bot-ux/                 searchable bilingual native animation catalog
tmp/                         scratch / research notes (gitignored)
```

## Build

Each app is a PlatformIO project. The shared `bot-ux` library is discovered via
`lib_extra_dirs = ../../lib`:

Run from the repository root:

```bash
pio run -d stopwatch/bot-ux-watch
pio run -d core2/bot-ux-codex-core2
```

To install on a device, identify its port with `pio device list`, then use the
matching project and an explicit port:

```bash
pio run -d stopwatch/bot-ux-watch -t upload --upload-port <StopWatch-port>
pio run -d core2/bot-ux-codex-core2 -t upload --upload-port <Core2-port>
```

Dependencies: `m5stack/M5Unified`, `m5stack/M5GFX`; platform `espressif32@6.13.0`.
Library revisions are pinned in each app's `platformio.ini`; the StopWatch also
uses the M5PM1/M5IOE1 drivers and OPI PSRAM configuration.

## Watch controls

Press A to choose a non-repeating random mood from the complete shared set. B
advances to the next mood; double-click B within 320 ms to return to automatic
Idle. A face touch temporarily looks straight ahead when inside the bot and
toward the contact when outside it, then returns to the selected mood. Hold A+B
together for three seconds to open settings. Bot personalization is available
from the Bot page in Settings. Hold B to dim the face;
the next touch/button press wakes it without activating a control.

The watch samples raw display contacts every 8 ms and derives its own press/release
edges. A captured control activates on release inside its original target within
one second; 20 px of vertical list travel instead begins scrolling, and stationary
face holds become long presses after three seconds. Button and non-face holds
retain the two-second threshold. Lists follow the finger with
continuous pixel scrolling and inertia, while a scroll gesture never also clicks a
row.

Swipe down from the top edge for the compact battery/charging panel. Double-tap
the visible time to open settings. Settings, Bot Personality and every editor keep
a single fixed Done control outside the scrolling content, with native antialiased
text, rounded touch targets and a live HSV body-color picker. Settings and
Personality use their dedicated horizontal and vertical navigation; preview
editors show two rows and scroll additional choices. A touch clears persistent
button selection and shows only the captured
control’s pressed fill. The M5PM1 power button
returns directly to the face and cancels unsaved editor changes. Its green status
LED is an independent persisted display option, off by default.
Time and `yyyy/mm/dd {weekday}` share one region. A hideable Bot description
occupies the opposite region; Layout swaps the two. Time/date use the RTC;
preferences use NVS. Auto rotates through Idle, Listening, Thinking, Happy,
Working, Waiting and Done, while interaction
delays the next change. Filtered wrist motion adds subtle movement with a cooldown
on shake reactions.

The companion is **Milo** by default. Both devices support a 16-character name,
English/Chinese UI and descriptions, and independent live selectors for all
14 moods × 10 expressions × 8 animations (1,120 combinations). Preview choices do
not overwrite the persistent expression/action or Core2 host state. Gaze offers
Auto plus nine explicit directions: Center, Left, Right, Up, Down and the four
diagonals. Temporary screen-directed gaze returns to that preference; normal
presets retain continuous motion. Idle rests at the canonical upper-right pose;
LookingAround explores the full gaze field, and FaceSide controls automatic or
fixed left/right mirroring. Watch contact and gaze validation is tracked in
[the current gaze specification](specs/watch-gaze-experience.md) and
[the combined acceptance record](specs/companion-catalog-validation.md).

## The bot

`bot-ux` renders into a caller-provided `M5Canvas` sprite and never owns the
display. Hosts control mood, talking, interaction, and appearance. Animation uses
elapsed time and fixed state rather than frame-count timing. See
[the component guide](lib/bot-ux/README.md),
[the searchable illustrated catalog](wiki/bot-ux/intro.html), or its
[Markdown edition](wiki/bot-ux/intro.md). The catalog has bilingual explanations
and actual native-rendered animations for all 32 enumeration entries.

Waiting keeps an alert, patient face; Asleep closes its eyes and breathes deeply
with drifting z marks. Thinking uses a breathing spherical point shell and
Working uses an upward-draining vortex shell; both replace the eyes. All ordinary
faces retain Idle’s baseline proportions; selected gaze changes mirrored
perspective, with downward travel closer to center.

## Core2 controller

The 320×240 command deck exposes six agent keys, Fast/Approve/Decline/Fork/Mic/Send,
encoder navigation, encoder press, and a touch joystick. These are the installed
Codex app's default mappings; the app can remap them. Pair **Core2 Codex Micro** in
macOS Bluetooth settings, then enable/connect Micro in Codex. Core2's USB connector
is a serial bridge, so host HID communication uses Bluetooth.

On macOS, Codex needs Input Monitoring permission to open this composite HID
interface. After enabling it in Privacy & Security, fully quit and reopen Codex.
If the device is paired but stays at `BLE`, check this permission and restart;
the green app-ready icon indicates that the RPC transport has initialized.

The header uses separate Bluetooth and app-ready icons to distinguish pairing
from an initialized RPC transport. Status glyphs are right-aligned, with the
battery number inside its icon. Agent cards use the exact host color, a corner
number and the corresponding lighting status. The selected Bot follows that
slot. Blue means Working, orange Needs input, green New reply, red Error and
white Idle. Green does not establish completion; unknown signals stay Unknown.
A fresh Working → New reply transition adds an attention highlight for up to
30 seconds. Screen or button interaction dismisses it; persistent green does
not restart the window. Authoritative host colors and states remain intact.
Meaningful transitions trigger a bounded highlight and optional sound, with silent
initial/reconnect baselines. See [the signal contract](specs/agent-signal-contract.md). End-to-end connection
validation is tracked in [the iteration record](specs/interaction-validation.md).

Holding MIC sends push-to-talk press/release events; the Mac captures the audio.
Releasing or dragging off stops the hold, and the host decides whether to submit.
Other buttons activate on release in the original target. The bottom left/right
halves switch pages around a compact page count. Double-tap the battery to open
settings for brightness, audio, theme, animation, motion, reduced motion and LED
brightness. The Lights page offers Off, Host and Alive modes plus transition
notifications. Alive preserves host hues with gentle breathing, a traveling
highlight and selection feedback; Reduced Motion keeps it static. Bot
personalization includes naming, language, complete combination preview and live RGB sliders for body, eyes and
accent; valid releases save in NVS and dragging outside cancels the color edit.

Bottom2 uses ten SK6812 LEDs on GPIO25 and replaces the stock Core2 bottom. Screen,
LEDs and optional sound provide feedback. Official Micro firmware updates are
unsupported because Core2 is different hardware.

Both devices share 16 synthesized cues with six timbres, envelopes, glides and
scales; optional signed/unsigned 8-bit PCM is a separate product. A fixed-buffer
sender isolates the SDK playback queue from the UI loop. Sound preferences apply
to new cues, and queued audio drains smoothly. See [the sound guide](lib/ux-components/SOUND.md).

## Host checks and previews

These checks require a C++11 compiler, without an attached device:

```bash
sh tools/check_host.sh
```

The suite runs the platform-independent contracts plus the real BotUx renderer checks.
It covers input timing, continuous scrolling, native font/shape coverage, naming,
1,120 combinations at multiple late time windows, gaze, point-shell antialiasing, HID
framing, exact host status projection, LED envelopes, sound waveforms/PCM and
playback-thread stalls.
Artifacts stay under `tmp/host-checks/`. Host raster timings are not device FPS;
[native firmware acceptance](specs/interaction-dynamics-validation.md) records that separately.

## Design docs

[Task specification](specs/start-up.md),
[implementation decisions and reference sources](specs/implementation-notes.md),
[baseline validation](specs/validation.md),
[HID iteration requirements](specs/interaction-iteration.md),
[HID validation](specs/interaction-validation.md),
[previous UX validation](specs/ux-polish-validation.md),
[previous shared UX requirements](specs/ux-components-iteration.md),
[earlier interaction requirements](specs/interaction-dynamics.md),
[current Watch gaze experience](specs/watch-gaze-experience.md),
[current Watch ambient experience](specs/watch-ambient-experience.md),
[current Thinking/Working orb motion](specs/orb-motion-experience.md),
[previous Watch settings requirements](specs/watch-settings-iteration.md),
[previous Bot vocabulary and catalog](specs/bot-vocabulary-iteration.md),
[current combined validation](specs/companion-catalog-validation.md),
[shared UX library](specs/ux-components-library.md),
[previous shared UX validation](specs/ux-components-validation.md),
[interaction dynamics validation](specs/interaction-dynamics-validation.md),
[previous Watch touch/gaze validation](specs/touch-gaze-validation.md), and the `AGENTS.md` files document the durable contracts. `tmp/` contains ignored
research, generated previews and validation logs.

## License

MIT

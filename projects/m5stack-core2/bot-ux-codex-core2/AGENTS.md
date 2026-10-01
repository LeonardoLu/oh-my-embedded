# bot-ux-codex-core2 — Codex Micro BLE controller

This app adapts the Codex Micro control model to a 320x240 touch display and
connects to the Codex desktop app through a compatible BLE HID vendor report.

## Hardware

See [device hardware](../specs/hardware.md); keep app-specific integration below.

## Module layout

    projects/m5stack-core2/bot-ux-codex-core2/
      include/AudioFeedback.h    short speaker feedback
      include/BatteryDoubleTap.h battery settings-entry gesture state
      include/BottomLeds.h       Bottom2 Off/Status/Alive lighting
      include/CodexLink.h        BLE HID identity, RPC, and native events
      include/HidFraming.h       fixed-buffer report 6 framing
      include/Settings.h         NVS settings and seven-page personalization UI
      src/main.cpp               touch dispatch, rendering, and diagnostics
      test/                      platform-independent framing/input checks

## Product behavior

- Advertise as Core2 Codex Micro using BLE HOGP. Codex compatibility uses VID
  0x303A, PID 0x8360, usage page 0xFF00, and report ID 6.
- A standard idle keyboard report exists only so operating systems offer BLE
  enrollment. Never send keyboard reports or substitute keyboard shortcuts for
  Codex vendor events.
- Six agent keys emit AG00..AG05. Controls emit the app defaults: Fast, Approve,
  Decline, Fork, push-to-talk, Send, encoder, and analog events.
- HOLD MIC sends Codex app press/release events. The Mac records audio; Core2
  does not capture or transmit microphone audio. Dragging off releases.
- A BLE connection is not enough to show Codex-ready. Require a valid app RPC,
  notification subscription, and a negotiated MTU large enough for report 6.
- The app supplies agent/key/ambient colors and effects. AgentSignal projects
  exact RGB24 status colors for solid/breath effects; unknown stays Unknown. Green
  means New reply, never Done. The transport carries no task identity/title or
  separate approval/response reason; preserve those limits.
- Cards show host color/status and corner index; selected Bot follows its slot.
  Notifications are bounded and deduplicated, with silent reconnect baselines.
  A Working → NewReply event starts a complete-like fresh-reply attraction for
  up to 30 seconds; it never renames green to Done and interaction dismisses it.
  Clear cached lighting on connection changes and await fresh thread lighting.
- The top bar uses separate Bluetooth and app-ready icons. The battery is the
  settings entry: two valid taps within 420 ms open settings; one tap is inert.
- Settings provide independent audio enable and six-step volume controls; mute
  preserves the selected volume. A changed nonzero volume previews Select only
  after its new software gain is applied. The app raises Core2's hardware master
  to 128 with bounded single-channel mixer headroom. Settings also provide
  continuous RGB editing for the bot body, eyes, and accent,
  plus style, English naming, English/Chinese descriptions and independent
  14×10×8 previews in a separate checked 112 px canvas. Motion also selects
  Auto plus nine explicit gaze directions: Center/Left/Right/Up/Down and the
  four diagonals. Preview selector values
  do not change NVS or overwrite the actual host Bot. Save valid edits in NVS.
- Command surfaces use semantic colors and pressed feedback. Fast is an action
  that requests a toggle; the protocol does not report its on/off value. Only a
  successfully sent PTT press shows MIC ACTIVE until release.
- Paper/Warm/Dark use Settings::themePalette across deck and settings, with
  high-contrast surfaces, outlines and vivid accents on each palette.
- Reject bootloader/firmware operations; incompatible hardware firmware must
  never be accepted by the Core2 emulator.

## Engineering rules

- Controls activate on a valid release in the original target. All primary touch
  targets are at least 40 px high. Push-to-talk also releases on drag-off.
- Bottom pagers use their full left and right halves as touch targets. Serial
  capture selectors are 0 Agents, 1 Control, 2 General, 3 Bot, 4 Color, 5 Motion,
  6 Identity, 7 Preview, 8 Lights, 9 naming keyboard. `sound` previews a cue;
  `ui-tap x y` only operates an already-open Settings page, never HID controls.
- Avoid allocation in update/draw and BLE framing paths. Cache static UI and
  bound frame pacing so BLE callbacks stay responsive.
- BLE callbacks only latch connection/MTU state. The main loop owns reconnect,
  RPC state, and pending event cleanup.
- Check both sprite allocations before binding or drawing BotUx.
- Settings persist with Preferences; keep display, audio enable, audio volume,
  theme, bot animation, IMU motion, reduced motion, and Bottom2 LED brightness.
- Keep UI text colors independent of BotUx eyeColor; the shared bot's default
  eyes are intentionally dark.
- Prefer host tests, PlatformIO builds, serial metrics/capture, hidapi
  enumeration, and Codex logs over manual-only acceptance.

- `Settings::animate()` requests only the private Bot preview region; use
  `drawAnimatedPreview()` and `previewRect()` for partial frames. First entry,
  interaction and capture still draw the whole page. Do not redraw static AA
  selectors every preview frame (native cost drops from full-page ~88 ms).
- Shared UX fonts/shapes use RGB565 coverage on canvases. Active status glyphs
  have color without tiles; the battery number is inside its icon.
- Bottom2 offers Off/Status/Alive. Both active modes keep the six card identity
  colors and encode known host states through distinct rhythms; Alive adds
  traveling interaction and fresh-reply emphasis. Reduced Motion is static. Both
  mode Off and brightness zero immediately clear LEDs; notify(0) cancels notices.
- Current contracts: [touch/gaze](../../m5stack-stopwatch/bot-ux-watch/specs/touch-gaze-iteration.md),
  [agent signals](specs/agent-signal-contract.md) and
  [Alive lighting](specs/core2-alive-lighting.md). Validate with root
  `tools/check_host.sh` and native capture/telemetry; never confuse serial transfer
  FPS with steady animation.

# bot-ux-watch — companion watch

A round watch face that hosts the shared `bot-ux` character. There is no elapsed-time
stopwatch mode in this app.

## Hardware

See [device hardware](../specs/hardware.md); keep app-specific integration below.

## Module layout

```
projects/m5stack-stopwatch/bot-ux-watch/
  platformio.ini
  include/CalendarMath.h   # deterministic date helpers
  include/TouchContact.h   # raw contact edges + clock double-tap
  include/InputSemantics.h # tap, swipe and two-second hold arbitration
  include/WatchFace.h      # owns botux::BotUx + RTC/power face
  include/Settings.h       # NVS-backed preferences
  include/WatchInteraction.h # chord, motion filter and ambient timing
  include/WatchControls.h    # shared render/input target geometry
  include/Power.h          # M5PM1 readings + brightness
  src/main.cpp             # face/settings/editors + input/frame loop
  src/WatchFace.cpp
  include/WatchLvgl.h      # native LVGL settings widgets and input
  include/CoralliumDevice.h # BLE/serial protocol endpoint
  src/WatchLvgl.cpp
  src/CoralliumDevice.cpp
  src/Settings.cpp
  src/Power.cpp
  test/test_calendar_math.cpp
  test/test_input_semantics.cpp
  test/test_touch_contact.cpp
  test/test_ui_controls.cpp
```

## Integration with bot-ux

The hero bot renders into a dedicated 286×286 sprite. A fixed 178×178 sprite provides
the live settings preview. Both bot sprites and the full-screen sprite are allocated
once in `setup()` and checked before use.

```cpp
M5Canvas canvas(&M5.Display);       // full 466×466
M5Canvas botSprite(&M5.Display);    // 286×286 bot
botSprite.createSprite(286, 286);
bot.begin(&botSprite);              // reads size from the sprite
// each frame:
M5.update();
bot.update(millis());
bot.draw();                          // into botSprite
botSprite.pushSprite(&M5.Display, 90, 90);
```

The watch draws its own RTC clock, date and battery status. Bot overlays are hidden;
there is no Wi-Fi/signal placeholder. UI text uses a dedicated light ink because the
official bot treatment uses dark pill eyes.

## Input semantics

- Face: A selects a random mood from the complete shared set; B advances through
  every mood, while B double returns to automatic Idle. A face touch inside the actual bot body looks Front; one outside it
  follows that direction before returning to the selected mood. A stationary
  bot hold no longer opens personalization. A+B held together for three seconds
  opens settings, and Long B enters dim doze.
- Hardware touch bypasses the SDK gesture classifier: `M5.Touch.end()` disables its
  polling, then the app samples `M5.Display.getTouch()` every 8 ms and derives one
  press/release edge per physical contact. A stationary face hold becomes Long at
  three seconds and only cancels tap recognition; a release after more than one
  second cannot become a tap.
- A downward swipe recognized from a contact beginning in the top 20 px reveals the
  compact battery panel. Tap the panel to dismiss it; otherwise it closes after six
  seconds. Double-tap the visible time within 420 ms to open settings; panel taps and
  non-tap gestures cancel the pending clock tap.
- Settings uses LVGL 8.4 native vertical lists for Time, Bot, Display, Sound,
  Power and Connection. Editors use native dropdowns for choices, thick sliders
  for small numeric ranges, switches for booleans and a keyboard for names;
  there are no touch +/- controls. Tapping a choice opens its option panel in the
  same editor; selecting a value closes the panel and updates the draft, with
  NVS persistence deferred to Done. Switch rows accept a tap anywhere in the row
  and emit one change, including taps on the switch itself. Slider gestures and
  vertical list scrolling are mutually exclusive for the entire contact.
  `WatchLvgl` owns draw/hit geometry and click-vs-scroll arbitration. `main.cpp`
  retains screen state, editor snapshots, RTC validation and save/cancel policy.
  Done stays outside scrolling as the bottom circular segment; there is no touch
  Cancel control, including the name keyboard's close key. A opens the selected
  menu row; B advances
  and reveals it. In editors A/B change the selected field's value, Long A cancels and
  Long B saves. Native touching clears the hardware navigation marker.
- Connection opens BLE explicitly for five minutes; advertise as StopWatch.
  Daily device UI must not use the app name. Only the separate protocol page
  names Corallium v1 and supported time.set/battery capabilities. Encrypted GATT RX and serial
  JSONL implement root protocols/corallium-v1. CoralliumDevice handles requests on
  the main task; NimBLE callbacks only assemble/queue. Do not add ordinary Wi-Fi as a substitute for Apple-compatible Wi-Fi Aware;
  this ESP32-S3/SDK firmware has no Wi-Fi backend or capability.
  Preserve watch NVS and flash partition layout. Offset lives in corallium NVS.
- Any touch or A/B press wakes from doze and is consumed, so it cannot trigger the
  control underneath. When Power saving applies, independent persisted dim and
  display-off timers use 5 s, 15 s, 1 min, 5 min, 10 min or 15 min choices and
  default to 15 s / 1 min. Touch and all three hardware buttons wake and consume
  their input. Charge awake skips ordinary dim/off on external VBUS; a valid
  forced-sleep interval overrides it while the global Power save switch remains on.
  The optional Keys-only wake mode uses ESP32 light sleep after display-off; A/B use
  direct EXT1 wake while a one-second timer polls power/home and external power. Touch
  is disabled as a wake source in this mode.
- M5PM1 single-click reset is disabled without changing double-click power-off or
  download behavior. `BtnPWR.wasClicked()` returns to the face, cancels an unsaved
  editor snapshot and consumes the active gesture. The green PM status LED is a
  persisted Display setting and defaults off.

## Behavior

- Face: RTC `yyyy/mm/dd {weekday}` and time share one information block. A hideable
  English/Chinese description occupies the other band; Layout swaps these regions. Battery
  has no permanent percentage; its factory-inspired top panel slides in over 300 ms
  and shows percentage and a gauge on a measured-level green/yellow/red fill;
  charging adds a separate bolt. Rendering and hit testing share its geometry.
- Settings is the vertical Time, Bot, Display, Sound, Power and Connection hierarchy described
  in `specs/settings.md`. Its styling follows Mosaico with a black background,
  flat rows with 1 px separators, white/gray text and orange controls, while retaining
  the round display's safe content inset and existing 24 px text fonts. Dropdown
  panels stay in the safe center of the circle; lists and the name keyboard end
  at y=381. Brightness retains the persisted 1..5 range, and dim brightness cannot
  exceed normal brightness. RTC errors appear below the title; choosing a date
  or time value does not write the RTC, and invalid clocks still require explicit
  confirmation of both date and time before a verified write. Time owns
  time/date/format; Display owns
  brightness, theme, indicator, button feedback and both layout controls; Sound owns
  master/startup/button/alert switches; Power owns the complete idle and forced-sleep policy.
  Personality includes expression, action, shape, eye style, HSV color, action amount
  and speed, naming, English/Chinese UI, all 1,120 independent combinations and a live
  gaze selector. The native keyboard enters up to 16 ASCII name characters. The HSV sliders
  preview live and Done persists the draft.
- NVS persists 12/24-hour format, seconds, theme, shape, eye style, custom HSV body
  color, expression, animation, wrist response, motion amount/speed, brightness,
  master/startup/button/alert sound, PM status LED, name, language, gaze direction,
  caption visibility, swapped layout, power-save enable, dim level, both idle timers,
  wake mode, charging keep-awake and forced-sleep enable/start/end. Gaze is Auto plus
  Center/Left/Right/Up/Down and four diagonals. RTC hardware persists time/date.
- BMI270 tilt passes through a low-pass filter and dead zone. Shake uses hysteresis and
  a seven-second cooldown before poke. Auto expression begins with Idle for 5–15 seconds,
  alternates with 30–60 seconds of Looking around, then shows a safe ambient mood for
  5–15 seconds. Returning from a held/manual state restarts that sequence at Idle.
- Display-off uses AMOLED panel sleep. The default Touch + keys mode keeps the MCU awake
  to poll power and wake inputs; Keys only also enters ESP32 light sleep. Clock
  continuity is preserved; bot animation, ambient scheduling, IMU, sound and rendering
  pause until wake. Neither mode uses deep sleep.
- Rendering targets 16 ms active / 33 ms preview / 250 ms dozing. The face pushes only
  the bot region; disjoint clock/status regions redraw when their values change. A fixed 466×90
  HUD canvas (83,880 bytes) provides coverage text/shapes without panel readback.
  Settings use a 466×24 LVGL draw buffer and native dirty-area invalidation. Serial
  `PERF` summaries report real frame timing for hardware validation.
- Serial `c` remains raw RGB565 capture. Diagnostic pages accept `vNN` plus newline:
  0 face, 1 settings, 2 personality, 3 expression, 4 appearance, 5 motion, 6 color,
  7 display, 8 last settings item, 9 scrolled personality, 10 battery panel, 11 format,
  12 name, 13 combinations, 14 layout, 15 language, 16 Chinese face, 17 swapped
  face, 18/19 settings list (legacy carousel IDs), 20 Happy/Joy/Wave
  preview, 21 gaze, 22 Thinking dots, 23 Time, 24 Sound and 25 lower Power rows.
  `contact 1|0 x y` replays raw contact through `TouchContact`, gesture arbitration
  and the current screen handler; `physical` returns to CST820B sampling.
  Optional `@N` is echoed in
  the `UI seq=N` acknowledgement. `ui` reads state; `sound N` previews a cue.
  Contact replay remains separate from hardware touch-controller acceptance.
  Language/layout diagnostics restore settings when leaving; no NVS write.
  Single digits remain accepted for compatibility; page selection never saves NVS.

## Rules

- Match existing code style; keep comments purposeful. No per-frame heap allocation.
- Keep the state machine in `main.cpp`; modules are plain classes.
- Run tools/check_lvgl.sh after widget/input edits; it compiles real LVGL with
  a native M5 display adapter. Legacy carousel/control tests are retained fixtures,
  not evidence for the current LVGL rendering or physical touch behavior.
- Persist settings with `Preferences` (NVS). Do not over-engineer edge cases.
- Keep the topical contracts in `specs/README.md` consistent with the implementation.
  Specs explain current facts, decisions, causes and verification limits. Raw
  contact replays and deployment logs belong in ignored `tmp/`, not spec appendices.

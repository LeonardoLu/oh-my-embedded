# stopwatch — companion watch (M5Stack StopWatch)

A round watch face that hosts the shared `bot-ux` character. There is no elapsed-time
stopwatch mode in this app.

## Hardware

- Device: M5Stack **StopWatch**, ESP32-S3R8 (16 MB flash + 8 MB PSRAM), 1.75" **round
  AMOLED** 466×466 (CO5300 driver, QSPI). PlatformIO `board = esp32s3box` +
  `board_build.arduino.memory_type = qio_opi` + 16 MB partitions.
- Inputs: capacitive **touch** (CST820B) + 2 programmable buttons (BtnA/BtnB). Touch is
  the primary input; buttons are shortcuts.
- Power: **M5PM1** through M5Unified. Battery is derived from filtered VBAT; charging
  requires VIN plus the active-low M5PM1 GPIO2 charge state.
- Audio: ES8311 codec + 1 W speaker via shared `ux::sound::Synth` and the M5 PCM sender.
- RTC: RX8130CE via `M5.Rtc`.

## Module layout

```
stopwatch/bot-ux-watch/
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
- Settings is a five-page horizontal launcher for Time, Bot, Display, Sound and
  Power. Each page shows one 200 px RGB565 icon on black with a name, position
  dots and fixed chevrons. It follows the finger, projects release velocity and
  eases to exact page centers; a horizontal drag cannot activate an item. B cycles all five pages and A
  opens the selected one. Time and Bot Personality use four-row vertical lists. Shared
  ScrollModel follows vertical finger displacement with bounded inertia; draw and
  hit tests use the same offset, and a drag cannot activate a row. One centered
  Done control sits outside list scrolling on every non-face screen;
  editors save through it and hardware Long A remains the cancel path. Buttons
  remain optional shortcuts. Touch feedback clears after release; only hardware
  button navigation keeps a selected row visible. Rows place labels on the left
  and current values on the right. Preview editors show up to two rows and scroll
  additional rows in their 120 px lower viewport.
  `ux::PointerSession` owns each settings sequence: 20 px vertical travel promotes
  list scrolling. Otherwise, release inside the captured original target within
  one second activates it, including after small drift or leaving and re-entering.
  Cancel inertia before target capture; use WatchControls for hit geometry.
  Color surfaces retain the pointer through drag and cannot activate Done.
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
  and shows percentage, a horizontal gauge, green charging fill and bolt.
- Settings is the horizontal Time, Bot, Display, Sound and Power hierarchy described
  in `specs/watch-settings-carousel.md`. Time owns time/date/format; Display owns
  brightness, theme, indicator, button feedback and both layout controls; Sound owns
  master/startup/button/alert switches; Power owns the complete idle and forced-sleep policy.
  Personality includes expression, action, shape, eye style, HSV color, action amount
  and speed, naming, English/Chinese UI, all 1,120 independent combinations and a live
  gaze selector. Chinese mode also localizes keyboard action labels. The HSV picker
  previews live and Done persists it.
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
  Launcher motion pushes its 466×280 content band; Time and Personality scrolling push
  their 466×264 list band. Static chrome stays cached. Serial
  `PERF` summaries report real frame timing for hardware validation.
- Serial `c` remains raw RGB565 capture. Diagnostic pages accept `vNN` plus newline:
  0 face, 1 settings, 2 personality, 3 expression, 4 appearance, 5 motion, 6 color,
  7 display, 8 last settings item, 9 scrolled personality, 10 battery panel, 11 format,
  12 name, 13 combinations, 14 layout, 15 language, 16 Chinese face, 17 swapped
  face, 18 fractional carousel, 19 animated carousel timing, 20 Happy/Joy/Wave
  preview, 21 gaze, 22 Thinking dots, 23 Time, 24 Sound and 25 lower Power rows.
  `contact 1|0 x y` replays raw contact through `TouchContact`, gesture arbitration
  and the current screen handler; `physical` returns to CST820B sampling.
  `td/tm/tu x y` remains a direct UI-pointer diagnostic. Optional `@N` is echoed in
  the `UI seq=N` acknowledgement. `ui` reads state; `sound N` previews a cue.
  Both replay paths remain separate from hardware touch-controller acceptance.
  Language/layout diagnostics restore settings when leaving; no NVS write.
  Single digits remain accepted for compatibility; page selection never saves NVS.

## Rules

- Match existing code style; keep comments purposeful. No per-frame heap allocation.
- Keep the state machine in `main.cpp`; modules are plain classes.
- Persist settings with `Preferences` (NVS). Do not over-engineer edge cases.
- The user's current goal, `specs/watch-settings-carousel.md`,
  `specs/watch-power-settings-iteration.md`, `specs/watch-gaze-experience.md` and
  `specs/watch-ambient-experience.md` supersede older stopwatch notes.
- Keep the final contact replay and on-device evidence in
  `specs/touch-gaze-validation.md`.

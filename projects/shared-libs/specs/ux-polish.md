# Companion UX refinement

This goal follows the user's September 6 feedback after the connected-HID
iteration. It supersedes earlier visible-SET and fixed-expression UI decisions.
BotUx exploration, design and implementation are assigned to GPT-6 at medium
reasoning, as the user's latest instruction specifies. StopWatch and Core2
implementation/cross review use Sol at high reasoning.

## StopWatch

- Remove the visible settings entry from the face. Hold A and B together for
  three continuous seconds to open settings. Consume both releases so the chord
  cannot also poke the character or switch expressions. Earlier single-button
  and swipe-to-settings shortcuts must not undermine this interaction.
- All settings are touch-operable, with readable native fonts, hierarchical
  pages and vertical scrolling. A scroll gesture must not also activate a row.
- Long-press the character to enter its personalization page directly.
- Place year, month, day, weekday and time in one coherent date/time region.
- Replace the battery/charging treatment with a transient top panel inspired by
  the factory implementation. Normal downward face gestures must not conflict
  with a gesture whose touch origin is the top edge.
- Motion should be continuous and subtle, using filtering, a dead zone and
  hysteresis. Avoid repeated surprise interrupts. Discrete reactions need a
  meaningful threshold and cooldown; manual interactions take priority.
- Idle behavior slowly shuffles varied non-strongly-negative moods and
  expressions. Avoid immediate repeats, abrupt changes, and interference with
  a manual choice or an active gesture. Provide a setting for the behavior.

## BotUx

- Keep Neutral's recognizable orb and paired-eye proportions. Redesign the
  other expressions so identity and natural balance survive their differences.
- Research public Grok Bot recreations/derivatives and related character motion,
  recording concrete design influences. Use original geometry and implementation.
- Improve continuity and restraint without losing recognizable expressions.
- Improve raster edges on the 310 px StopWatch sprite. Geometry-only SVG
  previews cannot prove anti-aliasing; inspect actual firmware raster pixels.
- Provide touch color pickers and a dedicated personalization flow in the device
  apps, with live preview and persistent color/style preferences. A preset list
  alone does not satisfy the color-picker requirement.
- Preserve the component's caller-owned canvas and allocation-free frame loop.
  Measure the quality/performance tradeoff on device before accepting it.

## Core2

- Put connection/status feedback in top-bar icons. Remove CODEX MICRO and CODEX
  text labels and the standalone SET control.
- Integrate settings access with battery: two quick valid taps open settings;
  one tap, a long pause, or a canceled drag must not open it.
- Use a compact current-page/total-pages indicator and simple navigation.
- Preserve the verified Micro HID identity, request/response framing, controls,
  neutral joystick handling, host lighting and automatic reconnection.
- Keep useful touch target sizes despite smaller visual chrome.

## Factory battery reference

The [official usage guide](https://docs.m5stack.com/en/guide/display_device/stopwatch/usage)
describes revealing battery level by swiping from the top edge toward the center.
The source review uses M5Stack's MIT-licensed factory firmware at commit
`6b4aa125288b6fe9dca661f10159f6e1e5ee785c`, particularly
`main/apps/common/status_bar/status_bar.cpp`.

The reference implementation tracks a touch starting in the top 20 px and
requires a predominantly vertical downward movement over 50 px. A rounded
158×65 panel slides between hidden y=-85 and shown y=-17 with a roughly 300 ms
spring. It contains a percentage and a horizontal battery icon; charging uses
green fill and a lightning mark. Tapping hides it, and ordinary displays time
out after six seconds. These are design facts to adapt, not mandatory pixel
coordinates for the new implementation. The CDN animation was blocked in the
browser; its appearance was not inferred from an unseen image.

## Acceptance evidence

- Focused host checks for chord timing/release consumption, scroll-vs-tap,
  long-press, double-tap, idle scheduling and relevant color conversions.
- Independent cross review of each device flow and renderer changes.
- Successful builds of both firmware applications.
- Actual native raster captures of the watch face, battery panel, scrolled and
  nested settings, personalization/color picker, plus Core2 header/page/settings.
- Before/after rendering measurements, including anti-aliasing cost and memory.
- Final flashing, boot health and Core2 reconnection with real Codex RPCs.
- Updated usage/validation docs and meaningful commits; scratch stays ignored.

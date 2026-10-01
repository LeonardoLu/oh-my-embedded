# Start-up revision

> Historical baseline record. The September 6 connected-controller iteration
> supersedes the local-simulation scope below. See [current requirements](../../m5stack-core2/bot-ux-codex-core2/specs/interaction-iteration.md)
> and [current validation](../../m5stack-core2/bot-ux-codex-core2/specs/interaction-validation.md).

This document records the implementation decisions for [start-up.md](start-up.md). The task
specification remains the source of truth.

## Product boundaries

- The StopWatch hardware hosts a watch: RTC time, date, battery/charging status,
  a companion character, and settings. There is no elapsed-time stopwatch mode.
- Core2 hosts a local Codex Micro simulation. Agent selection, workflow shortcuts,
  reasoning adjustment, and accept/reject/voice controls are its core concepts.
  Simulated task progress does not call Codex, run commands, or transmit audio.
- The shared character stays in `projects/shared-libs/bot-ux`, keeping the existing PlatformIO
  library boundary. Each host owns its display, sprites, input, and frame pacing.
- Use hardware power readings; do not draw an invented connected signal.

## Reference basis

[Grok Bot's design article](https://x.ai/news/designing-grok-bot) and its
interactive lifecycle demonstration establish the visual direction: a minimal
light orb with offset paired eye marks, motion expressing state, and distinct
thinking/blocked symbols. The firmware interprets this with RGB565 drawing
primitives; its extra moods, timing, and personalization are project choices.

[OpenAI's Codex Micro product page](https://openai.com/supply/co-lab/work-louder/)
describes agent keys with live status, joystick workflow shortcuts, command keys
including accept/reject/push-to-talk/new chat, and a reasoning dial. Core2 maps
these concepts onto its touch screen and bottom touch buttons. The adapted
layout and simulated timing are project design decisions, not a claim of exact
hardware or protocol compatibility.

Hardware references:

- [M5Stack StopWatch](https://docs.m5stack.com/en/core/StopWatch)
- [Core2 v1.3](https://docs.m5stack.com/en/core/Core2_v1.3)
- [M5GO Battery Bottom2 v1.3](https://docs.m5stack.com/en/base/Base_M5GO_Bottom2_v1.3)
- [M5Stack Core2 RGB LED example](https://docs.m5stack.com/en/arduino/m5core2/rgb_led)

Bottom2 exposes ten SK6812 RGB LEDs on GPIO25. Their state feedback is driven
through M5Unified's LED strip/RMT support. It does not provide independent battery
telemetry to be displayed as a second measured battery.
It replaces Core2's stock bottom, whose vibration motor is absent from the
Bottom2 feature list; the app uses screen, light, and optional audio feedback.

## Validation boundary

Build both PlatformIO projects and perform independent source/state review.
Host-rendered previews represent the firmware drawing commands; they are not
device photographs and cannot establish display timing, physical touch feel,
audio, charging accuracy, or LED behavior. Record those limitations explicitly.
Temporary research, build logs, and generated previews belong in ignored `tmp/`.

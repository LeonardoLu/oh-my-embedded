# Start-up validation — 2026-09-06

> Historical baseline record. The September 6 connected-controller iteration
> supersedes the local-simulation scope below. See [current requirements](../../m5stack-core2/bot-ux-codex-core2/specs/interaction-iteration.md)
> and [current validation](../../m5stack-core2/bot-ux-codex-core2/specs/interaction-validation.md).

Completed the implementation and source-based acceptance of [start-up.md](start-up.md).
The seven requested specialist roles (researcher, UX professor, ESP32 professor,
two device developers, and two independent device QA roles) used GPT-5.6 Sol
with high reasoning. The coordinating agent integrated and reviewed their work.

## Final builds

| Project / environment | Result | Static RAM | Application flash |
| --- | --- | ---: | ---: |
| StopWatch / `m5stack-stopwatch` | PASS | 23,912 B | 572,957 B |
| Core2 / `m5stack-core2` | PASS | 26,916 B | 527,617 B |

Both builds use `espressif32@6.13.0` and pinned library revisions. The RAM figures
are linker reports, not total runtime usage: sprites are allocated at startup.
StopWatch uses a 466×466 canvas plus 206×206 and 124×124 bot sprites; Core2 uses
a 320×240 canvas plus a 40×40 bot sprite. Both check allocation failures.
Build logs are in ignored `tmp/final-watch-build.log` and
`tmp/final-core2-build.log`.

## Automated and independent checks

- BotUx host renderer compiles the real component with warnings treated as
  errors, emits 40/72/200 px mood sheets, and checks bodyless task glyphs,
  long-uptime reaction behavior, graphite defaults, and distinct Sleepy/Waiting
  eye poses.
- Watch calendar tests cover leap years, month lengths, and weekdays. Input
  tests cover tap/long/swipe exclusivity, long-then-drag, and consumed wake input.
  Timed-state tests cover duration boundaries and `millis()` wrap.
- Watch QA's scratch harness invokes the real app state functions with fake RTC
  and power interfaces: time/date save and cancellation, month-day clamping,
  preservation of the unedited RTC fields, preference rollback, and charging
  acknowledgment versus low-battery/doze priority.
- Core2 model tests cover inert selection, workflow transitions, approvals,
  rejection, reasoning duration, per-job timing snapshots, and next-agent
  navigation.
- Core2 QA's scratch harness invokes the real touch handlers: original-target
  release, voice submission, drag-off cancellation, screen-to-bottom gesture
  isolation, and disabled approval hit targets.
- Source-derived app previews cover the watch face and every editor, both Core2
  pages, settings, and Dark pressed states. Independent UX review accepted the
  round layout, readable state labels, disabled controls, contrast-safe press
  feedback, and noninteractive information blocks.
- `git diff --check` passes. Scratch data and generated files remain ignored;
  no `tmp/` files are tracked.

Repeatable tracked host commands are in the root README. The full-app fake
device harnesses and research notes are temporary review aids under `tmp/`;
they are not a hardware test framework.

## Scope and remaining physical checks

The shared renderer, watch features, Codex Micro local simulation, settings, and
Bottom2 LED integration are implemented. The old stopwatch and QWERTY/chat
modules are removed. Agent work and voice are explicitly simulated; no host
commands, networking, microphone capture, or audio transmission are performed.

Two USB serial ports were detected. Neither device was flashed. Following the
task's preference for development/code review over acceptance requiring a
person, physical display rendering, touch alignment, battery/charging accuracy,
audio, Bottom2 LEDs, and on-device frame timing remain unverified. SVG previews
approximate drawing primitives and fonts; they are not device screenshots or
measurements of hardware performance.

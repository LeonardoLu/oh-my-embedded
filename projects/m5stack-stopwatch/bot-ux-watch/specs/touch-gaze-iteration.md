# Touch-up-inside, nine gaze directions and device controls

2026-09-06. This goal supersedes conflicting earlier gesture/UX notes.

- Watch settings use touch-up-inside: a press starts on a control and releases
  inside that same control within 1,000 ms. Leaving and re-entering is allowed.
  A captured vertical list drag (20 px threshold) owns its entire sequence and
  never clicks. Holds are distinct at 2,000 ms; 1–2 s releases do neither.
- Raw display contact samples drive host edges and preserve the last coordinate
  on release. Do not use M5Unified's flick/hold classifier as the contact source:
  its pinned implementation can swallow recontact immediately after flick_end or
  drag_end. Hardware and diagnostic contact injection share the host edge path.
- Settings/Personality keep Back/Done in fixed chrome independent of scroll.
  Rows and their current values are centered. Editor Back cancels, Done saves.
  Double-tap the visible time/date band within 420 ms to open Settings.
- Power single-click returns home, cancelling an open editor. Disable only the
  PM1 single-reset bit; preserve double-off/download behavior. The green status
  indicator is controlled separately through LED_EN, default off, with a setting.
- Watch language applies to all available translated UI text, enumeration names,
  descriptions, date periods and keyboard action labels. User names and English
  letter entry retain their content. The shared CJK corpus covers actual sources.
- Bot exposes Front, L/R/U/D and four diagonals (nine explicit directions); Auto
  remains an additional compatible mode. Direction changes also affect eye
  proportions, perspective and mirrored shapes, with continuous interpolation.
  Captured face touches follow the finger even outside the initial hero region,
  then hold for 2,200 ms and ease back to the selected direction.
- Thinking dots have visibly larger continuous travel, respecting reduced motion
  and zero amount and retaining subpixel coverage.
- Core2 adds saved feedback volume independent of mute, brighter/high-contrast
  themes and contextual Alive breathing/flow. Preserve host hues and authoritative
  host/held states; no fabricated toggle or completion status.
- Shared sound retains sixteen cues, six timbres, scales/glides and optional
  8-bit PCM. Volume zero is silent, volume changes smooth; sender waits must stay
  outside UI. Preserve fixed borrowed-buffer lifetime and bounded cancellation.

GPT-6 medium agents own Bot and shared sound/CJK. Sol high owns Core2 and reviews
Watch power control. Parent owns Watch contacts, gestures, layout, translation,
power integration, builds, native captures, flashing and acceptance. All changes
share one checkout with file ownership. Never inject HID user actions for checks.

Authoritative hardware evidence: [StopWatch documentation](https://docs.m5stack.com/en/core/StopWatch),
[schematic page 2](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1242/C152-SCH_Stopwatch_PRJ_Main_VA_20251201_2026_04_24_17_46_22.pdf)
and installed pinned M5PM1 register definitions. PM_STATUS_LED is active high;
0x06 bit4 controls its default level. Register 0x49 bit0 disables single reset;
0x4A controls double-off and is not modified. Runtime readback verifies only
register state; it is not a physical optical or button-press measurement.

Acceptance: host boundaries/recontact/scroll tests, actual SDK source regression,
all UI translation coverage, native bilingual pages and centered/fixed controls,
nine-direction raster comparisons and temporal/AA checks, volume/sound/LED tests,
both firmware builds and explicit-port uploads, PM1 readback and live Core2 RPC.
Native injected contacts bypass the touch IC and captures are firmware canvases;
manual physical touch, acoustic and optical acceptance remain separate evidence.

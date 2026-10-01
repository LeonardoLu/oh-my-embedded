# Shared UX and companion iteration — 2026-09-06

This is the current user goal and supersedes earlier conflicting UX decisions.
The existing verified Codex Micro Bluetooth transport must remain functional.

## Required outcomes

### StopWatch

- Improve non-Bot UI edge antialiasing, font rendering, touch accuracy and swipe
  sensitivity. Lists follow the finger with continuous pixel scrolling rather
  than stepping selection a row at a time.
- Date is `yyyy/mm/dd {weekday}`.
- Show a hideable Bot description at the top. Add a setting that swaps its
  location with the time/date block.
- A single click randomly selects a preset expression/mood/animation combination.
  B single click advances through presets; B double click returns to idle.
- While idle, a screen tap temporarily moves the Bot's gaze toward that location.
- Preserve the A+B 3-second settings entry and long-Bot personalization entry.

### BotUx

- Repair disconnected Joy eye strokes and bumpy/unrounded eye morphs.
- English user naming, default **Milo**; no Chinese input method.
- Current-state/expression descriptions in English and Chinese, using the name
  where appropriate. Provide device-accessible language and naming settings.
- Personalization exposes a complete Cartesian combination preview of every
  expression, mood and animation, with independent selectors and a live preview.
- Idle gaze varies naturally instead of staying fixed toward the upper right.
- Preserve caller-owned canvases, existing public enum values, and no frame-time
  heap allocation. Expose additive gaze, naming, description and preset helpers
  for device apps rather than duplicating semantic state tables.

### Core2

- Right-align top-bar device status icons. Draw colored active glyphs without
  colored background tiles. Overlay battery number within the battery icon.
- Top-bar Bot follows the selected agent's actual host-provided color/status.
- Agent cards show actual agent status, use host agent colors as backgrounds and
  put their index in a corner badge. Meaningful state transitions trigger bounded
  color animation and optional sound. Unknown state must stay explicitly unknown.
- Design optional, dynamic Bottom2 LED interaction with a sense of life; include
  an on/off setting and preserve meaningful host feedback.
- Integrate shared components plus naming, bilingual descriptions and complete
  combination preview in Bot settings. Preserve host events, handshake and
  reconnect behavior. Do not fabricate agent/task state.

### Shared UX components

- Create a reusable M5Stack UX directory with independently usable components or
  products for rendering, typography, touch/scroll, spacing and motion.
- Both StopWatch and Core2 integrate the relevant products. Avoid dragging unused
  assets/components into device firmware; document selective packaging and
  eventual CoreS3 support without implementing a nonexistent app.
- Use real coverage blending for UI/text improvements; demonstrate native raster
  output and measure on-device timing/memory. Fonts must have redistributable
  licenses and retain the relevant notices.

## Ownership and validation

GPT-6 medium owns BotUx and shared UX exploration/design/implementation. A Sol high StopWatch worker was requested but could not start because of the
active thread limit. The parent therefore owns both app integrations, builds,
flashing and acceptance; GPT-6 medium agents cross-review the Bot/UX integration. Cross-review owners' work after implementation. Do not edit another
owner's files without coordination. Publish shared APIs early. Never leave tmp
tracked; keep durable contracts and findings here or in component docs.

Verify focused interaction/geometry/text/preset checks, both PlatformIO builds,
native UI captures (including Chinese, naming and combination preview), real
frame timing and Core2 reconnect. Serial capture is diagnostic and can pause the
frame loop; exclude its transfer time from steady FPS. Preserve existing user
settings except intentional schema migration. Commit meaningful units.

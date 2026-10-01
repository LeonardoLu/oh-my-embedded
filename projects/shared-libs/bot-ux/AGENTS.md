# bot-ux — component notes for agents

The shared animation component. Public API is `src/BotUx.h` (source of truth);
implementation is `src/BotUx.cpp`.

## Rules

- The component renders into an `M5Canvas*`; it **never** owns a display, calls
  `M5.update()`, or pushes pixels. Hosts do that.
- All timing is `millis()`-based and framerate-agnostic. `update()` computes the
  per-frame state; `draw()` only renders. Never allocate in `draw()`.
- Keep drawing to M5GFX primitives (`fillCircle`, `fillEllipse`, `fillArc`,
  `fillRoundRect`, `fillTriangle`, `drawLine`, `drawString`, …). Coverage helpers
  may use `fillRect` spans, `drawPixel`, and RGB565 `readPixel` at edges; do not
  add a full-resolution secondary framebuffer or allocate while drawing.
- Arc angle convention (LovyanGFX, verified): **0° = right (3 o'clock), clockwise,
  90° = down, 180° = left, 270° = up**. Happy crescent eyes use the top
  arc (`180..360`).
- `Style` colors are RGB565 (`botux::rgb565`). Passing `uint16_t` to M5GFX's
  `uint32_t` color params is fine.

## When to touch this

- New mood/expression → add a case in `_resolveMood`, then handle silhouette
  replacement in `_drawBody` or eye treatment in `_drawEyes` if needed.
- New theme → add a preset in the device apps, not here (this stays generic).
- API change → update the header, keep it additive so device apps don't break.

## Visual invariants

- Neutral defines the face's proportions. Keep the other expressions close to
  its eye spacing and calm proportions; avoid extreme angles and size asymmetry.
- Joy/Wink/Alarmed morph continuously through eased scalar geometry. Do not
  restore abrupt per-expression glyph replacement for ordinary facial expressions.
- Default ellipse and capsule boundaries use explicit RGB565 coverage. Preserve
  subpixel centres and validate with the host preview; native raster/performance
  validation belongs to the host/device integration checks.
- Update `tools/host-preview/include/M5GFX.h` when using a new canvas operation.
  The preview's raster-backed rectangle/pixel calls check actual coverage colors;
  other primitives remain SVG approximations.

- Explicit Neutral must restore the baseline face even under Sleepy/Waiting;
  Auto keeps the mood's face. Keep tiny toolbar expressions on the same eased
  geometry path, rather than masking them with fixed generic eye glyphs.

- Joy eyes rasterize the union of a swept quadratic once per pixel, with fixed
  stack geometry; avoid overlapping blended segment fringes and visible bumps.
- Names are bounded ASCII with Milo fallback. Bilingual descriptions use UTF-8-safe
  truncation. Keep the consuming Cjk18 corpus current when changing Chinese copy.
- Presets, temporary gaze and combination enumeration are shared semantics. Device
  preview instances must remain separate from persistent/host-driven state.

## BotUx contract (source of truth: `projects/shared-libs/bot-ux/src/BotUx.h`)

- `begin(M5Canvas*)` — bind a sprite, never owns the display.
- `setStyle(Style)` — personalization (colors, eye/body style, sizes).
- `setMood(Mood)` — persistent mood; 14 current values including append-only Asleep and LookingAround.
  Use header counts/names: 14 moods × 10 expressions × 8 animations = 1,120 combinations.
- `setTalking(bool)`, `setBattery(pct)`, `setSignal(bars)`, `setTime(h,m,s,pm)`, `setLabel(text)`.
- `setGazeDirection(GazeDirection)` — Auto plus nine explicit directions (Center, L/R/U/D, four diagonals);
  temporary `gazeAt` overrides expire.
- `setFaceSide(FaceSide)` — Auto plus fixed Left/Right mirroring; Idle defaults to UpRight and LookingAround owns autonomous gaze.
- `poke()` — transient surprise→happy reaction.
- Naming, bilingual `describe`, curated presets and temporary `gazeAt` are additive APIs.
  Counts/name helpers drive independent device combination previews.
- `update(nowMs)` + `draw()` — host drives the frame loop.


Current motion contracts: [orb motion](specs/orb-motion-experience.md),
[Happy/Done](specs/bot-happy-done-experience.md), and
[Watch gaze](../../m5stack-stopwatch/bot-ux-watch/specs/watch-gaze-experience.md).
They supersede older vocabulary/touch records where those disagree. Keep
root `wiki/bot-ux/intro.md` and `intro.html` synchronized through the native renderer.

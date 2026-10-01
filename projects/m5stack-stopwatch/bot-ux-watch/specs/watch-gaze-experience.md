# Stopwatch gaze, looking-around, and face-side experience

2026-09-09 implementation and host-render acceptance for the shared `BotUx`
component and the Stopwatch ambient lifecycle.

## Interaction contract

- `Mood::Idle` with automatic or explicit Neutral expression rests at the
  established upper-right 45-degree face. Its settled eye group is `+0.26R,
  -0.38R`, the eye major-axis ratio is `+0.34`, and the screen-right eye keeps
  the established slight size emphasis.
- `GazeDirection::UpRight` and a unit-normalized upper-right
  `gazeAt(0.707107, -0.707107)` settle to that exact same native raster. A
  body-inside touch supplies `gazeAt(0, 0)` and remains a centred Front pose.
- Temporary `gazeAt` has precedence over a persisted `GazeDirection`, then
  eases back to that direction after the hold. `gazeAt` is accepted while the
  persistent mood is Idle or LookingAround. Inputs outside the unit circle are
  normalized before the shared field is evaluated.
- The horizontal field is left/right symmetric. The upper limit preserves the
  Idle baseline at `-0.38R`; the established comfortable downward limit stays
  at `+0.16R`. Pure side and Front poses remain upright, while pitch along a
  side arc continuously changes the mirrored eye-axis slope.
- An explicitly selected non-Neutral expression in Idle retains its authored
  gaze. Stopwatch touch tracking selects Auto, so touch still uses the complete
  shared direction field.

## LookingAround and Stopwatch timing

`Mood::LookingAround` is append-only value 13. Public counts are now 14 moods ×
10 expressions × 8 animations = 1,120 combinations. Its names are `Looking
around` and `到处看看`; the Chinese description phrase is `正在到处看看`.

LookingAround chooses a normalized random target across the full direction disk,
holds it for 900–2,800 ms, and eases to the next target. Normal motion can reach
both horizontal halves and the same upper/down limits as touch. Reduced motion
and zero motion scale autonomous travel back toward the canonical Idle pose;
explicit directions and touches remain available.

The Stopwatch owns the longer ambient sequence rather than embedding it in the
shared renderer: Idle holds upper-right for a random 5–15 seconds, LookingAround
follows, other eligible ambient moods appear after a random 30–60 seconds, and
the app returns to LookingAround after another random 5–15 seconds. Manual mood
selection disables app-level ambient rotation; manually selecting LookingAround
still runs that mood's internal eye movement.

## Facial handedness

`FaceSide::{Auto, Left, Right}` controls expression handedness independently of
eye-group location. Auto follows the actual blended eye-group position including
direction easing and IMU countershift. A small centre dead zone blends to a
symmetrical face, so side crossings do not swap the wink, lean, asymmetric
openness, twist, or closed-eye extension in one frame.

Explicit Left/Right chooses the authored facial half without moving the eyes or
reflecting the framebuffer. A caller that wants both position and handedness can
combine them, for example:

```cpp
bot.setFaceSide(BotUx::FaceSide::Left);
bot.setGazeDirection(BotUx::GazeDirection::UpLeft);
```

Capsule and smile-curve vectors carry their horizontal sign through closure and
small-canvas clamping. This matters for Sleepy and other nearly closed faces;
adding a positive screen-x closure width after rotation would not be a true
mirror.

## Native host acceptance

`projects/shared-libs/bot-ux/tools/host-preview/render.sh tmp/botux-gaze-preview` compiles the
real component as C++11 with `-Wall -Wextra -Werror` and passes all prior checks
plus:

- exact pixel equality among settled Idle, UpRight, and upper-right unit touch;
- temporary-gaze precedence and return to a persisted Left direction;
- reflected pairs for all nine explicit expressions × four eye styles, plus 11
  automatic facial moods × four eye styles (Thinking, Working, and Blocked use
  non-face body glyphs);
- eye containment at 40, 72, 178, and 286 pixels for every explicit expression,
  eye style, and explicit gaze direction;
- full LookingAround sampled range at 200 pixels: eye-group x `77.5..122.75`, y
  `66.75..113.75`;
- continuous unit-circle side arcs: maximum 16 ms measured eye-axis step
  `0.01238` left and `0.03136` right, below the existing `0.055` limit;
- all 1,120 mood/expression/animation combinations and the existing motion,
  coverage, sleep, and sparse-frame regressions.

Native RGB565 evidence:

- `projects/shared-libs/bot-ux/docs/idle-up-right-raster.png`: Idle, UpRight, mirrored UpLeft.
- `projects/shared-libs/bot-ux/docs/nine-directions-raster.png`: all explicit directions.
- `projects/shared-libs/bot-ux/docs/face-sides-raster.png`: rows are Skeptical/Wink,
  Bashful/Curious, Sad/Sleepy, Waiting/Done; each item is right then left.
- `projects/shared-libs/bot-ux/docs/looking-around-raster.png`: nine sequential autonomous
  samples.

These are host RGB565 captures of the component, not device recordings. No
serial port or hardware flash was used for this acceptance.

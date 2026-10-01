# Bot vocabulary and native catalog — 2026-09-06

## Contract

Idle establishes the face placement for all moods and expressions: 0.26 body
radius right and 0.38 above center. Selected gaze remains body-relative and
continuously changes spacing, size, slant and mirrored perspective. Downward
displacement is 0.16 radius, closer to center than the 0.24 upward displacement.
Expression openness, lean and twist remain recognizable on the shared geometry.

Thinking's three staggered dots have 0.42-radius vertical amplitude, increased
from 0.28, with float centers and existing RGB565 AA coverage. Reduced motion
and zero amount apply to the entire dot choreography.

Waiting now means patient attention: partly open eyes, a questioning lean and
5.2-second sideways search. It uses the normal eye morphology at small sizes,
without the earlier tiny-only fixed dash substitution. Auto animation is Calm.

Asleep is appended as Mood value 12; all earlier enum values remain unchanged.
Counts are 13 moods × 10 expressions × 8 animations = 1,040 combinations.
Asleep uses closed eyes, an 8.8-second breath and three vector z marks above and
to the right of the orb. Each z fades completely at its 4.8-second cycle wrap.
Marks blend against the actual underlying RGB565 pixel, preventing cutouts
even when a RoundedSquare corner moves underneath. Opaque eye capsules retain
their existing fast span path; only fading marks take the alpha interior path.
Reduced motion calms travel and zero amount freezes the marks. Explicit Neutral
reopens the face while retaining the sleeping indicator. No new per-frame heap
allocation or secondary framebuffer is introduced. Firmware Chinese additions
are `熟睡` and `正在熟睡`; Watch integration owns CJK corpus regeneration.

## Native evidence

`projects/shared-libs/bot-ux/tools/host-preview/render.sh` passes with C++11, Wall/Wextra/Werror:

- All 1,040 combinations sustain distinct movement in multiple late-uptime
  windows; nine directions and every eye style retain the intended direction.
- Thinking center-dot peak travel at 200px: 68.5px full, 13.5px reduced, 0px zero.
  Consecutive 16ms samples stay within 5px of the prior position.
- Asleep is distinct from Sleepy. All four body styles are checked across two
  complete z cycles. Round maximum adjacent RGB565 difference at wrap /
  elsewhere is 1,041 / 4,172 full and 207 / 828 reduced. All four styles have
  smaller wrap than ordinary maxima and zero amount gives 0 / 0. Aggregate
  differences across all styles are about 1.09 million / 202,351 / 0 for
  full/reduced/zero. RoundedSquare’s integer host primitive produces larger
  ordinary body-motion differences; Hexagon’s polygon remains SVG-only in
  this harness. The shared capsule alpha path itself is raster-backed.
- Explicit Neutral reopens Sleepy, Waiting and Asleep at 40, 72 and 200px.
  Eye-ink measurement excludes the body's AA fringe, which can quantize to the
  same RGB565 color as the eyes. The partly-open Waiting face differs by at
  least one measured pixel at the smaller sizes.
- Down at 200px lies 10–14px below center, while up remains more than 12px above.
  Diagonals, mirrored perspective and eased direction transitions still pass.

These checks are host raster and state evidence, not physical hardware or
firmware frame-time acceptance. Existing native circles were SVG-only; the
harness now also fills their RGB565 raster so captured sparkle effects appear.
That fill is a simple host approximation of native M5GFX circles.

## Illustrated catalog and browser acceptance

`wiki/bot-ux/intro.md` and `intro.html` list every implemented mood, expression
and animation, including both Auto entries. All 31 entries have bilingual
explanations and real C++ RGB565 captures: 72 frames, 120×120, 100ms apart.
The 31 GIF assets are retained beside the Markdown guide. HTML embeds every
asset and works offline, with Chinese/English search, category filter, motion
pause and an initial static mode when reduced motion is requested.

Reproduce after the native build:

```sh
./tmp/botux-preview/botux-preview tmp/botux-preview --catalog
python3 projects/shared-libs/bot-ux/tools/host-preview/catalog.py tmp/botux-preview
```

The packager requires Pillow. GIF quantization and the finite 7.2-second capture
loop can introduce artifacts absent from continuous firmware. Display remains
at the native 120px size to avoid artificial enlargement.

CUA browser checks on the localhost catalog verified desktop card appearance,
Chinese search `熟睡` → 1/31, Animation category → 8/31, pause/play switching
between static PNG and GIF, and a 390×844 viewport with 390px document width,
358px card width and no horizontal overflow. Screenshots were inspected at both
sizes. Root separately accepted the catalog and restored the complete 31-entry
view. Local preview server remains on 127.0.0.1:8765 for review; the artifact
itself does not require a server or network.

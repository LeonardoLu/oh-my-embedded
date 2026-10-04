# ux-components

[共享库索引](../README.md) · [规格与验收索引](specs/README.md)

Small independent native UI products for M5Stack canvases. Components own no
canvas, framebuffer, task, timer or display. Device hosts retain frame and touch
ownership. C++11; no allocation in any draw or input method.

## Select only the products needed

Device PlatformIO projects already use `lib_extra_dirs = ../../shared-libs`. Include
`UxRender.h`, `UxText.h`, `UxInput.h`, `UxKeyboard.h` or `UxTokens.h` separately.
There is no umbrella header, startup registration or global font cache.
Geometry, input, keyboard and motion are header implementations. Each font lives
in a separate object in the library archive: only referenced faces link.
Do not use `--whole-archive` for this library. It has no M5Unified dependency;
template render functions accept an M5Canvas-compatible 16-bit canvas providing
width/height, readPixel, drawPixel and fillRect. CoreS3 can consume these products
with its own input adapter when an app exists; no speculative CoreS3 app is added.

## Rendering and typography

```cpp
#include <UxRender.h>
#include <UxText.h>
ux::roundRect(canvas, 8, 8, 150, 40, 10, 0x2126);
ux::strokeRoundRect(canvas, 8, 8, 150, 40, 10, 0x7bef); // inward 1px
ux::drawText(canvas, "Milo / 开心", 18, 16, 0xffff, ux::Cjk18);
int width = ux::textWidth("Milo", ux::Latin18);
```

Coordinates for text are the top-left of a common line box. Text uses precomputed
4-bit FreeType coverage (16 opacity levels), blended with the actual underlying
RGB565 pixel, including over colored surfaces. No browser text, upscaled bitmap
or binary-mask font is used. Unknown glyphs become `?`. Chinese coverage is
explicitly bounded by `fonts/corpus.txt`; add strings there and regenerate before
introducing new Chinese copy. Mixed English/Chinese text uses the Latin glyphs
included in each CJK face.
Newlines are supported; wrapping and screen layout remain host responsibilities.
Use `textWidth` to align/center and `lineHeight` to budget vertical space.

| Face | Native size / line height | Coverage bytes | Glyphs |
| --- | --- | --- | --- |
| Latin14 | 14 / 19 px | 3,925 | 95 |
| Latin18 | 18 / 23 px | 5,981 | 95 |
| Latin24 | 24 / 29 px | 10,919 | 95 |
| Latin28 | 28 / 33 px | 14,829 | 95 |
| Clock36 | 36 / 41 px | 4,145 | 17 |
| Cjk18 | 18 / 23 px | 46,085 | 358 |
| Cjk22 | 22 / 27 px | 69,351 | 358 |
| Cjk24 | 24 / 29 px | 82,403 | 358 |
| Cjk28 | 28 / 33 px | 110,547 | 358 |

Glyph records also consume approximately 16 bytes each depending on target ABI.
Clock36 includes digits, colon, slash, space, AM/PM and fallback question mark.
All fonts are constant flash data; there is no runtime font decompression buffer.

Shapes use signed-distance boundary coverage. Opaque rounded-rectangle interiors
are horizontal fillRect spans; only fringes/corners use individual pixel blending.
`strokeRoundRect` skips its hollow interior and preserves existing content.
`circle` and rounded-end `line` use the same coverage/color contract. Clip drawing
through the canvas clip rectangle as usual; components never change that clip.
Pass RGB565 colors, and do not use this renderer on a 1-bit destination.

## Input, naming, spacing and motion

`ScrollModel::setBounds(contentHeight,viewportHeight)` clamps its positive pixel
offset. On down call `begin(y,nowMs)`, during touch `move(y,nowMs)`, then `end(nowMs)`.
The latter returns true only for a tap. Drag starts at six pixels, tracks every
pixel thereafter, and remains a drag even when a finger returns to its origin.
Repeated coordinates (including the release sample) retain the last nonzero
velocity for up to 90 ms. Holding still longer cancels momentum.
Call `update(elapsedMs)` every frame for bounded inertial motion; its result says
whether the offset changed. Render rows at `contentY-offset()` and hit-test using
that same offset. Call `cancel()` when changing pages. Do not also activate a row
from a separate release callback after this model reports a drag.

`NameEditor::begin(name)` initializes a bounded 16-character ASCII name. `press`
accepts key 0–25 (letters), Backspace=26, Space=27, Case=28, Done=29. `text()` is a
fixed buffer; Done trims trailing spaces and defaults an empty name to Milo.
Persist and update BotUx only after Done; the host owns cancellation. This is an
English keyboard, not an IME. Existing hyphen/apostrophe names are preserved.
`drawNameKeyboard(canvas,editor,Rect{x,y,w,h},keyColor,textColor,font,pressedKey,labels)`
and `nameKeyAt(rect,x,y)` share exact 6-column × 5-row key rectangles. Gaps reject
hits. Choose a rectangle wholly within the round display's usable chord and
measure action labels against the narrowest key interior. The Watch uses Latin24
for letters/actions and Cjk22 for its two-character Chinese actions inside a
47 px minimum key interior. Reserve an independent name field above the keyboard.

`UxTokens.h` provides space constants (4,8,12,16,24), motion durations (90,180,240
ms), cubic easeOut, and frame-time independent exponential approach.

## Font provenance and regeneration

Raster subsets derive from Noto Sans SC Regular, distributed under the SIL Open
Font License 1.1. Preserve [fonts/OFL.txt](fonts/OFL.txt) when redistributing the
font-derived coverage data. Component source uses the repository MIT license.
The source font is [Google Fonts' Noto Sans SC](https://github.com/google/fonts/blob/5e35378e6bda803962ee6fd257e444a7d459660d/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf)
from commit `5e35378e6bda803962ee6fd257e444a7d459660d`; its SHA-256 is recorded in
`fonts/source.sha256`. The 17 MB full font is not included in firmware or this
library. Regenerate using Python and Pillow (validated with Pillow 12.3.0):

```sh
python tools/generate_fonts.py /path/to/NotoSansSC.ttf
```

The generator selects the Regular variation, rasterizes at each native size and
quantizes FreeType coverage to four bits. It writes independent C++ objects.

## Verification

From repository root:

```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iprojects/shared-libs/ux-components/src \
  projects/shared-libs/ux-components/test/components_test.cpp projects/shared-libs/ux-components/src/Font*.cpp \
  -o /tmp/ux-components-test
/tmp/ux-components-test /tmp/ux-components-native.ppm
```

The PPM is direct native raster output, with no desktop replacement fonts.
Tests check partial coverage in both geometry and Chinese text, glyph presence,
clip safety, 80 optimized-vs-reference fractional rounded rectangles, hollow
strokes, span operation counts, continuous scroll/inertia/tap suppression, all
30 keyboard hit rectangles, and name limits/fallback. A 350×58 rounded panel uses
58 solid spans and fewer than 200 individual pixel writes, instead of 20,300.
These are host correctness and operation-count checks, not device FPS claims.
Measure target steady frame timing and memory in the consuming app; serial image
transfer time must be excluded from frame-time measurements.

## Sound product

[UxSound / UxSoundM5 / optional UxSoundPcm](SOUND.md) provide enveloped, harmonic
UI cues, musical notes/scales, and signed/unsigned 8-bit PCM playback. The synth
owns no hardware and uses no heap. The M5 output adapter uses three fixed buffers,
handles the speaker's borrowed-buffer contract and tolerates 100 ms host frames.
Sound is independently linked and has no font or graphics dependency.

Keyboard labels are optional: `NameKeyboardLabels zhKeys={"删除","空格","Aa","确定"}`
can be passed as the final pointer argument with `Cjk18`. Omit the pointer to keep
English labels and avoid linking the Chinese atlas into Latin-only consumers.
Name entry remains English in either UI language.

## Captured pointer controls

`PointerSession::begin` captures one target and its rectangle. `end(x,y,nowMs)`
activates only that same target on release inside, within 1,000 ms. Leaving and
re-entering a non-scroll control is allowed. In a scrollable list, 20 px of
predominantly vertical travel permanently gives the sequence to scrolling;
returning to the original point cannot turn that drag into a click. Hosts cancel
inertia before capturing a fresh target, and reserve fixed footer hit regions
outside the list. Feed contact edges independently of SDK gesture classifications;
see the [actual M5Unified source regression probe](test/sdk_touch/README.md).

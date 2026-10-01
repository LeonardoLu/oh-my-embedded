# Shared UX products: implementation and verification

Implemented in `projects/shared-libs/ux-components`; see its README for exact API and font
regeneration instructions. The independently usable products are UxRender,
UxText, UxInput, UxKeyboard and UxTokens. Device owners integrate them without
changing canvas ownership or the host frame loop.

Rendering uses native RGB565 coverage, with solid scanline spans for rounded
panel interiors and individual blended pixels only at boundaries. Hollow AA
strokes preserve existing content. Typography uses native 4-bit Noto Sans SC
Regular coverage at Latin14/18/24, Clock36 and bounded Cjk18. It is not a browser
font substitution or a scaled 1-bit glyph mask. The Watch settings extension adds
native Latin28 plus bounded Cjk22/24/28 faces while retaining the smaller faces
for other surfaces. Font-derived assets retain the
upstream SIL OFL 1.1 notice; component code is MIT. Source font SHA256 and a pinned
Google Fonts source revision are recorded under the component.

Input uses continuous positive pixel offsets, six-pixel drag hysteresis, a
latched no-tap-after-drag rule, and time-based inertia clamped to content bounds.
A shared 16-character English name editor and exact draw/hit rectangles support
both rectangular and round device viewports. Naming persistence and page
cancellation are caller responsibilities. No Chinese IME is provided.

Verified on host with C++11 and `-Wall -Wextra -Werror`:

- Both shape and Chinese text output contain intermediate blended RGB565 colors.
- All Bot Chinese description glyphs and UI weekday/language glyphs exist. The
  test scans the current BotUx.cpp Chinese source text directly against the actual
  Cjk18 atlas (265 glyphs, 31,811 coverage bytes after natural-description updates).
- Eighty fractional/clipped rounded rectangles match the reference SDF raster
  pixel-for-pixel after the scanline optimization.
- A 350×58 rounded panel uses 58 solid spans and fewer than 200 pixel writes,
  instead of 20,300 per-pixel calls. Hollow strokes preserve central pixels.
- Scroll motion follows individual pixels, cannot overflow bounds, and a drag
  returning to its origin still cannot become a tap. Duplicate release coordinates
  preserve the most recent nonzero velocity through 90 ms; a longer stationary
  hold cancels momentum, with or without intermediate stationary samples.
- All 30 keyboard center hits map to their rendered key rectangles; gaps reject
  hits, and names enforce length, allowed characters and Milo empty fallback.
- Independent static archive linkage keeps every face in its own translation
  unit, so consumers link only the font objects they reference.

`test/components_test.cpp` emits a native RGB565-derived PPM preview including
Chinese text and the naming keyboard; the preview uses the exact firmware
renderer and coverage data. Native device screenshots, steady frame timing,
heap/PSRAM measurements and Core2 reconnect acceptance remain integration-owner
checks. These host checks do not claim on-device FPS or hardware acceptance.

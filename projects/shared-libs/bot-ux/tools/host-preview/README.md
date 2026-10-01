# BotUx host preview

This small host-only harness compiles the real `BotUx.cpp` against an SVG-emitting
`M5Canvas` stand-in. It calls the actual `update()` and `draw()` paths and
produces mood sheets at 40, 72, and 200 px plus expression and animation pickers.
It also writes an interactive `animation-player.html`: each mode contains 60
frames sampled at 15 fps from the real C++ component. JavaScript only selects
and plays those frames; it does not substitute a CSS animation.

From this `tools/host-preview/` directory:

```sh
./render.sh
```

The generated `tmp/botux-preview/` directory is disposable. Assertions exercise
visibly distinct choices, direct and eased pose changes, normalized motion
input, reduced motion, and updates as sparse as 3 fps. The preview proves layout
and primitive-call behavior; it does not claim pixel-perfect M5GFX font or shape
rasterization, display transfer timing, or hardware performance.

`transitions.svg` shows eight successive 100 ms frames of Neutral → Joy and
Neutral → Wink. Pixel/rectangle drawing also maintains an RGB565 buffer, allowing
coverage-color and face-placement checks at 72, 200, and 310 px. SVG pixel
rectangles use `crispEdges` so a browser does not add its own antialiasing or
seams between adjacent scanlines. Native circle/triangle/round-rectangle calls
still use SVG approximations; this harness is not a replacement for M5GFX raster
capture or device frame-time measurements.


The complete bilingual offline catalog is generated with:

```sh
./render.sh
../../../../../tmp/botux-preview/botux-preview ../../../../../tmp/botux-preview --catalog
python3 catalog.py ../../../../../tmp/botux-preview
```

`catalog.py` requires Pillow. It packages 72 real 120px frames for every mood,
expression and animation into `wiki/bot-ux/assets/` GIFs and the self-contained
`wiki/bot-ux/intro.html`; `intro.md` is the matching illustrated source guide.
The browser has only search/filter/pause behavior; movement comes from the C++
renderer. GIFs repeat a finite capture and may show a loop boundary that is not
present in continuous firmware. Asleep's live z cycles have a separate native
wrap-continuity, reduced-motion and zero-motion regression test.

`test-orb-motion.sh` separately verifies the Thinking breathing shell and
Working vortex at native target sizes, including their 1.6/1.4-second base-phase
wraps, depth clusters, speed, low/reduced/zero motion, eye suppression and
explicit-animation composition. It also emits disposable RGB565 PPM stills and
16-frame full-cycle StopWatch-profile sequences under `tmp/botux-orb-motion/`.
At the level-two StopWatch controls (`speed=0.73`, `amount=0.55`), those base
periods become approximately 4.0 and 3.5 seconds.

# Watch button-feedback refresh optimization

The liquid button renderer originally restored each button's full fixed dirty rectangle. Those broad rectangles overlap the 286×286 Bot bounding box even though the liquid pixels themselves do not. To keep those broad restores coherent, every feedback frame also copied the Bot into the clean full-screen canvas. The original setup separately recalculated all 27 antialias masks with `sqrtf` and `atan2f`.

The optimized renderer keeps the visual model and its 16 ms animation schedule unchanged:

- A host generator includes `WatchButtonFeedback.h` and encodes the existing 27 masks losslessly. Its four token types represent literal, zero, 255, and repeated-byte runs of 1–64 pixels. The 754,785-byte decoded layout remains in PSRAM; its Flash payload is 29,969 bytes. Setup validates the exact output length and fails closed if decoding does not produce the full buffer.
- Each Face feedback restore subtracts the half-open Bot rectangle `{90,90,286,286}` from the button's fixed dirty rectangle. When both 90 px HUD bands were already submitted with feedback composited into them, those bands are subtracted too. The remaining non-overlapping rectangles cover every liquid pixel, including release restoration, without writing any Bot pixel. M5GFX's even-coordinate flush expansion also remains outside the excluded Bot and HUD regions.
- The current 286×286 Bot sprite is still drawn and submitted to the panel on every active Face frame. A non-transition feedback frame no longer copies that same sprite into the clean canonical canvas. Full-screen transitions still copy the Bot and submit one fully composited frame. HUD submissions remain composited before their first panel write, and frame capture still invalidates or rebuilds canonical state rather than leaving captured overlays in the clean base.

The generated header is reproducible: the checked-in generator rebuilt it byte for byte. Host checks decode malformed, truncated, and overlong streams safely and compare all 754,785 output bytes with the same C++ coverage function. On the ESP32, decoding took 24 ms instead of 805 ms for the old runtime calculation. A temporary device-side recomputation found five alpha values that differ by 1/255 from the host-generated asset, with maximum delta 1. This is the bounded effect of the host and ESP32 math libraries near coverage rounding boundaries; host generation and decoding themselves are byte-exact. Runtime blending still uses native `lgfx::rgb565_t`, so this work does not change the established typed color path.

Regenerate the asset from the repository root with:

```sh
c++ -std=c++11 -O2 -Wall -Wextra -Werror -Iprojects/m5stack-stopwatch/bot-ux-watch/include projects/m5stack-stopwatch/bot-ux-watch/tools/generate_button_feedback_masks.cpp -o tmp/generate_button_feedback_masks
tmp/generate_button_feedback_masks projects/m5stack-stopwatch/bot-ux-watch/include/WatchButtonFeedbackMasks.generated.h
```

Any change to button geometry, animation steps, or coverage math must regenerate the header and run `sh tools/check_host.sh`; that check independently regenerates the file, compares it byte for byte, and reruns mask and patch geometry coverage.

## Matched device profile

The same six scenarios and filter boundaries as [watch-button-feedback-profile.json](watch-button-feedback-profile.json) were captured with temporary buffered counters. The four Face cases are settled diagnostic render frames. They exercise the production compositing path but do not measure physical switch latency, panel optical response, or user-perceived FPS.

| Scenario | New n | Pixels old → new | Mean old → new | Change | P95 old → new |
|---|---:|---:|---:|---:|---:|
| Face, one A, warm cached base | 11 | 111,716 → 104,226 | 52.083 → 38.464 ms | −26.15% | 52.512 → 38.613 ms |
| Face, three keys plus rebuilt base/HUD | 12 | 249,541 → 190,758 | 137.607 → 99.796 ms | −27.48% | 137.984 → 100.241 ms |
| Face, three keys plus HUD, cached base | 12 | 249,541 → 190,758 | 124.360 → 87.352 ms | −29.76% | 124.730 → 87.707 ms |
| Face, three-key release plus HUD | 12 | 249,541 → 190,758 | 107.897 → 78.022 ms | −27.69% | 108.344 → 78.107 ms |
| Settings, PWR appear | 12 | 24,025 → 24,025 | 12.117 → 12.109 ms | −0.06% | 12.469 → 12.410 ms |
| Settings, PWR release | 12 | 24,025 → 24,025 | 8.728 → 8.736 ms | +0.08% | 9.016 → 9.034 ms |

The warm three-key/HUD case shows the intended component changes: canvas-copy time fell from 32.685 to 14.225 ms, blend time from 17.239 to 9.197 ms, and M5GFX submission time from 44.626 to 33.063 ms. Bot drawing stayed at about 14.8 ms because the renderer preserves the complete animated Bot draw and panel submission. The static Settings cases are controls: this Face-specific geometry change does not optimize them, and their small changes are measurement variation.

The pixel totals follow directly from geometry. A warm A frame keeps the 81,796-pixel Bot submission and changes its feedback restore from 29,920 to 22,430 pixels, totaling 104,226. A three-key HUD frame submits 83,880 HUD pixels, 81,796 Bot pixels, and 25,082 middle-band feedback pixels, totaling 190,758. The totals are transfer counts, not isolated component execution times.

Full machine-readable aggregates and measurement boundaries are in [watch-button-feedback-optimized-profile.json](watch-button-feedback-optimized-profile.json). Raw logs and the temporary instrumentation remain ignored under `tmp/watch-feedback-opt/`; the profiling hooks are not part of production firmware.

## Production verification

The final production source is renderer commit `7ac55d9` on top of mask-asset commit `954b570`; temporary comparison and profiling hooks were removed before the final build and upload. The focused AMOLED flush-alignment test passed, and the complete host runner reported 29 passes, including generated-header reproducibility, all mask bytes, every liquid animation step, rectangle subtraction, typography, shared components, sound, and Bot preview checks.

The StopWatch build used 48,920 bytes of RAM and 1,055,457 bytes of the configured application Flash region. The 1,055,824-byte `firmware.bin` has SHA-256 `89c8cdffa912e1a300bdd66731683b5fe135458d46d53695582589b529dc740b`; upload completed with data-hash verification. Production boot reported `keys_mask_ms=24` and the expected 754,785-byte decoded mask buffer. Final device captures covered all three liquid buttons on Face, their release, PWR on Settings, and its release; review found the expected colors, antialiasing, Bot/HUD continuity, and clean restoration. These diagnostic masks validate the render path rather than physical held/release timing. The restored state was Face with manual mode and diagnostics off, the saved button effect off, a valid unpressed power input, and the identity touch profile. The serial port was closed after capture.

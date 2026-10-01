# Watch button-feedback render profiling

The production renderer at source commit `52493f6` was measured on the StopWatch with temporary, command-gated `micros()` counters. Counters accumulated in memory and were printed only after capture. The temporary instrumentation was removed before restoring the production firmware.

The diagnostic key masks enter the same render paths and use the same cached AA masks as physical buttons. They establish render cost, not physical switch latency or optical panel response. `total` begins at `render()` entry and ends after the final feedback submission and `recordFrame`. It excludes `M5.update`, PMIC/touch I2C reads, gesture handling before render, serial command latency, panel scanout, and human-visible response.

## Measured scenarios

All values below are means in microseconds. P95 is the nearest-rank percentile of complete per-frame totals. “Interface” combines UI draw, base clear, and HUD draw. “Copy” combines Bot-to-canvas and canvas-to-scratch copies. “Display library” includes M5GFX range alignment, memory/format conversion, DMA-buffer handling, bus submission, and `waitDisplay`; it is not pure QSPI wire time. “Other” is each sample's total less the mutually exclusive timed stages.

| Scenario | n | pixels | State | Bot draw | Interface | Copy | AA blend | Display library | Other | Total mean | Total median | Total p95 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Face, one A, warm cached base | 13 | 111,716 | 302 | 14,739 | 0 | 14,685 | 4,217 | 18,088 | 53 | 52,083 | 52,084 | 52,512 |
| Face, three keys plus rebuilt base/HUD | 12 | 249,541 | 407 | 14,773 | 27,764 | 32,682 | 17,249 | 44,640 | 92 | 137,607 | 137,556 | 137,984 |
| Face, three keys plus HUD, cached base | 12 | 249,541 | 267 | 14,803 | 14,650 | 32,685 | 17,239 | 44,626 | 91 | 124,360 | 124,312 | 124,730 |
| Face, three-key release plus HUD | 12 | 249,541 | 305 | 14,779 | 15,247 | 32,679 | 0 | 44,815 | 72 | 107,897 | 107,864 | 108,344 |
| Settings, PWR appear | 12 | 24,025 | 378 | 0 | 8 | 3,368 | 3,402 | 4,913 | 48 | 12,117 | 11,973 | 12,469 |
| Settings, PWR release | 12 | 24,025 | 375 | 0 | 7 | 3,359 | 0 | 4,940 | 47 | 8,728 | 8,592 | 9,016 |

The warm one-key group intentionally excludes periodic HUD frames. A first key frame after the ordinary Face path invalidates the clean canvas; its direct `base_fill` counter measures 12.508 ms. The two three-key/HUD groups preserve the same 249,541 submitted pixels and differ by 13.247 ms in total, but that difference also includes small changes in HUD and other stages, so it is not a pure base-clear measurement.

Machine-readable statistics, including component mean/median/p95/range and the measurement boundaries, are in `projects/m5stack-stopwatch/bot-ux-watch/specs/watch-button-feedback-profile.json`. Raw logs remain in `tmp/watch-edge-buttons/stage-profile-*-raw.log` for local audit.

After measurement, the temporary counters were removed and the clean `52493f6` production source was rebuilt and uploaded. The restored 1,026,768-byte image has SHA-256 `8b203a922621d8e2b8f5c42c1df93fbb72e264800118685676c700983002b3e2`; the new binary hash differs from the earlier archived build because the firmware embeds compile-time strings. Upload verification passed. The final device report showed Face, manual mode off, diagnostics off, the user's saved Chinese/sound/button-effect preferences unchanged, and the identity touch profile with no stored candidate.

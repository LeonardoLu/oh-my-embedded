# StopWatch settings icon sources

These five 200 × 200 PNG files are the lossless RGB565 sources for the Watch
settings launcher. Their black background is intentional: the launcher follows
the factory StopWatch's black AMOLED presentation.

| Local source | Meaning | Provenance |
| --- | --- | --- |
| `time.png` | Time | Exact RGB565 pixels decoded from M5Stack `icon_watch_face.c` |
| `bot.png` | Bot personality | Original project artwork, 2026-09-16 |
| `display.png` | Display | Exact RGB565 pixels decoded from M5Stack `icon_setup.c` |
| `sound.png` | Sound | Exact RGB565 pixels decoded from M5Stack `icon_fft.c` |
| `power.png` | Power saving | Original project artwork, 2026-09-16 |

The three M5Stack images come from
[`m5stack/M5StopWatch-UserDemo`](https://github.com/m5stack/M5StopWatch-UserDemo)
commit `6b4aa125288b6fe9dca661f10159f6e1e5ee785c`. That repository is MIT
licensed, Copyright (c) 2026 M5Stack Technology CO LTD. The required license
notice is retained in `M5STACK-MIT.txt` beside these sources.

The Bot and Power images are original drawings made for this repository. They
use the same RGB565 palette, glass gradient, soft halo and white specular edge
as the factory assets. Bot uses the shared companion's dark pill-eye language;
Power uses a conventional upright battery and large lightning mark.

`tools/generate_settings_icon_assets.py` quantizes these files to native-order
RGB565, losslessly applies row-local PackBits, and regenerates
`include/WatchSettingsIconAssets.h`. Every command stays inside one row. The
runtime decoder therefore needs one fixed 200-pixel (400-byte) scanline and no
heap allocation or full icon buffer.

Regenerate with the repository-bundled Pillow and NumPy runtime:

```sh
python3 stopwatch/bot-ux-watch/tools/generate_settings_icon_assets.py
```

# StopWatch settings icon sources

The launcher uses one unified set of five Phosphor Duotone symbols. Each icon
starts as a two-level vector mask, then receives the same diagonal gradient,
upper-left sheen and restrained radial halo on a pure-black AMOLED background.
Only the semantic colors differ between icons.

| Local output | Meaning | Phosphor symbol |
| --- | --- | --- |
| `time.png` | Time | `clock-duotone` |
| `bot.png` | Bot personality | `robot-duotone` |
| `display.png` | Display brightness | `sun-duotone` |
| `sound.png` | Sound | `speaker-high-duotone` |
| `power.png` | Battery and power saving | `battery-charging-vertical-duotone` |

The exact upstream SVGs are retained in `source/phosphor-duotone/` from
[`phosphor-icons/core`](https://github.com/phosphor-icons/core) commit
`2b75f3ad12b420c9504ef05df8d2564a28f8500e`. Phosphor is MIT licensed,
Copyright (c) 2023 Phosphor Icons; `PHOSPHOR-MIT.txt` retains the notice.
The set was selected through the
[Iconify Phosphor catalog](https://icon-sets.iconify.design/ph/), whose API
exposes these same symbols.

Flaticon packs and Apple's SF Symbols were reviewed as visual references during
selection. No artwork from either source is included in these files.

The tracked 512 × 512 files in `source/masks/` are transparent raster snapshots
of those SVGs. Keeping masks in the repository makes the color composition
portable and deterministic without requiring an SVG renderer in firmware or in
the normal asset-generation command. `tools/render_settings_icons.py` contains
all layout, color, gradient, halo and sheen parameters and creates the five
200 × 200 RGB PNGs above.

`tools/generate_settings_icon_assets.py` first runs that composition, quantizes
the results to native-order RGB565, and losslessly applies row-local PackBits to
regenerate `include/WatchSettingsIconAssets.h`. Every command stays inside one
row, so the runtime decoder uses one fixed 200-pixel (400-byte) scanline with no
heap allocation or full icon buffer.

Regenerate every derived icon asset and the firmware header in one command:

```sh
python3 stopwatch/bot-ux-watch/tools/generate_settings_icon_assets.py
```

To refresh all mask snapshots from the fixed SVG sources on macOS, run:

```sh
sh stopwatch/bot-ux-watch/tools/rasterize_settings_icon_masks.sh
```

The script invokes `sips -s format png -z 512 512` for each source and writes
directly to `source/masks/`; this step is only needed when the selected upstream
symbols change.

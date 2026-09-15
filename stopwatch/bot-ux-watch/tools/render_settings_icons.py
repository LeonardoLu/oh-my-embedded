#!/usr/bin/env python3
"""Compose the Watch launcher icons from tracked Phosphor duotone masks."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


SIZE = 200
MASK_SIZE = 512
SYMBOL_CANVAS = 166


@dataclass(frozen=True)
class IconRecipe:
    name: str
    symbol: str
    first: tuple[int, int, int]
    last: tuple[int, int, int]


# All five icons use the same geometry, lighting direction, halo and sheen.
# The two semantic colors are the only per-icon rendering difference.
RECIPES = (
    IconRecipe("time", "clock-duotone", (255, 196, 54), (255, 105, 40)),
    IconRecipe("bot", "robot-duotone", (75, 235, 255), (102, 92, 255)),
    IconRecipe("display", "sun-duotone", (90, 238, 255), (38, 112, 255)),
    IconRecipe("sound", "speaker-high-duotone", (255, 103, 224), (133, 73, 255)),
    IconRecipe("power", "battery-charging-vertical-duotone",
               (115, 255, 190), (35, 208, 97)),
)


def _resample_mask(path: Path) -> Image.Image:
    image = Image.open(path).convert("RGBA")
    if image.size != (MASK_SIZE, MASK_SIZE):
        raise ValueError(f"{path}: expected {MASK_SIZE}x{MASK_SIZE}, got {image.size}")
    alpha = image.getchannel("A").resize(
        (SYMBOL_CANVAS, SYMBOL_CANVAS), Image.Resampling.LANCZOS)
    result = Image.new("L", (SIZE, SIZE), 0)
    inset = (SIZE - SYMBOL_CANVAS) // 2
    result.paste(alpha, (inset, inset))
    return result


def _gradient(first: tuple[int, int, int],
              last: tuple[int, int, int]) -> Image.Image:
    y, x = np.mgrid[0:SIZE, 0:SIZE]
    amount = np.clip((0.62 * y + 0.38 * x) / (SIZE - 1), 0, 1)[..., None]
    start = np.array(first, dtype=np.float32)
    end = np.array(last, dtype=np.float32)
    rgb = (start * (1 - amount) + end * amount).astype(np.uint8)
    pixels = np.empty((SIZE, SIZE, 4), dtype=np.uint8)
    pixels[:, :, :3] = rgb
    pixels[:, :, 3] = 255
    return Image.fromarray(pixels, "RGBA")


def _radial(color: tuple[int, int, int]) -> Image.Image:
    y, x = np.mgrid[0:SIZE, 0:SIZE]
    distance = np.sqrt(((x - 96) / 91) ** 2 + ((y - 91) / 84) ** 2)
    alpha = np.clip(1 - distance, 0, 1) ** 2 * 255 * 0.31
    pixels = np.zeros((SIZE, SIZE, 4), dtype=np.uint8)
    pixels[:, :, :3] = color
    pixels[:, :, 3] = alpha.astype(np.uint8)
    return Image.fromarray(pixels, "RGBA")


def _compose(mask: Image.Image, recipe: IconRecipe) -> Image.Image:
    result = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 255))
    midpoint = tuple((first + last) // 2
                     for first, last in zip(recipe.first, recipe.last))
    result = Image.alpha_composite(result, _radial(midpoint))

    glow_alpha = mask.filter(ImageFilter.GaussianBlur(15)).point(
        lambda value: int(value * 0.34))
    glow = _gradient(recipe.first, recipe.last)
    glow.putalpha(glow_alpha)
    result = Image.alpha_composite(result, glow)

    glyph = _gradient(recipe.first, recipe.last)
    glyph.putalpha(mask)
    result = Image.alpha_composite(result, glyph)

    y, x = np.mgrid[0:SIZE, 0:SIZE]
    sheen = np.clip(
        1 - np.sqrt(((x - 73) / 92) ** 2 + ((y - 60) / 76) ** 2), 0, 1) ** 2
    mask_fraction = np.asarray(mask, dtype=np.float32) / 255
    sheen_alpha = (sheen * mask_fraction * 72).astype(np.uint8)
    highlight = Image.new("RGBA", (SIZE, SIZE), (255, 255, 255, 0))
    highlight.putalpha(Image.fromarray(sheen_alpha, "L"))
    return Image.alpha_composite(result, highlight).convert("RGB")


def render_icons(mask_dir: Path, output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    svg_dir = mask_dir.parent / "phosphor-duotone"
    for recipe in RECIPES:
        svg = svg_dir / f"{recipe.symbol}.svg"
        if not svg.is_file():
            raise FileNotFoundError(f"missing fixed vector source: {svg}")
        mask = _resample_mask(mask_dir / f"{recipe.name}.png")
        output = output_dir / f"{recipe.name}.png"
        _compose(mask, recipe).save(output, optimize=True, compress_level=9)
        print(f"wrote {output}")


def main() -> None:
    project = Path(__file__).resolve().parents[1]
    assets = project / "assets" / "settings-icons"
    parser = argparse.ArgumentParser()
    parser.add_argument("--masks", type=Path,
                        default=assets / "source" / "masks")
    parser.add_argument("--output", type=Path, default=assets)
    args = parser.parse_args()
    render_icons(args.masks, args.output)


if __name__ == "__main__":
    main()

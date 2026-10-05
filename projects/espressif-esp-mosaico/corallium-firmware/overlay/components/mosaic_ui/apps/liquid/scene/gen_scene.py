#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
import sys

from PIL import Image

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent.parent.parent / "common"))
from font_paths import DEJAVU_SANS, DEJAVU_SANS_BOLD  # noqa: E402
from scene_common import (  # noqa: E402
    asset_scene, button, container, explicit_charset, image, label, layer,
    scene_out_path, shared_charset, shared_prefix, write_scene,
)

FONT_POLICIES = {
    16: shared_charset(),
    18: shared_charset(),
    30: explicit_charset("PixelGradientWater‹"),
}


def main():
    Image.new("RGB", (456, 320), "#000511").save(HERE / "liquid_canvas.png")
    objs, content = shared_prefix([], FONT_POLICIES, DEJAVU_SANS_BOLD)
    page = len(objs)
    objs.append(layer(content, 0, 0, 480, 480, name="liquid_root"))
    objs.append(button(page, 12, 14, 40, 44, "‹", size=30, bg="#000000",
                       fg="#D6D6DE", name="liquid_return", callback="liquid_return"))
    objs.append(label(page, 54, 16, 400, 42, "Pixel", size=30,
                      color="#D6D6DE", bind="liquid_title", name="liquid_title",
                      font_charset="PixelGradientWater"))
    canvas = image(page, "liquid_canvas.png", 12, 74, 456, 320,
                   bind="liquid_canvas", name="liquid_canvas")
    canvas["codec"] = "raw"
    objs.append(canvas)
    objs.append(label(page, 12, 48, 456, 24, "", size=16, align="center",
                      color="#FFB020", bind="liquid_status", name="liquid_status"))
    objs.append(button(page, 12, 410, 328, 42, "", size=18, radius=14,
                       name="liquid_theme", callback="liquid_theme"))
    objs.append(label(page, 12, 418, 328, 28, "Deep Sea", size=18, align="center",
                      bind="liquid_palette_name", name="liquid_palette_name"))
    objs.append(button(page, 352, 410, 116, 42, "Reset", size=18, radius=14,
                       name="liquid_reset", callback="liquid_reset"))
    write_scene(scene_out_path(HERE, "liquid_480.json"), "liquid", objs,
                font=DEJAVU_SANS)
    asset_scene(scene_out_path(HERE, "liquid_assets_480.json"), "liquid_assets",
                [], FONT_POLICIES, DEJAVU_SANS_BOLD, DEJAVU_SANS)


if __name__ == "__main__":
    main()

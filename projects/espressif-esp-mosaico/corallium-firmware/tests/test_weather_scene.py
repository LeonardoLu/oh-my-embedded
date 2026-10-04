#!/usr/bin/env python3
"""Check actual Weather illustration masks and native shell title geometry."""

import argparse
import json
import math
from pathlib import Path
import re

from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", type=Path, required=True)
    args = parser.parse_args()
    ui = args.upstream.resolve() / "components/mosaic_ui"
    scene_dir = ui / "apps/weather/scene"
    objects = json.loads((scene_dir / "weather_480.json").read_text())["objects"]
    by_name = {obj["name"]: obj for obj in objects if obj.get("name")}
    panel = objects[by_name["weather_forecast_list"]["parent"]]
    status = by_name["weather_status"]
    header_h = 64
    assert 0 <= status["x"] < status["x"] + status["w"] <= 480
    assert 0 <= status["y"] < status["y"] + status["h"] <= header_h

    smallest_gap = panel["y"]
    for name in ("overcast", "sunny", "cloudy", "snow", "windy", "thunder"):
        artwork = by_name[f"weather_art_{name}"]
        icon = by_name[f"weather_{name}_image"]
        assert objects[icon["parent"]] is artwork
        assert artwork["clip_children"] and icon["fit"] == "contain"
        image = Image.open(scene_dir / icon["image"]).convert("RGBA")
        box = image.getchannel("A").getbbox()
        assert box, name
        scale = min(icon["w"] / image.width, icon["h"] / image.height)
        origin_x = artwork["x"] + icon["x"] + (icon["w"] - image.width * scale) / 2
        origin_y = artwork["y"] + icon["y"] + (icon["h"] - image.height * scale) / 2
        # Include a pixel of resampling fringe on every side of the real mask.
        ink = (math.floor(origin_x + box[0] * scale) - 1,
               math.floor(origin_y + box[1] * scale) - 1,
               math.ceil(origin_x + box[2] * scale) + 1,
               math.ceil(origin_y + box[3] * scale) + 1)
        assert artwork["x"] <= ink[0] and ink[2] <= artwork["x"] + artwork["w"], (name, ink)
        assert artwork["y"] <= ink[1] and ink[3] <= artwork["y"] + artwork["h"], (name, ink)
        assert ink[1] >= header_h and ink[3] <= panel["y"] - 4, (name, ink)
        assert status["y"] + status["h"] <= ink[1], (name, ink)
        smallest_gap = min(smallest_gap, panel["y"] - ink[3])

    # The native shell uses an A8 asset rather than the scene's font catalog.
    titles = (ui / "common/mosaic_app_shell_titles.c").read_text()
    entry = re.search(
        r'\{"Weather", (s_title_\d+), sizeof\(\1\), (\d+), (\d+), (\d+)\}',
        titles)
    assert entry, "Weather title is absent from the native shell"
    symbol, width, height, stride = entry.groups()
    width, height, stride = map(int, (width, height, stride))
    body = re.search(r"static const uint8_t " + symbol + r"\[\] = \{(.*?)\};",
                     titles, re.S)[1]
    alpha = bytes(int(value) for value in re.findall(r"\b\d+\b", body))
    assert len(alpha) == stride * height and stride >= width
    assert any(alpha), "Weather title bitmap is empty"
    title_x, title_y = 52, 26
    assert title_y + height <= header_h
    assert title_x + width + 16 <= status["x"], "Weather title/status overlap"
    print(f"PASS: six Weather masks keep at least {smallest_gap} px from card; "
          f"{width}x{height} native title and status fit header")


if __name__ == "__main__":
    main()

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
    home_dir = ui / "hub/scene"
    home = json.loads((home_dir / "mosaic_hub_480.json").read_text())["objects"]
    home_names = {obj["name"]: obj for obj in home if obj.get("name")}
    card = home_names["clock_card"]
    text_right = max(home_names[name]["x"] + home_names[name]["w"]
                     for name in ("home_weather_temp", "home_weather_desc", "home_weather_city"))
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

        home_art = home_names[f"home_weather_art_{name}"]
        home_icon = home_names[f"home_weather_{name}_image"]
        assert home[home_icon["parent"]] is home_art
        assert home_art["clip_children"] and home_icon["fit"] == icon["fit"]
        # Both packages must use the same original file at the same native
        # scale, rather than approximating the dots or making a second bitmap.
        assert (home_dir / home_icon["image"]).resolve() == (scene_dir / icon["image"]).resolve()
        assert (home_icon["w"], home_icon["h"]) == (icon["w"], icon["h"])
        hx = home_art["x"] + home_icon["x"]
        hy = home_art["y"] + home_icon["y"]
        home_ink = (math.floor(hx + box[0] * scale) - 1,
                    math.floor(hy + box[1] * scale) - 1,
                    math.ceil(hx + box[2] * scale) + 1,
                    math.ceil(hy + box[3] * scale) + 1)
        assert home_art["x"] <= home_ink[0] and home_ink[2] <= home_art["x"] + home_art["w"], (name, home_ink)
        assert home_art["y"] <= home_ink[1] and home_ink[3] <= home_art["y"] + home_art["h"], (name, home_ink)
        assert text_right + 8 <= home_ink[0], (name, home_ink)
        assert card["x"] + 8 <= home_ink[0] and home_ink[2] <= card["x"] + card["w"] - 8
        assert card["y"] + 8 <= home_ink[1] and home_ink[3] <= card["y"] + card["h"] - 8

    for source_path in (ui / "hub/mosaic_hub_app.c", ui / "apps/weather/weather_app.c"):
        assert '"mosaic_weather_art.h"' in source_path.read_text()
        assert "mosaic_weather_art_select(" in source_path.read_text()

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
          f"{width}x{height} native title and status fit header; Home uses identical art and scale")


if __name__ == "__main__":
    main()

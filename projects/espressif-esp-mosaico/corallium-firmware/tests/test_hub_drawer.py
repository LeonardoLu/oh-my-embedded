#!/usr/bin/env python3
"""Check full-screen drawer coverage and compact control/hit geometry.

With --gspc, render the authored Hub through the pinned native simulator. The
presentation fixture sets the drawer open and supplies controlled slider/toggle
states; it does not exercise the Hub C backend or open/close gestures.
"""

import argparse
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile

from PIL import Image, ImageChops


def absolute_assets(node, scene_dir):
    if isinstance(node, dict):
        return {key: absolute_assets(value, scene_dir) for key, value in node.items()}
    if isinstance(node, list):
        return [absolute_assets(value, scene_dir) for value in node]
    if isinstance(node, str) and node.endswith((".png", ".svg", ".ttf")):
        return str((scene_dir / node).resolve())
    return node


def run(command, log):
    result = subprocess.run(command, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, timeout=120)
    log.write_text(result.stdout)
    if result.returncode:
        raise RuntimeError(f"Native drawer check failed; see {log}\n{result.stdout[-2000:]}")


def check(args, output=None):
    scene_dir = args.upstream.resolve() / "components/mosaic_ui/hub/scene"
    scene = json.loads((scene_dir / "mosaic_hub_480.json").read_text())
    objects = scene["objects"]
    named = {obj["name"]: (index, obj) for index, obj in enumerate(objects)
             if obj.get("name")}
    drawer_index, drawer = named["quick_drawer"]
    main_index, main = named["quick_main"]
    _, background = named["quick_background"]
    assert main["parent"] == drawer_index and background["parent"] == main_index
    for obj in (drawer, main, background):
        assert (obj["x"], obj["y"], obj["w"], obj["h"]) == (0, 0, 480, 480)
    assert background["radius"] == 0
    assert background["bg_color"] == "#272727"
    assert background.get("opacity", 255) == 255 and not background.get("hidden")

    def position(obj):
        x = y = 0
        while True:
            x += obj.get("x", 0)
            y += obj.get("y", 0)
            parent = obj.get("parent", -1)
            if parent < 0 or parent == drawer_index:
                return x, y
            obj = objects[parent]

    _, group = named["quick_connectivity_group"]
    assert (group["w"], group["h"]) == (220, 220)
    for name, expected in (("quick_wlan", (34, 76)),
                           ("quick_bluetooth", (138, 76)),
                           ("quick_ringtone", (34, 180)),
                           ("quick_vibration", (138, 180))):
        _, obj = named[name]
        assert position(obj) == expected and (obj["w"], obj["h"]) == (72, 72)
    _, level = named["quick_level_group"]
    assert (level["w"], level["h"]) == (group["w"], group["h"])
    assert level["y"] == group["y"]
    level_index = named["quick_level_group"][0]
    descriptions = [obj.get("text") for obj in objects
                    if obj.get("parent") == level_index]
    assert "Volume" not in descriptions and "Brightness" not in descriptions
    for name, expected in (("quick_volume", (270, 70)),
                           ("quick_brightness", (374, 70))):
        _, slider = named[name + "_input"]
        _, fill = named[name + "_fill"]
        assert position(slider) == position(fill) == expected
        assert (slider["w"], slider["h"]) == (fill["w"], fill["h"]) == (72, 152)
        label = next(obj for obj in objects if obj.get("bind") == name + "_text")
        assert label["parent"] == level_index and label["y"] == 184
        assert label["text"].endswith("%") and label["font_charset"] == "0123456789%"
    for name in ("quick_wlan", "quick_bluetooth", "quick_ringtone", "quick_vibration"):
        for state in ("off", "on"):
            assert named[name + "_" + state][1]["codec"] == "raw"
    for name in ("quick_volume_icon", "quick_brightness_icon", "quick_up_chevron"):
        assert named[name][1]["codec"] == "raw"
    _, chevron = named["quick_up_chevron"]
    assert position(chevron) == (228, 450) and (chevron["w"], chevron["h"]) == (24, 24)
    assert not chevron.get("hidden") and chevron["y"] + chevron["h"] <= 480

    if args.gspc:
        base = absolute_assets(scene, scene_dir)
        base["objects"][drawer_index]["open"] = True
        for state in ("off", "on"):
            variant = copy.deepcopy(base)
            by_name = {obj.get("name"): obj for obj in variant["objects"]}
            if state == "on":
                # Match the runtime: white off-images stay visible for WLAN/BLE.
                for name in ("quick_wlan", "quick_bluetooth", "quick_vibration"):
                    by_name[name]["bg_color"] = "#FF4C01"
                    by_name[name + "_off"]["hidden"] = False
                    by_name[name + "_on"]["hidden"] = True
                by_name["quick_ringtone"]["bg_color"] = "#FF3B30"
                by_name["quick_ringtone_off"]["hidden"] = True
                by_name["quick_ringtone_on"]["hidden"] = False
                by_name["quick_ble_badge"]["hidden"] = False
                for name, value in (("quick_volume", 80), ("quick_brightness", 58)):
                    by_name[name + "_fill"]["value"] = value
                    by_name[name + "_input"]["value"] = value
                    next(obj for obj in variant["objects"] if obj.get("bind") == name + "_text")["text"] = f"{value}%"
            path = output / f"drawer-{state}.json"
            path.write_text(json.dumps(variant))
            bundle = output / f"drawer-{state}.gspb"
            run([args.gspc, "pack", str(path), "--deployable", "--profile",
                 str(scene_dir.parent.parent / "common/mosaic_rgb565_auto.yaml"),
                 "-o", str(bundle)], output / f"drawer-{state}-compile.log")
            picture = output / f"drawer-{state}.png"
            run([sys.executable, "-m", "gsp.execute", "--version", "1.6.0", "sim",
                 "--bundle", str(bundle), "--headless", "--frames", "4", "--dump",
                 str(picture), "--dump-format", "png", "--fail-on-error"],
                output / f"drawer-{state}-render.log")
            image = Image.open(picture).convert("RGB")
            image.save(picture)
            corners = [image.getpixel(point) for point in
                       ((0, 0), (479, 0), (0, 479), (479, 479))]
            assert len(set(corners)) == 1 and all(abs(value - 39) <= 6 for value in corners[0]), corners
            lower_background = image.crop((0, 340, 480, 440))
            assert not ImageChops.difference(lower_background,
                Image.new("RGB", lower_background.size, corners[0])).getbbox()
            for left, top in ((34, 76), (138, 76), (34, 180), (138, 180)):
                glyph = image.crop((left + 12, top + 12, left + 60, top + 60))
                white = sum(min(glyph.getpixel((x, y))) > 220
                            for y in range(glyph.height) for x in range(glyph.width))
                assert white > 30, (state, left, top, white)
            close = image.crop((228, 450, 252, 474))
            assert max(maximum for _, maximum in close.getextrema()) > 180
        print("PASS: native open drawer corners/lower background opaque; bottom chevron visible")
    print("PASS: full 480px drawer; compact buttons and slider hits preserved; percentage-only labels")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", type=Path, required=True)
    parser.add_argument("--gspc", help="Also render native open-drawer fixtures")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.gspc and not args.output:
        with tempfile.TemporaryDirectory(prefix="hub-drawer-") as directory:
            check(args, Path(directory))
    else:
        if args.output:
            args.output = args.output.resolve()
            args.output.mkdir(parents=True, exist_ok=True)
        check(args, args.output)


if __name__ == "__main__":
    main()

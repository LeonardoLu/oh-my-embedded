#!/usr/bin/env python3
"""Check Settings icon tiles, scrolling bounds and independent BLE controls."""

import argparse
import importlib.util
import json
from pathlib import Path
import re

from PIL import Image, ImageChops, ImageFont


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def decode_qoi(data):
    assert data[:4] == b"qoif"
    width = int.from_bytes(data[4:8], "big")
    height = int.from_bytes(data[8:12], "big")
    assert data[12] == 3 and data[-8:] == b"\0\0\0\0\0\0\0\1"
    index = [(0, 0, 0, 0)] * 64
    pixel = (0, 0, 0, 255)
    pixels = []
    cursor = 14
    while len(pixels) < width * height:
        command = data[cursor]
        cursor += 1
        if command == 0xFE:
            pixel = (*data[cursor:cursor + 3], pixel[3])
            cursor += 3
        elif command == 0xFF:
            pixel = tuple(data[cursor:cursor + 4])
            cursor += 4
        elif command >> 6 == 0:
            pixel = index[command]
        elif command >> 6 == 1:
            pixel = ((pixel[0] + ((command >> 4) & 3) - 2) & 255,
                     (pixel[1] + ((command >> 2) & 3) - 2) & 255,
                     (pixel[2] + (command & 3) - 2) & 255, pixel[3])
        elif command >> 6 == 2:
            second = data[cursor]
            cursor += 1
            green_delta = (command & 63) - 32
            pixel = ((pixel[0] + green_delta + (second >> 4) - 8) & 255,
                     (pixel[1] + green_delta) & 255,
                     (pixel[2] + green_delta + (second & 15) - 8) & 255,
                     pixel[3])
        else:
            pixels.extend([pixel[:3]] * ((command & 63) + 1))
            continue
        slot = sum(value * weight for value, weight in
                   zip(pixel, (3, 5, 7, 11))) % 64
        index[slot] = pixel
        pixels.append(pixel[:3])
    assert len(pixels) == width * height
    return Image.frombytes("RGB", (width, height),
                           bytes(value for pixel in pixels for value in pixel))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", type=Path, required=True)
    args = parser.parse_args()
    app = args.upstream.resolve() / "components/mosaic_ui/apps/settings"
    scene_dir = app / "scene"
    scene = load_module("settings_scene_regression", scene_dir / "gen_scene.py")
    icons = load_module("settings_icons_regression",
                        scene_dir / "generate_root_icon_data.py")
    objects = json.loads((scene_dir / "settings_480.json").read_text())["objects"]
    by_name = {obj["name"]: (index, obj) for index, obj in enumerate(objects)
               if obj.get("name")}
    source = (app / "settings_app.c").read_text()

    def const(name):
        return int(re.search(r"#define " + name + r" (\d+)\b", source)[1])

    viewport_index, viewport = by_name["settings_display_viewport"]
    content_index, content = by_name["settings_display_content"]
    assert viewport["clip_children"]
    assert content["parent"] == viewport_index
    assert viewport["y"] == const("SETTINGS_DISPLAY_VIEWPORT_Y")
    assert viewport["h"] == const("SETTINGS_DISPLAY_VIEWPORT_H")
    assert content["h"] == const("SETTINGS_DISPLAY_CONTENT_H")
    assert content["y"] == {"default": 0, "min": viewport["h"] - content["h"],
                            "max": 0}
    brightness = by_name["settings_brightness_input"][1]
    assert brightness["parent"] == content_index
    assert viewport["y"] + brightness["y"] == const("SETTINGS_BRIGHTNESS_TRACK_Y_MIN")
    assert viewport["y"] + brightness["y"] + brightness["h"] == const("SETTINGS_BRIGHTNESS_TRACK_Y_MAX")

    # The option chooser belongs to the fixed page, not the scrolling subtree.
    options_index, options = by_name["settings_display_options"]
    assert options["parent"] == viewport["parent"]
    assert options["parent"] != content_index
    assert options["y"] == viewport["y"] and options["h"] == viewport["h"]
    for index in range(6):
        row = by_name[f"settings_display_option{index}"][1]
        assert row["parent"] == options_index
        assert 0 <= row["y"] and row["y"] + row["h"] <= options["h"]
    assert not any(obj.get("type") == "dropdown" for obj in objects)
    for name in ("rotation", "screen_timeout", "dim_timeout", "poweroff_timeout"):
        control = by_name[f"settings_{name}_selector"][1]
        chevrons = [obj for obj in objects if obj.get("type") == "shape"
                    and obj.get("parent") == control["parent"]
                    and obj.get("y") == control["y"] + 20]
        assert len(chevrons) == 2
        assert all(control["x"] < obj["x"] and
                   obj["x"] + obj["w"] < control["x"] + control["w"] and
                   obj["y"] + obj["h"] < control["y"] + control["h"]
                   for obj in chevrons)
    for name in ("dim_while_charging", "sleep_while_charging"):
        row = by_name[f"settings_{name}"][1]
        assert row["parent"] == content_index
        at_end = viewport["y"] + row["y"] + content["y"]["min"]
        assert viewport["y"] <= at_end
        assert at_end + row["h"] <= viewport["y"] + viewport["h"]

    password_page = by_name[f"settings_stack_page{scene.PAGE_WLAN_PASSWORD}"][0]
    password_objects = [obj for obj in objects if obj.get("parent") == password_page]
    assert not any("bluetooth" in json.dumps(obj).lower() or
                   "wlan_phone_setup_open" in json.dumps(obj)
                   for obj in password_objects)
    ble = by_name["settings_bluetooth_enabled"][1]
    assert ble["parent"] == by_name[f"settings_stack_page{scene.PAGE_BLUETOOTH}"][0]
    assert ble["events"][0]["target_name"] == "settings_bluetooth_toggle"
    assert 'SETTINGS_ROOT_ROW("Corallium", corallium)' in source
    assert 'SETTINGS_ROOT_ROW("Bluetooth", bluetooth)' in source

    header = (app / "settings_root_icon_data.h").read_text()
    tiles = {}
    for name, encoded in re.findall(
            r"settings_root_icon_(\w+)\[\] = \{(.*?)\};", header, re.S):
        data = bytes(int(byte, 16) for byte in re.findall(r"0x([0-9a-f]{2})", encoded))
        tile = decode_qoi(data)
        assert tile.size == (48, 48)
        bounds = ImageChops.difference(tile, Image.new("RGB", tile.size)).getbbox()
        assert bounds and min(bounds[:2]) >= 7
        assert max(bounds[2:]) <= 41, (name, bounds)
        tiles[name] = tile
    assert set(tiles) == set(icons.ICONS)
    assert ImageChops.difference(tiles["corallium"], tiles["network"]).getbbox()
    protocol = icons.draw_protocol_icon()
    assert all(red == green == blue for red, green, blue, alpha in
               getattr(protocol, "get_flattened_data", protocol.getdata)()
               if alpha)
    # Opaque black cutouts in vendor PNGs must remain empty after tinting.
    about_mask = icons.padded_tint_icon(Image.open(scene_dir / "settings_root_about.png"))
    alpha = about_mask.getchannel("A")
    bounds = alpha.getbbox()
    center = ((bounds[0] + bounds[2]) // 2, (bounds[1] + bounds[3]) // 2)
    assert alpha.getpixel(center) < 255, "About icon lost its information cutout"
    assert by_name["settings_root_row_icon"][1]["fit"] == "contain"

    # Back and WLAN-state icons must also fit their authored image slots.
    checked = 0
    for obj in objects:
        if obj.get("type") != "image" or obj.get("parent", -1) < 0:
            continue
        parent = objects[obj["parent"]]
        if parent.get("name") == "resource_seed":
            continue
        if isinstance(obj.get("x"), int) and isinstance(obj.get("y"), int):
            assert obj["x"] >= 0 and obj["y"] >= 0
            assert obj["x"] + obj["w"] <= parent["w"], obj
            assert obj["y"] + obj["h"] <= parent["h"], obj
            checked += 1
    font = ImageFont.truetype(str(scene_dir / "settings_font.ttf"), 20)
    for label in ("10 sec", "30 sec", "15 min", "1 hour", "Never", "270°"):
        assert font.getlength(label) <= 112
    print(f"PASS: {len(tiles)} safe icon tiles; {checked} icon slots; "
          "Display scroll/chooser geometry; independent Bluetooth route")


if __name__ == "__main__":
    main()

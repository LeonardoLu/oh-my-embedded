#!/usr/bin/env python3
"""Render real Settings C binders and setters through ESP-GSP's native bridge.

Requires the pinned ESP-GSP component, GSPC, CMake and the Python gsp simulator.
Only the row background and initial page are changed in presentation fixtures.
Hardware providers, StackView navigation and common shell APIs are stubbed;
List materialization, image upload/decoding, pointer scroll, chooser actions
and state-property rendering are real.
Lower-page pointer samples and chooser CALLs are injected at the registered
observer/callback boundary; this does not test GSP hit routing or hardware input.
The temporary C copy replaces the host-only constant-false Bluetooth provider;
the Bluetooth render function and all other UI logic stay byte-for-byte intact.
"""

import argparse
import copy
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

from PIL import Image, ImageChops

from test_settings_scene import decode_qoi


def run(command, log, env=None):
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, env=env, timeout=120)
    log.write_text(result.stdout)
    if result.returncode:
        raise RuntimeError(f"Command failed; see {log}\n{result.stdout[-3000:]}")
    return result.stdout


def grb_members(path):
    data = path.read_bytes()
    assert data[:4] == b"GSPB"
    members = []
    for index in range(struct.unpack_from("<H", data, 16)[0]):
        kind, _, offset, size, asset, _, _ = struct.unpack_from(
            "<HHIIIII", data, 32 + 24 * index)
        assert offset + size <= len(data)
        if kind == 2:
            members.append((asset, data[offset:offset + size]))
    assert members, "No compiled image resource bank"
    return members


def absolute_assets(node, scene_dir):
    if isinstance(node, dict):
        return {key: absolute_assets(value, scene_dir) for key, value in node.items()}
    if isinstance(node, list):
        return [absolute_assets(value, scene_dir) for value in node]
    if isinstance(node, str) and node.endswith((".png", ".svg", ".ttf")):
        return str((scene_dir / node).resolve())
    return node


def coordinate(objects, index):
    x = y = 0
    while index >= 0:
        obj = objects[index]
        value = lambda key: obj[key].get("default", 0) if isinstance(obj.get(key), dict) else obj.get(key, 0)
        # Authored StackView pages sit beside each other; the active page is
        # translated into the stack's viewport by the renderer.
        if not obj.get("name", "").startswith("settings_stack_page"):
            x += value("x")
            y += value("y")
        index = obj.get("parent", -1)
    return x, y


def connected_components(mask):
    remaining = {(x, y) for y in range(mask.height) for x in range(mask.width)
                 if mask.getpixel((x, y))}
    sizes = []
    while remaining:
        queue = [remaining.pop()]
        size = 0
        while queue:
            x, y = queue.pop()
            size += 1
            for point in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if point in remaining:
                    remaining.remove(point)
                    queue.append(point)
        sizes.append(size)
    return sizes


def check(args, output):
    upstream = args.upstream.resolve()
    component = (args.component or upstream / "managed_components/espressif__esp-gsp").resolve()
    fixture = Path(__file__).resolve().parent / "settings_native"
    ui = upstream / "components/mosaic_ui"
    scene_dir = ui / "apps/settings/scene"
    authored = json.loads((scene_dir / "settings_480.json").read_text())
    objects = authored["objects"]
    by_name = {obj["name"]: (index, obj) for index, obj in enumerate(objects)
               if obj.get("name")}
    base = absolute_assets(authored, scene_dir)
    root = copy.deepcopy(base)
    for obj in root["objects"]:
        if obj.get("template") == "settings_root_row":
            obj["bg_color"] = "#29292B"
    root_json = output / "root-gray.json"
    root_json.write_text(json.dumps(root))
    source = (ui / "apps/settings/settings_app.c").read_text()
    provider = re.search(r"static bool settings_bluetooth_enabled\(void\)\n\{.*?\n\}",
                         source, re.S)
    assert provider and provider[0].count("return false;") == 1
    injected = provider[0].replace("return false;",
                                   "return settings_native_bluetooth_enabled();")
    native_source = output / "settings_app_native.c"
    native_source.write_text(source[:provider.start()] + injected + source[provider.end():])
    build = output / "build"
    run(["cmake", "-S", str(fixture), "-B", str(build),
         "-DCMAKE_BUILD_TYPE=Release", f"-DSETTINGS_UPSTREAM={upstream}",
         f"-DESP_GSP_COMPONENT_DIR={component}", f"-DGSPC_EXECUTABLE={args.gspc}",
         f"-DESP_GSP_PYTHON_EXECUTABLE={sys.executable}",
         f"-DSETTINGS_NATIVE_APP_SOURCE={native_source}",
         f"-DSETTINGS_NATIVE_SCENE={root_json}"], output / "configure.log")
    run(["cmake", "--build", str(build), "--target", "settings_native",
         "settings_native_module", "--parallel"], output / "build.log")
    manifest = json.loads((build / "settings_native-Release.json").read_text())
    bundle = Path(manifest["bundle"])
    generated = bundle.parent / "scene0"
    execution = json.loads((generated / "settings.execution.json").read_text())
    placeholder = next(item for item in execution["resources"]
                       if item.get("source") == "settings_root_placeholder.png")
    assert placeholder["pixel_format"] == "rgb565_a8", placeholder
    templates = (generated / "settings_templates.h").read_text()
    counts = [int(value) for value in re.findall(r"#define \w+_MAX_INSTANCES (\d+)", templates)]
    dynamic = int(re.search(r"#define GSP_SETTINGS_DYNAMIC_IMAGE_SLOTS (\d+)",
                           (generated / "settings_objects.h").read_text())[1])
    assert len(counts) == 3

    def pack(name, page):
        variant = copy.deepcopy(base)
        next(obj for obj in variant["objects"] if obj.get("name") == "settings_stack")["initial_page"] = page
        path = output / f"{name}.json"
        path.write_text(json.dumps(variant))
        result = output / f"{name}.gspb"
        run([args.gspc, "pack", str(path), "--deployable", "--profile",
             str(ui / "common/mosaic_rgb565_auto.yaml"), "--gen-dir",
             str(output / f"{name}-assets"), "-o", str(result)], output / f"{name}-pack.log")
        return result

    display = pack("display", 1)
    bluetooth = pack("bluetooth", 13)
    linked = output / "linked"
    catalog = linked / "common-fonts.gspb"
    run([args.gspc, "font-link", str(bundle), str(display), "--output-dir",
         str(linked), "--catalog", str(catalog)], output / "font-link.log")
    assert grb_members(bundle) == grb_members(linked / bundle.name), "Font-link changed the A8 image bank"
    materialized = output / "materialized"
    run([args.gspc, "font-link", str(linked / bundle.name), "--materialize",
         str(catalog), "--output-dir", str(materialized)], output / "materialize.log")

    def capture(name, scene_bundle, overrides=None, scroll=False, script=None, page=None):
        env = dict(os.environ)
        for key in ("SETTINGS_NATIVE_SCROLL", "SETTINGS_NATIVE_CHECKED", "SETTINGS_NATIVE_PAGE",
                    "SETTINGS_NATIVE_DISPLAY_INPUT"):
            env.pop(key, None)
        env.update(overrides or {})
        if page is not None:
            env["SETTINGS_NATIVE_PAGE"] = str(page)
        if script is None:
            script = (["--wait", "30", "--drag", "240", "420", "240", "130",
                       "--wait", "60"] if scroll else [])
        picture = output / f"{name}.png"
        log = run([sys.executable, "-m", "gsp.execute", "--version", args.sim_version,
                   "sim", "--bundle", str(scene_bundle), "--backend-library",
                   manifest["backend_library"], "--backend-required", "--headless",
                   "--frames", "300" if "SETTINGS_NATIVE_DISPLAY_INPUT" in env else "150",
                   "--instance-slots", str(sum(counts)),
                   "--dynamic-image-slots", str(counts[0] + dynamic), "--dump",
                   str(picture), "--dump-format", "png"] + script,
                  output / f"{name}.log", env)
        assert "row binder returned" not in log, log
        if overrides and "SETTINGS_NATIVE_CHECKED" in overrides:
            expected = "On - ready to connect" if overrides["SETTINGS_NATIVE_CHECKED"] == "1" else "Off"
            assert f"native Bluetooth status={expected}\n" in log, log
        image = Image.open(picture).convert("RGB")
        # Standardize the PNG container for preview decoders, preserving every
        # native RGB pixel used by the assertions below.
        image.save(picture)
        return image

    header = (ui / "apps/settings/settings_root_icon_data.h").read_text()
    tiles = {name: decode_qoi(bytes(int(byte, 16) for byte in re.findall(r"0x([0-9a-f]{2})", encoded)))
             for name, encoded in re.findall(r"settings_root_icon_(\w+)\[\] = \{(.*?)\};", header, re.S)}
    icon = by_name["settings_root_row_icon"][1]
    _, root_list = by_name["settings_root_list"]
    x, y = icon["x"], root_list["y"] + icon["y"]
    row_height = objects[icon["parent"]]["h"]

    def check_rows(picture, names):
        background = picture.getpixel((x - 2, y + 20))
        for index, name in enumerate(names):
            at_y = y + row_height * index
            actual = picture.crop((x, at_y, x + 48, at_y + 48))
            expected = Image.alpha_composite(Image.new("RGBA", (48, 48),
                (*background, 255)), tiles[name]).convert("RGB")
            extrema = ImageChops.difference(actual, expected).getextrema()
            assert max(maximum for _, maximum in extrema) <= 6, (name, extrema)

    first = ["network", "bluetooth", "display", "battery"]
    check_rows(capture("root-gray", bundle), first)
    check_rows(capture("root-gray-scrolled", bundle, scroll=True),
               ["display", "battery", "corallium", "about"])
    check_rows(capture("root-font-linked", materialized / bundle.name), first)

    index, toggle = by_name["settings_bluetooth_enabled"]
    tx, ty = coordinate(objects, index)
    for checked in (False, True):
        picture = capture(f"bluetooth-{'on' if checked else 'off'}", bluetooth,
                          {"SETTINGS_NATIVE_CHECKED": str(int(checked))})
        red, green, blue = picture.getpixel((tx + toggle["w"] // 2, ty + toggle["h"] // 2))
        if checked:
            assert red > 200 and 50 < green < 140 and blue < 40, (red, green, blue)
        else:
            assert max(red, green, blue) - min(red, green, blue) < 12 and 30 < red < 100
    def check_chevron(picture, name, offset=0):
        index, chevron = by_name[f"settings_{name}_chevron"]
        cx, cy = coordinate(objects, index)
        cy -= offset
        viewport = by_name["settings_display_viewport"][1]
        assert viewport["y"] <= cy and cy + chevron["h"] <= viewport["y"] + viewport["h"]
        tile = picture.crop((cx, cy, cx + chevron["w"], cy + chevron["h"]))
        mask = tile.convert("L").point(lambda value: 255 if value >= 200 else 0)
        bounds = mask.getbbox()
        assert bounds and min(bounds[:2]) >= 4 and max(bounds[2:]) <= 28, (name, bounds)
        assert len(connected_components(mask)) == 1, f"{name}: chevron has a clipped or broken join"

    picture = capture("display", display, page=1)
    for name in ("rotation", "screen_timeout", "dim_timeout"):
        check_chevron(picture, name)

    lower = capture("display-lower", display,
                    overrides={"SETTINGS_NATIVE_DISPLAY_INPUT": "lower"}, page=1)
    lower_log = (output / "display-lower.log").read_text()
    offsets = re.findall(r"native Display scroll=(\d+)\n", lower_log)
    assert offsets, "The actual Settings pointer path did not scroll"
    offset = int(offsets[-1])
    assert offset == -by_name["settings_display_content"][1]["y"]["min"], offset
    check_chevron(lower, "poweroff_timeout", offset)
    for name in ("dim_while_charging", "sleep_while_charging"):
        index, toggle = by_name[f"settings_{name}"]
        tx, ty = coordinate(objects, index)
        ty -= offset
        viewport = by_name["settings_display_viewport"][1]
        assert viewport["y"] <= ty and ty + toggle["h"] <= viewport["y"] + viewport["h"]
        tile = lower.crop((tx, ty, tx + toggle["w"], ty + toggle["h"]))
        white = tile.convert("L").point(lambda value: 255 if value >= 200 else 0)
        bounds = white.getbbox()
        assert bounds and 0 < bounds[0] < bounds[2] < toggle["w"], (name, bounds)
        assert 0 < bounds[1] < bounds[3] < toggle["h"], (name, bounds)
        red, green, blue = tile.getpixel((toggle["w"] // 2, toggle["h"] // 2))
        assert max(red, green, blue) - min(red, green, blue) < 12 and 30 < red < 100
        label = lower.crop((12, ty, 332, ty + toggle["h"]))
        assert label.convert("L").point(lambda value: 255 if value >= 200 else 0).getbbox(), name

    chooser = capture("display-poweroff-chooser", display,
                      overrides={"SETTINGS_NATIVE_DISPLAY_INPUT": "chooser"}, page=1)
    assert "native chooser open=1 kind=3\n" in (output / "display-poweroff-chooser.log").read_text()
    assert ImageChops.difference(lower, chooser).crop((24, 70, 456, 452)).getbbox()
    returned = capture("display-poweroff-selected", display,
                       overrides={"SETTINGS_NATIVE_DISPLAY_INPUT": "selected"}, page=1)
    returned_log = (output / "display-poweroff-selected.log").read_text()
    assert "native Power-off saved=300000\n" in returned_log
    assert f"native chooser open=0 value=300000 scroll={offset}\n" in returned_log
    assert "native Power-off label=5 min\n" in returned_log
    check_chevron(returned, "poweroff_timeout", offset)
    # Closing the fixed chooser restores every lower-page pixel except the
    # selected value text. This catches an overlay left on top of the controls.
    before = lower.copy()
    after = returned.copy()
    value_index, value = by_name["settings_poweroff_timeout_value"]
    vx, vy = coordinate(objects, value_index)
    box = (vx, vy - offset, vx + value["w"], vy - offset + value["h"])
    before.paste((0, 0, 0), box)
    after.paste((0, 0, 0), box)
    assert not ImageChops.difference(before, after).getbbox()
    print("PASS: real C binder RGBA upload, initial/recycled/refreshed rows, "
          "font-linked A8 bank, runtime Bluetooth text/colors, all 4 native SVG "
          "chevrons, lower charge toggles and real C Power-off chooser/save/return "
          "(observer/callback input doubles)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", type=Path, required=True)
    parser.add_argument("--gspc", required=True)
    parser.add_argument("--component", type=Path)
    parser.add_argument("--sim-version", default="1.6.0")
    parser.add_argument("--output", type=Path, help="Keep native logs and PNG evidence here")
    args = parser.parse_args()
    if args.output:
        output = args.output.resolve()
        output.mkdir(parents=True, exist_ok=True)
        check(args, output)
    else:
        with tempfile.TemporaryDirectory(prefix="settings-native-") as directory:
            check(args, Path(directory))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Portable native tests with sanitizers and optional prepared-upstream checks."""
import argparse
import json
import os
import re
from pathlib import Path
import subprocess
import sys

PROJECT = Path(__file__).resolve().parents[1]
REPO = PROJECT.parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--upstream", type=Path)
args = parser.parse_args()
output = REPO / "tmp/mosaico/host-tests"
output.mkdir(parents=True, exist_ok=True)
include = PROJECT / "overlay/components/corallium/include"
compiler = os.environ.get("CC", "clang")
flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined"]
def run(command): subprocess.run([str(x) for x in command], check=True)
run([compiler, *flags, "-I", include, PROJECT / "tests/test_core.c", "-o", output / "test-core"])
run([output / "test-core"])
run([compiler, *flags, "-I", PROJECT / "tests/audio_stubs", "-I", PROJECT / "overlay/components/mosaico_audio/include",
     PROJECT / "tests/test_audio.c", PROJECT / "overlay/components/mosaico_audio/mosaico_audio.c", "-o", output / "test-audio"])
run([output / "test-audio"])
if args.upstream:
    upstream = args.upstream.resolve()
    cjson = upstream / "managed_components/espressif__cjson/cJSON"
    if not (cjson / "cJSON.c").is_file():
        raise SystemExit("Run firmware dependency configuration first; cJSON test source is unavailable.")
    run([compiler, "-std=c11", "-Wno-deprecated-declarations", "-fsanitize=address,undefined", "-c", cjson / "cJSON.c", "-o", output / "cjson.o"])
    run([compiler, *flags, "-I", include, "-I", PROJECT / "tests/stubs", "-I", cjson,
         PROJECT / "tests/test_protocol.c", PROJECT / "overlay/components/corallium/corallium_protocol.c", output / "cjson.o", "-o", output / "test-protocol"])
    run([output / "test-protocol"])
    run([sys.executable, upstream / "components/mosaic_ui/hub/scene/gen_scenes.py"])
    scene = json.loads((upstream / "components/mosaic_ui/hub/scene/mosaic_hub_480.json").read_text())
    objects = scene["objects"]
    forbidden = {"app_camera", "app_ai_create", "app_works", "app_music", "app_interact", "quick_join_toggle", "quick_low_power_toggle"}
    def hidden(index):
        seen = set()
        while 0 <= index < len(objects):
            assert index not in seen, "scene parent cycle"
            seen.add(index)
            obj = objects[index]
            if obj.get("hidden"): return True
            index = obj.get("parent", -1)
        return False
    for index, obj in enumerate(objects):
        if obj.get("callback") in forbidden:
            assert hidden(index), f"Unsupported visible control: {obj}"
        if not hidden(index):
            assert "Corallium" not in str(obj.get("text", "")), "Protocol branding leaked into daily UI"
    assert next(o for o in objects if o.get("name") == "launcher_flow")["page_count"] == 2
    assert not any(o.get("name", "").startswith("clock_shortcut_") for o in objects), "Duplicate home shortcuts remain"
    assert "callback" not in next(o for o in objects if o.get("name") == "clock_card")
    for name in ("app_weather", "app_settings", "app_imu", "app_album", "app_breakout"):
        assert sum(o.get("name") == name for o in objects) == 1, f"Launcher entry duplicated/missing: {name}"
    assert any(o.get("bind") == "quick_ble_state" for o in objects), "BLE state label missing"
    def position(obj):
        x, y = obj.get("x", 0), obj.get("y", 0)
        parent = obj.get("parent", -1)
        while 0 <= parent < len(objects):
            ancestor = objects[parent]
            x += ancestor.get("x", 0); y += ancestor.get("y", 0)
            if ancestor.get("name") == "quick_drawer": break
            parent = ancestor.get("parent", -1)
        return x, y
    for name, xy in {"quick_wlan": (34, 62), "quick_bluetooth": (266, 62),
                     "quick_ringtone": (34, 154), "quick_vibration": (266, 154),
                     "quick_volume_input": (34, 280), "quick_brightness_input": (266, 280)}.items():
        index, obj = next((i, o) for i, o in enumerate(objects) if o.get("name") == name)
        assert not hidden(index), f"Required control hidden: {name}"
        assert position(obj) == xy, f"Control and pointer hit test disagree: {name} {position(obj)}"
    from PIL import ImageFont
    import importlib.util
    font_path = upstream / "components/mosaic_ui/common/font_paths.py"
    spec = importlib.util.spec_from_file_location("mosaic_font_paths", font_path)
    font_paths = importlib.util.module_from_spec(spec); spec.loader.exec_module(font_paths)
    ble_font = ImageFont.truetype(str(font_paths.DEJAVU_SANS), 19)
    assert all(ble_font.getlength(text) <= 106 for text in ("Bluetooth", "On", "Off", "Connected"))
    print("Factory scene: removed actions hidden; two-column controls and volume/brightness hit targets match")
    run([sys.executable, upstream / "components/mosaic_ui/apps/settings/scene/gen_scene.py"])
    settings = json.loads((upstream / "components/mosaic_ui/apps/settings/scene/settings_480.json").read_text())
    for obj in settings["objects"]:
        encoded = json.dumps(obj).lower()
        assert "settings_update" not in encoded, "Update page/action remains in Settings scene"
        assert "software update" not in encoded, "Update entry remains in Settings scene"
    source = (upstream / "components/mosaic_ui/apps/settings/settings_app.c").read_text()
    about_rows = source.split("s_about_rows[] = {", 1)[1].split("};", 1)[0]
    assert "Corallium" not in about_rows
    assert 'SETTINGS_ROOT_ROW("Corallium", network)' in source and "settings_open_detail(ui, SETTINGS_DETAIL_PROTOCOL)" in source
    assert '"Disch. Pwr"' in source and '"Runtime"' in source and '"Discharge power"' not in source
    from PIL import ImageFont
    font = ImageFont.truetype(str(upstream / "components/mosaic_ui/apps/settings/scene/settings_font.ttf"), 32)
    for array in ("s_about_rows", "s_protocol_rows", "s_battery_rows"):
        labels = re.findall(r'\{"([^"]+)"', source.split(array + "[] = {", 1)[1].split("};", 1)[0])
        assert all(font.getlength(label) <= 180 for label in labels), f"{array} label exceeds its actual column"
    print("Settings scene: updater absent; dedicated protocol route; shortened battery labels fit actual font")
else:
    print("Protocol/scene checks require --upstream with configured factory checkout.")

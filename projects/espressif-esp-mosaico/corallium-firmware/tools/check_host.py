#!/usr/bin/env python3
"""Portable native tests with sanitizers and optional prepared-upstream checks."""
import argparse
import json
import os
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
    forbidden = {"app_camera", "app_ai_create", "app_works", "app_music", "app_interact", "quick_join_toggle", "quick_low_power_toggle", "quick_ringtone_toggle"}
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
    assert any(o.get("bind") == "quick_ble_state" for o in objects), "BLE state label missing"
    print("Factory scene: removed actions have no visible/touchable route")
    run([sys.executable, upstream / "components/mosaic_ui/apps/settings/scene/gen_scene.py"])
    settings = json.loads((upstream / "components/mosaic_ui/apps/settings/scene/settings_480.json").read_text())
    for obj in settings["objects"]:
        encoded = json.dumps(obj).lower()
        assert "settings_update" not in encoded, "Update page/action remains in Settings scene"
        assert "software update" not in encoded, "Update entry remains in Settings scene"
    print("Settings scene: update page, bindings and actions are absent")
else:
    print("Protocol/scene checks require --upstream with configured factory checkout.")

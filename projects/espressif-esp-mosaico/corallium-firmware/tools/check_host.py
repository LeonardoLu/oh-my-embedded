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
run([compiler, *flags, "-I", PROJECT / "overlay/components/mosaic_ui/common",
     PROJECT / "tests/test_weather_art.c", "-o", output / "test-weather-art"])
run([output / "test-weather-art"])
run([sys.executable, PROJECT / "tests/test_scene_resource_refs.py"])
run([sys.executable, PROJECT / "tests/test_device_diagnostics.py"])
run([sys.executable, PROJECT / "tests/check_diagnostics.py"])
if args.upstream:
    upstream = args.upstream.resolve()
    run([sys.executable, PROJECT / "tests/test_ui_diagnostics.py", "--upstream", upstream])
    run([sys.executable, PROJECT / "tests/check_diagnostics_open.py", upstream])
    cjson = upstream / "managed_components/espressif__cjson/cJSON"
    if not (cjson / "cJSON.c").is_file():
        raise SystemExit("Run firmware dependency configuration first; cJSON test source is unavailable.")
    run([compiler, "-std=c11", "-Wno-deprecated-declarations", "-fsanitize=address,undefined", "-c", cjson / "cJSON.c", "-o", output / "cjson.o"])
    run([compiler, *flags, "-I", include, "-I", PROJECT / "tests/stubs", "-I", cjson,
         PROJECT / "tests/test_protocol.c", PROJECT / "overlay/components/corallium/corallium_protocol.c", output / "cjson.o", "-o", output / "test-protocol"])
    run([output / "test-protocol"])
    run([sys.executable, PROJECT / "tests/check_works_audio.py", "--upstream", upstream])
    run([sys.executable, PROJECT / "tests/check_boot_commands.py", upstream])
    run([sys.executable, PROJECT / "tests/check_display_back.py", "--upstream", upstream])
    run([compiler, *flags, "-I", PROJECT / "tests/power_stubs",
         "-I", upstream / "components/app_system_config/include",
         "-I", upstream / "components/app_settings_service/include",
         "-I", upstream / "components/app_config/include",
         "-I", PROJECT / "overlay/components/mosaico_audio/include",
         "-I", upstream / "components/mosaic_ui/include",
         "-I", upstream / "third-party/esp-claw/components/common/settings/include",
         PROJECT / "tests/test_display_idle.c",
         upstream / "components/app_system_config/app_system_config.c",
         upstream / "components/app_settings_service/app_settings_service.c",
         "-o", output / "test-display-idle"])
    run([output / "test-display-idle"])
    run([sys.executable, upstream / "components/mosaic_ui/hub/scene/gen_scenes.py"])
    scene = json.loads((upstream / "components/mosaic_ui/hub/scene/mosaic_hub_480.json").read_text())
    objects = scene["objects"]
    forbidden = {"app_camera", "app_ai_create", "app_music", "app_interact", "quick_join_toggle", "quick_low_power_toggle"}
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
    for icon, app in (("settings", "settings"), ("skills", "works"), ("album", "album")):
        shortcut = next(o for o in objects if o.get("name") == "clock_shortcut_" + icon)
        assert shortcut.get("callback") == "app_" + app, f"Home shortcut route missing: {icon}"
    assert next(o for o in objects if o.get("name") == "clock_card").get("callback") == "app_weather"
    for name in ("app_weather", "app_settings", "app_imu", "app_album", "app_breakout", "app_works"):
        assert sum(o.get("name") == name for o in objects) == 1, f"Launcher entry duplicated/missing: {name}"
    group = next(i for i, o in enumerate(objects) if o.get("name") == "quick_connectivity_group")
    assert not any(o.get("parent") == group and o.get("type") == "label" for o in objects), "Quick buttons must remain icon-only"
    assert not any(o.get("bind") in ("quick_ble_state", "quick_mute_state") for o in objects)
    def position(obj):
        x, y = obj.get("x", 0), obj.get("y", 0)
        parent = obj.get("parent", -1)
        while 0 <= parent < len(objects):
            ancestor = objects[parent]
            x += ancestor.get("x", 0); y += ancestor.get("y", 0)
            if ancestor.get("name") == "quick_drawer": break
            parent = ancestor.get("parent", -1)
        return x, y
    for name, xy in {"quick_wlan": (34, 76), "quick_bluetooth": (138, 76),
                     "quick_ringtone": (34, 180), "quick_vibration": (138, 180),
                     "quick_volume_input": (270, 70), "quick_brightness_input": (374, 70)}.items():
        index, obj = next((i, o) for i, o in enumerate(objects) if o.get("name") == name)
        assert not hidden(index), f"Required control hidden: {name}"
        assert position(obj) == xy, f"Control and pointer hit test disagree: {name} {position(obj)}"
    for name in ("quick_wlan", "quick_bluetooth", "quick_ringtone", "quick_vibration"):
        tile = next(o for o in objects if o.get("name") == name)
        for state in ("off", "on"):
            icon = next(o for o in objects if o.get("name") == name + "_" + state)
            assert abs((icon["x"] * 2 + icon["w"]) - (tile["x"] * 2 + tile["w"])) <= 1
            assert abs((icon["y"] * 2 + icon["h"]) - (tile["y"] * 2 + tile["h"])) <= 1
    badge_index, badge = next((i, o) for i, o in enumerate(objects) if o.get("name") == "quick_ble_badge")
    count = next(o for o in objects if o.get("name") == "quick_ble_badge_count")
    assert badge.get("hidden") and badge.get("bind_target") == "visible"
    assert badge.get("bind") == "quick_ble_badge_visible" and count.get("text") == "1"
    assert count.get("parent") == badge_index and position(badge) == (188, 126)
    assert all(not o.get("events") and not o.get("callback") for o in (badge, count)), "BLE badge must not capture touch"
    controls = objects[group]
    assert controls["w"] == controls["h"] == 220
    assert controls["w"] * controls["h"] <= 480 * 480 / 4
    assert not any(o.get("name", "").startswith("home_clock") for o in objects)
    print("Factory scene: Settings/Works/Album Home, compact icons, hit regions and one-peer badge verified")
    run([sys.executable, upstream / "components/mosaic_ui/apps/settings/scene/gen_scene.py"])
    settings = json.loads((upstream / "components/mosaic_ui/apps/settings/scene/settings_480.json").read_text())
    for obj in settings["objects"]:
        encoded = json.dumps(obj).lower()
        assert "settings_update" not in encoded, "Update page/action remains in Settings scene"
        assert "software update" not in encoded, "Update entry remains in Settings scene"
    source = (upstream / "components/mosaic_ui/apps/settings/settings_app.c").read_text()
    about_rows = source.split("s_about_rows[] = {", 1)[1].split("};", 1)[0]
    assert "Corallium" not in about_rows
    assert 'SETTINGS_ROOT_ROW("Corallium", corallium)' in source and "settings_open_detail(ui, SETTINGS_DETAIL_PROTOCOL)" in source
    assert '"Disch. Pwr"' in source and '"Runtime"' in source and '"Discharge power"' not in source
    from PIL import ImageFont
    font = ImageFont.truetype(str(upstream / "components/mosaic_ui/apps/settings/scene/settings_font.ttf"), 32)
    for array in ("s_about_rows", "s_protocol_rows", "s_battery_rows"):
        labels = re.findall(r'\{"([^"]+)"', source.split(array + "[] = {", 1)[1].split("};", 1)[0])
        assert all(font.getlength(label) <= 180 for label in labels), f"{array} label exceeds its actual column"
    run([sys.executable, PROJECT / "tests/test_settings_scene.py", "--upstream", upstream])
    from gsp.execute import executable_from_environment
    version = (upstream / "managed_components/espressif__esp-gsp/.gspc_version").read_text().strip()
    gspc = executable_from_environment("gspc", version=version)
    run([sys.executable, PROJECT / "tests/test_settings_native.py", "--upstream", upstream,
         "--gspc", gspc, "--output", output / "settings-native"])
    run([sys.executable, PROJECT / "tests/test_hub_drawer.py", "--upstream", upstream,
         "--gspc", gspc, "--output", output / "hub-drawer"])
    run([sys.executable, upstream / "components/mosaic_ui/apps/weather/scene/gen_scene.py"])
    run([sys.executable, PROJECT / "tests/test_weather_scene.py", "--upstream", upstream])
    print("Settings scene: updater absent; separate protocol/Bluetooth routes; battery labels fit actual font")
else:
    print("Protocol and scene checks require --upstream with configured factory checkout.")

#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent.parent.parent / "common"))
from font_paths import DEJAVU_SANS, DEJAVU_SANS_BOLD  # noqa: E402
from scene_common import (  # noqa: E402
    app_root_shell, asset_scene, button, container, explicit_charset, image,
    label, layer, scene_out_path, shared_charset, shared_prefix, write_scene,
)

FONT_POLICIES = {
    16: shared_charset(),
    18: shared_charset(),
    36: explicit_charset("-+.0123456789°"),
}


def visible_layer(parent, name, *, hidden=False):
    obj = layer(parent, 0, 0, 480, 480, name=name, hidden=hidden)
    obj.update(bind=name + "_visible", bind_target="visible")
    return obj


def main():
    objs, content = shared_prefix([], FONT_POLICIES, DEJAVU_SANS_BOLD)
    page = len(objs)
    objs.append(layer(content, 0, 0, 480, 480, name="imu_root"))
    app_root_shell(objs, page, "IMU", name="shell_imu")
    level = len(objs)
    objs.append(visible_layer(page, "imu_level"))
    objs.append(container(level, 12, 74, 456, 232, bg="#F9FAFB", radius=43,
                          name="imu_horizon"))
    objs.append(container(level, 12, 189, 456, 2, bg="#000000"))
    objs.append(container(level, 239, 74, 2, 232, bg="#000000"))
    bubble = len(objs)
    objs.append({
        "type": "container", "parent": level,
        "x": {"default": 202, "min": 130, "max": 270},
        "y": {"default": 160, "min": 90, "max": 230},
        "w": 76, "h": 76, "bg_color": "#FFB020", "radius": 38,
        "name": "imu_bubble",
    })
    objs.append(label(bubble, 0, 14, 76, 48, "0°", size=36, align="center",
                      bind="imu_angle", name="imu_angle",
                      font_charset="-+.0123456789°"))
    for x, text, bind in ((12, "Pitch", "imu_pitch"), (170, "Roll", "imu_roll"),
                           (327, "Yaw", "imu_yaw")):
        card = len(objs)
        objs.append(container(level, x, 320, 141, 78, bg="#181819", radius=20,
                              border="#3B3C3D", border_w=1))
        objs.append(label(card, 0, 8, 141, 22, text, size=16, color="#91919B",
                          align="center"))
        objs.append(label(card, 0, 30, 141, 44, "0", size=36, align="center",
                          bind=bind, name=bind, font_charset="-+.0123456789°"))
    write_scene(scene_out_path(HERE, "imu_480.json"), "imu", objs, font=DEJAVU_SANS)
    asset_scene(scene_out_path(HERE, "imu_assets_480.json"), "imu_assets", [],
                FONT_POLICIES, DEJAVU_SANS_BOLD, DEJAVU_SANS)


if __name__ == "__main__":
    main()

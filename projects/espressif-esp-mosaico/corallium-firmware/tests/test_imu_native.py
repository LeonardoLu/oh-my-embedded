#!/usr/bin/env python3
"""Real IMU C app, GSP hit routing and Canvas rendering; sensor inputs are fixtures."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from PIL import Image

PROJECT = Path(__file__).resolve().parents[1]
BRIGHT = [0x00FF00, 0x00FFFF, 0xFF8800, 0xDDDDDD,
          0xFF0000, 0x00CCFF, 0x39FF14, 0xD6AD00]
BACKGROUND = [0x000500, 0x050005, 0x0C0500, 0x080808,
              0x0A0000, 0x000511, 0x08000C, 0x0D0B00]


def run(command, log, env=None):
    result = subprocess.run([str(arg) for arg in command], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            env=env, timeout=60)
    log.write_text(result.stdout)
    if result.returncode:
        raise RuntimeError(f"Command failed; see {log}\n{result.stdout[-2500:]}")
    return result.stdout


def color_near(pixel, rgb):
    expected = (rgb >> 16, (rgb >> 8) & 255, rgb & 255)
    return all(abs(a - b) <= 7 for a, b in zip(pixel, expected))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--upstream", type=Path, required=True)
    parser.add_argument("--gspc", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sim-version", default="1.6.0")
    args = parser.parse_args()
    upstream, output = args.upstream.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    ui = upstream / "components/mosaic_ui"
    component = upstream / "managed_components/espressif__esp-gsp"
    run([sys.executable, ui / "apps/imu/scene/gen_scene.py"], output / "scene.log")
    run(["cmake", "-S", PROJECT / "tests/imu_native", "-B", output / "build",
         "-DCMAKE_BUILD_TYPE=Release", f"-DIMU_UPSTREAM={upstream}",
         f"-DESP_GSP_COMPONENT_DIR={component}", f"-DGSPC_EXECUTABLE={args.gspc}",
         f"-DESP_GSP_PYTHON_EXECUTABLE={sys.executable}"], output / "configure.log")
    run(["cmake", "--build", output / "build", "--target", "imu_native",
         "imu_native_module", "--parallel"], output / "build.log")
    manifest = json.loads((output / "build/imu_native-Release.json").read_text())
    generated = Path(manifest["bundle"]).parent / "scene0"
    resources = json.loads((generated / "imu.execution.json").read_text())["resources"]
    canvas = [item for item in resources if item.get("source") == "imu_liquid_canvas.png"]
    assert len(canvas) == 1 and canvas[0]["pixel_format"] == "rgb565", canvas

    def capture(name, style, palette=5, script=()):
        env = dict(os.environ, IMU_NATIVE_STYLE=str(style), IMU_NATIVE_PALETTE=str(palette))
        picture = output / f"{name}.png"
        log = run([sys.executable, "-m", "gsp.execute", "--version", args.sim_version,
                   "sim", "--bundle", manifest["bundle"], "--backend-library",
                   manifest["backend_library"], "--backend-required", "--headless",
                   "--frames", "120", "--dump", picture, "--dump-format", "png",
                   *script], output / f"{name}.log", env)
        assert "error" not in log.lower() and "unsupported" not in log.lower(), log
        return Image.open(picture).convert("RGB")

    def tank_colors(picture):
        return set(picture.crop((12, 74, 468, 394)).get_flattened_data())

    level = capture("level", 0)
    assert color_near(level.getpixel((20, 180)), 0xF9FAFB)
    sheet = Image.new("RGB", (8 * 240, 3 * 240))
    for style in range(1, 4):
        for palette in range(8):
            picture = capture(f"style-{style}-palette-{palette}", style, palette)
            colors = tank_colors(picture)
            assert any(color_near(pixel, BACKGROUND[palette]) for pixel in colors)
            assert len(colors) >= 2, (style, palette, colors)
            if style == 1:
                assert len(colors) == 2 and any(color_near(pixel, BRIGHT[palette]) for pixel in colors), colors
            else:
                assert len(colors) >= 3, (style, palette, colors)
            sheet.paste(picture.resize((240, 240), Image.Resampling.NEAREST), (palette * 240, (style - 1) * 240))
    sheet.save(output / "styles-palettes.png")
    tap = ("--wait", "5", "--tap", "79", "431", "--wait", "10")
    pixel = capture("style-tap", 0, script=tap)
    assert len(tank_colors(pixel)) == 2 and any(color_near(c, BRIGHT[5]) for c in tank_colors(pixel))
    theme = capture("theme-tap", 1, script=("--wait", "5", "--tap", "248", "431", "--wait", "10"))
    assert any(color_near(c, BRIGHT[6]) for c in tank_colors(theme))
    back_to_level = capture("style-cycle", 0, script=tap * 4)
    assert color_near(back_to_level.getpixel((20, 180)), 0xF9FAFB)
    reset = capture("reset-tap", 3, script=("--wait", "5", "--tap", "408", "431", "--wait", "10"))
    assert len(tank_colors(reset)) >= 3
    print("PASS: real native IMU rendering, 24 style/palette combinations, touch routing, reset, bounded Canvas frames and delayed release")
    print(output / "styles-palettes.png")


if __name__ == "__main__":
    main()

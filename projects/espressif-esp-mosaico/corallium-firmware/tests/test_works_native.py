#!/usr/bin/env python3
"""Render real Works C, native routes and Lua paging through GSP input."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

PROJECT = Path(__file__).resolve().parents[1]


def run(command, log, env=None):
    result = subprocess.run([str(arg) for arg in command], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            env=env, timeout=60)
    log.write_text(result.stdout)
    if result.returncode:
        raise RuntimeError(f"Works native check failed; see {log}\n{result.stdout[-2500:]}")
    return result.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--upstream", type=Path, required=True)
    parser.add_argument("--gspc", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    upstream, output = args.upstream.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    ui = upstream / "components/mosaic_ui"
    run([sys.executable, ui / "apps/works/scene/gen_scene.py"], output / "scene.log")
    run(["cmake", "-S", PROJECT / "tests/works_native", "-B", output / "build",
         "-DCMAKE_BUILD_TYPE=Release", f"-DWORKS_UPSTREAM={upstream}",
         f"-DESP_GSP_COMPONENT_DIR={upstream / 'managed_components/espressif__esp-gsp'}",
         f"-DGSPC_EXECUTABLE={args.gspc}",
         f"-DESP_GSP_PYTHON_EXECUTABLE={sys.executable}"], output / "configure.log")
    run(["cmake", "--build", output / "build", "--target", "works_native",
         "works_native_module", "--parallel"], output / "build.log")
    manifest = json.loads((output / "build/works_native-Release.json").read_text())

    def capture(name, script=(), unavailable=False):
        env = dict(os.environ)
        if unavailable:
            env["WORKS_UNAVAILABLE"] = "1"
        log = run([sys.executable, "-m", "gsp.execute", "--version", "1.6.0", "sim",
                   "--bundle", manifest["bundle"], "--backend-library",
                   manifest["backend_library"], "--backend-required", "--headless",
                   "--frames", "40", "--dump", output / f"{name}.png",
                   "--dump-format", "png", *script], output / f"{name}.log", env)
        assert "error" not in log.lower() and "unsupported" not in log.lower(), log
        return log

    capture("lab")
    for style, y in (("Pixel", 156), ("Gradient", 228), ("Water", 300)):
        log = capture(style.lower(), ("--wait", "5", "--tap", "240", str(y), "--wait", "10"))
        assert f"NATIVE_ROUTE {style}" in log and "LUA_TOGGLE" not in log, log
    log = capture("lab-page-two", ("--wait", "5", "--tap", "314", "435",
                                   "--wait", "5", "--tap", "240", "156", "--wait", "10"))
    assert "LUA_TOGGLE flappybird" in log and "NATIVE_ROUTE" not in log, log
    log = capture("installed", ("--wait", "5", "--tap", "150", "90", "--wait", "5",
                               "--tap", "240", "300", "--wait", "10"))
    assert "NATIVE_ROUTE Water" in log, log
    log = capture("recent", ("--wait", "5", "--tap", "55", "90", "--wait", "5",
                            "--tap", "240", "156", "--wait", "10"))
    assert "LUA_TOGGLE dino" in log and "NATIVE_ROUTE" not in log, log
    log = capture("lua-unavailable", ("--wait", "5", "--tap", "240", "300",
                                     "--wait", "10"), unavailable=True)
    assert "NATIVE_ROUTE Water" in log, log
    print("PASS: real Works native entries, three direct routes, Lab/Installed paging, Recent Lua routing and native availability without Lua")


if __name__ == "__main__":
    main()

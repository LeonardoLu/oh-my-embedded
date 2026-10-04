#!/usr/bin/env python3
"""Read the packed SYSTEM image with the builder's own LittleFS runtime."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

PROJECT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--read-image", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    build = args.build.resolve()
    if not args.read_image:
        runtime = build / "littlefs_py_venv/bin/python"
        if not runtime.is_file():
            runtime = build / "littlefs_py_venv/Scripts/python.exe"
        assert runtime.is_file(), "Build the SYSTEM partition first"
        subprocess.run([str(runtime), str(Path(__file__).resolve()), "--build",
                        str(build), "--read-image"], check=True)
        return

    from littlefs import LittleFS, UserContext
    image = (build / "system.bin").read_bytes()
    assert len(image) % 4096 == 0
    context = UserContext(len(image))
    context.buffer[:] = image
    # An invalid filesystem must fail; do not auto-format it on mount failure.
    filesystem = LittleFS(context=context, mount=False, block_size=4096,
                          block_count=len(image) // 4096, name_max=255)
    filesystem.mount()
    staging = build / "system_fs_image"
    paths = [p for p in staging.rglob("*") if p.is_file()]
    assert paths, "SYSTEM staging is empty"
    for path in paths:
        relative = str(path.relative_to(staging))
        with filesystem.open("/" + relative, "rb") as packed:
            assert packed.read() == path.read_bytes(), f"Stale SYSTEM image: {relative}"
    for path in (PROJECT / "overlay/fatfs_image/system").rglob("*"):
        if path.is_file():
            relative = path.relative_to(PROJECT / "overlay/fatfs_image/system")
            assert (staging / relative).read_bytes() == path.read_bytes(), f"Stale fluid staging: {relative}"
    for app in ("dino", "flappybird", "album_app", "fluid_liquid", "fluid_particles"):
        with filesystem.open(f"/apps/{app}/launcher.json", "r") as launcher:
            descriptor = json.load(launcher)
        assert descriptor["id"] == app and descriptor.get("visible", True)
        with filesystem.open(f"/apps/{app}/{descriptor['entry']}", "rb") as script:
            assert script.read(), f"Empty Works entry: {app}"
    registry = (build / "esp-idf/mosaic_ui/generated/mosaic_app_registry.c").read_text()
    assert "&mosaic_works_app," in registry
    filesystem.unmount()
    print(f"PASS: {len(paths)} SYSTEM files match packed image; Works launchers and latest fluid sources included")


if __name__ == "__main__":
    main()

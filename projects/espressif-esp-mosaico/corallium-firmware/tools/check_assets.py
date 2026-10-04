#!/usr/bin/env python3
"""Validate generated font references and the exact assets packed for flashing."""
import argparse
from pathlib import Path
import struct
import subprocess
import sys

PROJECT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(PROJECT / "overlay/components/mosaic_ui/common"))
from check_font_refs import require, validate_bundle


def mmap_files(data):
    require(len(data) >= 32 and data[:4] == b"MMAP", "Invalid MMAP header")
    version, name_size, count, checksum, size = struct.unpack_from("<IIIII", data, 4)
    require(version == 0x10000, "Unsupported MMAP version")
    require(size + 32 == len(data), "MMAP size mismatch")
    require(sum(data[32:]) & 0xffff == checksum, "MMAP checksum mismatch")
    base = 32 + count * (name_size + 12)
    require(name_size > 0 and base <= len(data), "MMAP directory truncated")
    result = {}
    for index in range(count):
        offset = 32 + index * (name_size + 12)
        name = data[offset:offset + name_size].split(b"\0", 1)[0].decode("utf-8")
        length, relative = struct.unpack_from("<II", data, offset + name_size)
        start = base + relative
        require(start + 2 + length <= len(data) and data[start:start + 2] == b"ZZ",
                f"Invalid MMAP asset bounds/marker: {name}")
        require(name not in result, f"Duplicate MMAP asset: {name}")
        result[name] = data[start + 2:start + 2 + length]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", required=True, type=Path)
    args = parser.parse_args()
    build = args.build.resolve()
    assets = build / "esp-idf/mosaic_ui"
    generated = assets / "mosaic_gen"
    intermediate = sorted(p for p in generated.glob("*/*.gspb") if p.parent.name != "linked")
    require(bool(intermediate), "No generated application bundles")
    original_refs = sum(validate_bundle(p.read_bytes(), name=str(p)) for p in intermediate)

    packed = mmap_files((build / "mmap_build/ui_apps/ui_apps/ui_apps.bin").read_bytes())
    staged = {p.name: p.read_bytes() for p in (assets / "ui_apps").iterdir() if p.is_file()}
    require(packed == staged, "MMAP image differs from staged assets; rebuild the flash image")
    catalog = packed["common-fonts.gspb"]
    final_refs = sum(validate_bundle(data, catalog, name) for name, data in packed.items()
                     if name.endswith(".gspb") and name != "common-fonts.gspb")
    print(f"PASS: {len(intermediate)} original bundles / {original_refs} font references")
    print(f"PASS: {len(packed)} packed assets match staging; {final_refs} font references resolve in the flash image")
    subprocess.run([sys.executable, str(Path(__file__).with_name("check_system_assets.py")),
                    "--build", str(build)], check=True)


if __name__ == "__main__":
    main()

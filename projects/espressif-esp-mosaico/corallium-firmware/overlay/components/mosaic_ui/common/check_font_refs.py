#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check GSPB/GSB font ordinals against embedded GFBs and a shared catalog.

The pinned GSPC contract uses 32-byte headers, 24-byte GSPB entries and
16-byte GSB section entries. GSB section 9 contains 8-byte asset references;
kind 2 entries, in order, define font_ref ordinals. A matching asset elsewhere
in the bundle does not satisfy a font_ref: its GSPB member ordinal must match.
"""
import argparse
from pathlib import Path
import struct
import zlib


def require(condition, message):
    if not condition:
        raise ValueError(message)


def header(data, magic):
    require(len(data) >= 32 and data[:4] == magic, f"Invalid {magic.decode()} header")
    require(struct.unpack_from('<I', data, 8)[0] == len(data), 'Declared package size differs')
    expected = struct.unpack_from('<I', data, 12)[0]
    require(zlib.crc32(data[:12] + bytes(4) + data[16:]) == expected, 'Package CRC mismatch')
    require(struct.unpack_from('<I', data, 28)[0] == 0, 'Unsupported header flags')
    return struct.unpack_from('<H', data, 16)[0]


def members(data):
    count = header(data, b'GSPB')
    require(struct.unpack_from('<HH', data, 4) in ((1, 2), (1, 3)), 'Unsupported GSPB version')
    require(32 + count * 24 <= len(data), 'GSPB member table truncated')
    result = []
    for index in range(count):
        kind, ordinal, offset, size, asset, name, flags = struct.unpack_from('<HHIIIII', data, 32 + index * 24)
        require(not flags and offset >= 32 + count * 24 and offset + size <= len(data), 'GSPB member bounds/flags invalid')
        result.append((kind, ordinal, asset, data[offset:offset + size]))
    return result


def fonts(data):
    result = {}
    for kind, ordinal, asset, payload in members(data):
        if kind != 3:
            continue
        header(payload, b'GFB1')
        require(struct.unpack_from('<I', payload, 24)[0] == asset, 'GFB content ID disagrees with its bundle')
        require(ordinal not in result, 'Duplicate font member ordinal')
        result[ordinal] = asset
    return result


def scene_fonts(data):
    count = header(data, b'GSB1')
    require(struct.unpack_from('<H', data, 4)[0] == 2, 'Unsupported GSB major version')
    require(32 + count * 16 <= len(data), 'GSB section table truncated')
    result = []
    for index in range(count):
        kind, flags, offset, size, entries = struct.unpack_from('<HHIII', data, 32 + index * 16)
        require(offset + size <= len(data), 'GSB section truncated')
        if kind != 9:
            continue
        require(size == entries * 8, 'Invalid GSB asset table size')
        for pos in range(offset, offset + size, 8):
            asset, asset_kind, asset_flags, reserved = struct.unpack_from('<IBBH', data, pos)
            if asset_kind == 2:
                require(not reserved and asset_flags in (0, 2), 'Unsupported font asset flags')
                result.append(asset)
    return result


def validate_bundle(data, catalog=None, name='bundle'):
    local = fonts(data)
    shared = set(fonts(catalog).values()) if catalog else set()
    refs = 0
    for kind, scene_id, asset, payload in members(data):
        if kind != 1:
            continue
        for ordinal, expected in enumerate(scene_fonts(payload)):
            if ordinal in local:
                actual = local[ordinal]
                require(actual == expected, f'{name}: scene {scene_id} font_ref {ordinal}: expected 0x{expected:08x}, member has 0x{actual:08x}')
            else:
                require(expected in shared, f'{name}: scene {scene_id} font_ref {ordinal}: missing asset 0x{expected:08x} from bundle/catalog')
            refs += 1
    return refs


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundles', nargs='+', type=Path)
    parser.add_argument('--catalog', type=Path)
    args = parser.parse_args()
    catalog = args.catalog.read_bytes() if args.catalog else None
    total = sum(validate_bundle(p.read_bytes(), catalog, str(p)) for p in args.bundles)
    print(f'PASS: {len(args.bundles)} bundles, {total} font references resolve at the correct ordinal')

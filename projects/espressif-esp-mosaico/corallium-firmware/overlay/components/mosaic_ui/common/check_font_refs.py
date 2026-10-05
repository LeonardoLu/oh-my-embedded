#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check GSPB image/vector resources and font ordinals before shipping.

The pinned GSPC contract uses 32-byte headers, 24-byte GSPB entries and
16-byte GSB section entries. GSB section 9 contains 8-byte asset references;
kind 0 entries resolve image/vector resource IDs in the bundle's shared GRB;
kind 2 entries, in order, define font_ref ordinals. A matching font asset
elsewhere in the bundle does not satisfy a font_ref: its member ordinal must
match. GRB 1.4/1.5 section 0x101 uses 40-byte resource records.
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
    require(struct.unpack_from('<HH', data, 4) in ((1, 2), (1, 3), (1, 4)), 'Unsupported GSPB version')
    require(32 + count * 24 <= len(data), 'GSPB member table truncated')
    result = []
    for index in range(count):
        kind, ordinal, offset, size, asset, name, flags = struct.unpack_from('<HHIIIII', data, 32 + index * 24)
        require(not flags and offset >= 32 + count * 24 and offset + size <= len(data), 'GSPB member bounds/flags invalid')
        result.append((kind, ordinal, asset, data[offset:offset + size]))
    return result


def sections(data, magic):
    count = header(data, magic)
    end = 32 + count * 16
    require(end <= len(data), f'{magic.decode()} section table truncated')
    for index in range(count):
        kind, flags, offset, size, entries = struct.unpack_from('<HHIII', data, 32 + index * 16)
        require(offset >= end and offset + size <= len(data), f'{magic.decode()} section bounds invalid')
        yield kind, entries, data[offset:offset + size]


def image_resources(data):
    header(data, b'GRB1')
    require(struct.unpack_from('<HH', data, 4) in ((1, 4), (1, 5)), 'Unsupported GRB version')
    result = set()
    found = False
    for kind, entries, payload in sections(data, b'GRB1'):
        if kind != 0x101:
            continue
        require(not found, 'Duplicate GRB resource table')
        found = True
        require(len(payload) == entries * 40, 'Invalid GRB resource table size')
        for pos in range(0, len(payload), 40):
            resource_id = struct.unpack_from('<I', payload, pos)[0]
            require(resource_id and resource_id not in result, 'Zero or duplicate GRB resource ID')
            result.add(resource_id)
    require(found, 'Missing GRB resource table')
    return result


def scene_images(data):
    header(data, b'GSB1')
    require(struct.unpack_from('<H', data, 4)[0] == 2, 'Unsupported GSB major version')
    found = False
    for kind, entries, payload in sections(data, b'GSB1'):
        if kind != 9:
            continue
        require(not found, 'Duplicate GSB asset table')
        found = True
        require(len(payload) == entries * 8, 'Invalid GSB asset table size')
        for index in range(entries):
            resource_id, asset_kind, flags, reserved = struct.unpack_from('<IBBH', payload, index * 8)
            if asset_kind == 0:
                require(not flags and not reserved, 'Unsupported image/vector asset flags')
                yield index, resource_id


def validate_scene_resources(data, name='bundle'):
    """Resolve all scene image/vector refs against the one shared GRB bank."""
    parsed = members(data)
    banks = [payload for kind, _, _, payload in parsed if kind == 2]
    # GSPC links multi-scene bundles through one shared registry. Combining
    # independently compiled banks could hide absent or conflicting IDs.
    require(len(banks) <= 1, f'{name}: multiple GRB banks require a shared registry')
    resources = image_resources(banks[0]) if banks else set()
    refs = 0
    for kind, scene_id, asset, payload in parsed:
        if kind != 1:
            continue
        for index, resource_id in scene_images(payload):
            require(resource_id in resources,
                    f'{name}: scene {scene_id} asset_ref {index}: image/vector resource_id {resource_id} missing from GRB')
            refs += 1
    return refs


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
    validate_scene_resources(data, name)
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
    images = sum(validate_scene_resources(p.read_bytes(), str(p)) for p in args.bundles)
    print(f'PASS: {len(args.bundles)} bundles, {total} font references at the correct ordinal, {images} image/vector references resolve')

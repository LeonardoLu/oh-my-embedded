#!/usr/bin/env python3
"""Regression for missing raster/vector resources in the shipped GSPB path."""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest
import zlib


sys.dont_write_bytecode = True
COMMON = Path(__file__).resolve().parents[1] / "overlay/components/mosaic_ui/common"
spec = importlib.util.spec_from_file_location("scene_resource_checker", COMMON / "check_font_refs.py")
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


def finish(data, magic, version, count, asset=1):
    struct.pack_into("<4sHHII", data, 0, magic, *version, len(data), 0)
    struct.pack_into("<H", data, 16, count)
    struct.pack_into("<I", data, 24, asset)
    struct.pack_into("<I", data, 12, zlib.crc32(data))
    return bytes(data)


def scene(refs):
    payload = b"".join(struct.pack("<IBBH", resource, kind, 0, 0)
                       for resource, kind in refs)
    data = bytearray(48) + payload
    struct.pack_into("<HHIII", data, 32, 9, 0, 48, len(payload), len(refs))
    return finish(data, b"GSB1", (2, 16), 1)


def bank(resources, version=(1, 5)):
    # Images and SVG vectors share the same resource-ID field and 40-byte
    # metadata record. Their codec does not change reference resolution.
    payload = b"".join(struct.pack("<10I", resource, *([0] * 9))
                       for resource in resources)
    data = bytearray(48) + payload
    struct.pack_into("<HHIII", data, 32, 0x101, 0, 48, len(payload), len(resources))
    return finish(data, b"GRB1", version, 1)


def bundle(*members, version=(1, 3)):
    data = bytearray(32 + len(members) * 24)
    for index, (kind, ordinal, payload) in enumerate(members):
        offset = len(data)
        asset = struct.unpack_from("<I", payload, 24)[0]
        struct.pack_into("<HHIIIII", data, 32 + index * 24,
                         kind, ordinal, offset, len(payload), asset, 0, 0)
        data.extend(payload)
    return finish(data, b"GSPB", version, len(members))


class SceneResourceRefs(unittest.TestCase):
    def test_settings_seed_bank_omits_named_svg(self):
        # Actual failure: 7 font refs + 43 images put the new SVG at asset_ref
        # 50/resource 44, while the independent seed bank stops at ID 43.
        refs = [(100 + index, 2) for index in range(7)]
        refs += [(index, 0) for index in range(1, 45)]
        data = bundle((1, 0, scene(refs)), (2, 0, bank(range(1, 44))))
        with self.assertRaisesRegex(ValueError, "scene 0 asset_ref 50: image/vector resource_id 44 missing from GRB"):
            checker.validate_scene_resources(data, "settings.gspb")
        complete = bundle((1, 0, scene(refs)), (2, 0, bank(range(1, 45))))
        self.assertEqual(checker.validate_scene_resources(complete), 44)

    def test_validate_bundle_enforces_resources_and_keeps_font_count_api(self):
        bad = bundle((1, 0, scene([(44, 0)])), (2, 0, bank([43])))
        with self.assertRaisesRegex(ValueError, "resource_id 44 missing from GRB"):
            checker.validate_bundle(bad)
        good = bundle((1, 0, scene([(44, 0)])), (2, 0, bank([44])))
        self.assertEqual(checker.validate_bundle(good), 0)

    def test_one_registry_serves_all_scenes(self):
        data = bundle((1, 0, scene([(1, 0)])),
                      (1, 4, scene([(44, 0), (1, 0)])),
                      (2, 0, bank([1, 44])), version=(1, 4))
        self.assertEqual(checker.validate_scene_resources(data), 3)
        incomplete = bundle((1, 0, scene([(1, 0)])),
                            (1, 4, scene([(44, 0)])),
                            (2, 0, bank([1])))
        with self.assertRaisesRegex(ValueError, "scene 4 asset_ref 0.*resource_id 44"):
            checker.validate_scene_resources(incomplete)

    def test_independent_banks_cannot_hide_missing_resources(self):
        data = bundle((1, 0, scene([(44, 0)])),
                      (2, 0, bank([1])), (2, 1, bank([44])))
        with self.assertRaisesRegex(ValueError, "multiple GRB banks"):
            checker.validate_scene_resources(data)

    def test_font_only_scene_needs_no_bank(self):
        data = bundle((1, 0, scene([(100, 2)])))
        self.assertEqual(checker.validate_scene_resources(data), 0)
        missing = bundle((1, 0, scene([(1, 0)])))
        with self.assertRaisesRegex(ValueError, "resource_id 1 missing from GRB"):
            checker.validate_scene_resources(missing)

    def test_grb_versions_ids_and_crc(self):
        for version in ((1, 4), (1, 5)):
            data = bundle((1, 0, scene([(44, 0)])), (2, 0, bank([44], version)))
            self.assertEqual(checker.validate_scene_resources(data), 1)
        duplicate = bundle((1, 0, scene([(44, 0)])), (2, 0, bank([44, 44])))
        with self.assertRaisesRegex(ValueError, "duplicate GRB resource ID"):
            checker.validate_scene_resources(duplicate)
        invalid = bytearray(bank([44]))
        invalid[-1] ^= 1
        data = bundle((1, 0, scene([(44, 0)])), (2, 0, invalid))
        with self.assertRaisesRegex(ValueError, "CRC mismatch"):
            checker.validate_scene_resources(data)


if __name__ == "__main__":
    unittest.main()

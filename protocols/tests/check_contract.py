#!/usr/bin/env python3
"""Validate portable wire fixtures, schemas and cross-language BLE declarations."""
import json
from pathlib import Path
import sys
import unittest

try:
    from jsonschema import Draft202012Validator
except ImportError:
    sys.exit("Install protocol test dependencies: python3 -m pip install -r protocols/tests/requirements.txt")

ROOT = Path(__file__).resolve().parents[2]
CONTRACT = ROOT / "protocols/corallium-v1"
SCHEMA = json.loads((CONTRACT / "message.schema.json").read_text())
VALIDATOR = Draft202012Validator(SCHEMA)
CHANNELS = json.loads((CONTRACT / "channels.json").read_text())


def validate(frame):
    VALIDATOR.validate(frame)
    wire = json.dumps(frame, ensure_ascii=False, separators=(",", ":")).encode()
    if len(wire) > CHANNELS["max_line_bytes"]:
        raise ValueError("line exceeds byte limit")
    if frame.get("op") == "wifi.set" and "ok" not in frame:
        payload = frame["payload"]
        if not 1 <= len(payload["ssid"].encode()) <= 32:
            raise ValueError("SSID byte limit")
        length = len(payload["password"].encode())
        if length and not 8 <= length <= 63:
            raise ValueError("password byte limit")


class ContractTests(unittest.TestCase):
    def test_schema(self):
        Draft202012Validator.check_schema(SCHEMA)

    def test_device_admission_policies(self):
        watch, mosaico = (CHANNELS["ble"]["device_profiles"][model] for model in
                          ("m5stack-stopwatch", "espressif-esp-mosaico"))
        self.assertFalse(watch["initial_enabled"])
        self.assertFalse(watch["persist_enabled"])
        self.assertEqual(watch["window_seconds"], 300)
        self.assertEqual(mosaico["admission"], "local_switch")
        self.assertFalse(mosaico["initial_enabled"])
        self.assertTrue(mosaico["persist_enabled"])
        self.assertIsNone(mosaico["window_seconds"])

    def test_complete_session(self):
        pending = {}
        seen = set()
        for line in (CONTRACT / "examples/session.jsonl").read_bytes().splitlines():
            self.assertLessEqual(len(line), 2048)
            frame = json.loads(line)
            validate(frame)
            if "id" not in frame:
                continue
            key = frame["id"]
            if "ok" not in frame:
                self.assertNotIn(key, seen)
                self.assertFalse(pending)
                pending[key] = frame["op"]
                seen.add(key)
            elif key in pending:
                self.assertEqual(frame["op"], pending.pop(key))
        self.assertFalse(pending)

    def test_unknown_readings(self):
        frame = json.loads((CONTRACT / "examples/unknown-status.json").read_text())
        validate(frame)
        self.assertIsNone(frame["payload"]["time"]["unix_ms"])
        self.assertTrue(all(value is None for value in frame["payload"]["battery"].values()))

    def test_stopwatch_capabilities(self):
        frame = json.loads((CONTRACT / "examples/stopwatch-info.json").read_text())
        validate(frame)
        self.assertEqual(frame["payload"]["model"], "m5stack-stopwatch")
        self.assertEqual(set(frame["payload"]["capabilities"]), {"time.set", "battery"})

    def test_warm_rtc_quality(self):
        frame = json.loads((CONTRACT / "examples/warm-rtc-status.json").read_text())
        validate(frame)
        self.assertTrue(frame["payload"]["time"]["valid"])
        self.assertEqual(frame["payload"]["time"]["quality"], "estimated")
        frame["payload"]["time"]["source"] = "checkpoint"
        with self.assertRaises(Exception):
            validate(frame)

    def test_invalid_time_and_boolean_integer(self):
        for milliseconds in (-1, 4102444800000, True, "1790946000000"):
            with self.assertRaises(Exception):
                validate({"v": 1, "id": "1", "op": "time.set", "payload": {
                    "unix_ms": milliseconds, "utc_offset_min": 480}})
        for offset in (-721, 841, 1.5, False):
            with self.assertRaises(Exception):
                validate({"v": 1, "id": "1", "op": "time.set", "payload": {
                    "unix_ms": 1790946000000, "utc_offset_min": offset}})

    def test_utf8_credential_limits(self):
        request = {"v": 1, "id": "1", "op": "wifi.set"}
        for ssid, password in (("珊" * 10, "abcdefgh"), ("x" * 32, ""), ("x", "p" * 63)):
            validate(request | {"payload": {"ssid": ssid, "password": password}})
        for ssid, password in (("珊" * 11, "abcdefgh"), ("", "abcdefgh"), ("x", "short"), ("x", "珊" * 22)):
            with self.assertRaises(Exception):
                validate(request | {"payload": {"ssid": ssid, "password": password}})

    def test_response_and_event_are_disjoint(self):
        frame = json.loads((CONTRACT / "examples/unknown-status.json").read_text())
        validate(frame)
        event = {key: value for key, value in frame.items() if key not in ("id", "ok")}
        validate(event)
        with self.assertRaises(Exception):
            validate(event | {"ok": True})
        with self.assertRaises(Exception):
            validate(frame | {"ok": False})

    def test_untrusted_version_and_ids(self):
        frame = {"v": 1, "id": "1", "op": "device.info", "payload": {}}
        for change in ({"v": 2}, {"v": True}, {"id": ""}, {"id": "a" * 65}, {"id": "with space"}):
            with self.assertRaises(Exception):
                validate(frame | change)


if __name__ == "__main__":
    unittest.main()

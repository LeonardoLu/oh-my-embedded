#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Host protocol/port tests use synthetic I/O and never open a real serial port."""
import base64
from contextlib import redirect_stdout
import importlib.util
import io
import json
from pathlib import Path
import stat
import struct
import sys
import tempfile
from types import ModuleType, SimpleNamespace
import unittest
from unittest.mock import patch
import zlib

SOURCE = Path(__file__).resolve().parents[1] / "tools/device_diagnostics.py"
SPEC = importlib.util.spec_from_file_location("device_diagnostics", SOURCE)
diag = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = diag
SPEC.loader.exec_module(diag)


class Clock:
    def __init__(self):
        self.now = 0.0
        self.sleeps = []

    def read(self):
        return self.now

    def sleep(self, seconds):
        self.sleeps.append(seconds)
        self.now += seconds


class Serial:
    def __init__(self, reply, fragment=47):
        self.reply = reply
        self.fragment = fragment
        self.pending = bytearray()
        self.writes = []
        self.opens = []
        self.closed = False
        self.is_open = False
        self.line_events = []
        self.wire_states = []
        self.dtr = self.rts = None
        self.port = None

    def write(self, data):
        self.writes.append(data)
        fields = data.decode().split()
        self.pending.extend(self.reply(int(fields[1]), fields[2], fields[3:]))
        return len(data)

    def read(self, count):
        chunk = bytes(self.pending[:min(count, self.fragment)])
        del self.pending[:len(chunk)]
        return chunk

    def open(self):
        self.opens.append((self.port, self.dtr, self.rts))
        self.is_open = True
        self.wire_states.append((self.rts, self.dtr))

    def close(self):
        self.line_events.append(("close", None))
        self.is_open = False
        self.closed = True

    @property
    def dtr(self):
        return getattr(self, "_dtr", None)

    @dtr.setter
    def dtr(self, value):
        self.line_events.append(("dtr", value))
        self._dtr = value
        if self.is_open: self.wire_states.append((self.rts, self.dtr))

    @property
    def rts(self):
        return getattr(self, "_rts", None)

    @rts.setter
    def rts(self, value):
        self.line_events.append(("rts", value))
        self._rts = value
        if self.is_open: self.wire_states.append((self.rts, self.dtr))


def line(request_id, payload):
    return f"@MOSAICO {request_id} {payload}\r\n".encode()


def status_values():
    values = {section: {name: 0 for name in fields} for section, fields in diag.STATUS_FIELDS.items()}
    values["device"] = {"chip": "esp32s31", "mac": diag.TARGET_MAC}
    values["uptime_ms"] = 4294967300
    values["ui"].update(app_name="Hub", owner="gsp", width=480, height=480,
                        started=1, panel=1, hub=1, runtime=1, capture_supported=1)
    values["render"].update(frames=9876543210, busy_us=67890123456, last_error=-1)
    values["controls"].update(volume=40, brightness=60, panel_brightness_percent=60,
                              dim_ms=10000, sleep_ms=30000)
    return values


def status_reply(request_id, values=None):
    values = values or status_values()
    sections = (("device", "uptime_ms"), ("ui",), ("render",), ("controls", "battery"))
    return b"".join(line(request_id, ("STATUS " if index < 3 else "OK ") +
                         json.dumps({key: values[key] for key in keys}, separators=(",", ":")))
                    for index, keys in enumerate(sections))


def default_reply(request_id, command, args):
    if command == "ping":
        return line(request_id, f"OK mac={diag.TARGET_MAC} protocol=1")
    if command == "status":
        return status_reply(request_id)
    if command == "open":
        return line(request_id, "OK admitted")
    return line(request_id, "OK")


def client(reply=default_reply, initial_id=10, fragment=47):
    clock = Clock()
    serial = Serial(reply, fragment)
    instance = diag.DiagnosticClient(serial, timeout=0.1, initial_id=initial_id,
                                     clock=clock.read, sleep=clock.sleep)
    return instance, serial, clock


def port(device="/dev/cu.target", *, vid=diag.TARGET_VID, pid=diag.TARGET_PID,
         location=diag.TARGET_LOCATION, serial_number="123456"):
    return SimpleNamespace(device=device, vid=vid, pid=pid, location=location, serial_number=serial_number)


def frame_reply(request_id, pixels, width, height, stride, transform=lambda records: records):
    records = [f"FRAME {width} {height} {stride} RGB565LE {len(pixels)}"]
    for offset in range(0, len(pixels), 192):
        data = pixels[offset:offset + 192]
        records.append(f"DATA {offset} {len(data)} {zlib.crc32(data):08x} {base64.b64encode(data).decode()}")
    records.append(f"END {len(pixels)} {zlib.crc32(pixels):08x}")
    return b"I (1) private log discarded\n" + b"".join(line(request_id, record) for record in transform(records))


class PortTests(unittest.TestCase):
    def test_exact_metadata_only(self):
        target = port()
        unrelated = [port("/dev/cu.other1", location="2-1.4.1"),
                     port("/dev/cu.other2", location="2-1.4.2", serial_number="214201")]
        self.assertIs(diag.select_port(unrelated + [target]), target)
        self.assertIs(diag.select_port(unrelated + [target], target.device), target)
        with self.assertRaises(diag.DiagnosticError):
            diag.select_port(unrelated)
        with self.assertRaises(diag.DiagnosticError):
            diag.select_port([target, port("/dev/cu.ambiguous")])
        with self.assertRaises(diag.DiagnosticError):
            diag.select_port([target], unrelated[0].device)

    def test_rom_or_generic_serial_does_not_select(self):
        with self.assertRaises(diag.DiagnosticError):
            diag.select_port([port(pid=0x0020, serial_number=diag.TARGET_MAC)])
        with self.assertRaises(diag.DiagnosticError):
            diag.select_port([port(location="2-1.4.1")])


class ClientTests(unittest.TestCase):
    def test_requires_identity_before_controls_or_capture(self):
        instance, serial, _ = client()
        for action in (lambda: instance.request("status"), lambda: instance.request("tap", (1, 2)), instance.capture):
            with self.assertRaises(diag.DiagnosticError):
                action()
        self.assertEqual(serial.writes, [])

    def test_wrong_mac_or_protocol_refused(self):
        for payload in ("OK mac=00:00:00:00:00:00 protocol=1", f"OK mac={diag.TARGET_MAC} protocol=2"):
            instance, serial, _ = client(lambda request_id, *_: line(request_id, payload))
            with self.assertRaises(diag.DiagnosticError):
                instance.identify()
            with self.assertRaises(diag.DiagnosticError):
                instance.request("back")
            self.assertEqual(len(serial.writes), 1)

    def test_fragmented_mixed_logs_status_and_request_wrap(self):
        values = status_values()
        values["credentials"] = {"password": "not-to-publish"}
        values["ui"]["scene_text"] = "another-private-value"
        values["controls"]["ssid"] = "private-network"
        def reply(request_id, command, args):
            noise = b"I (1) WiFi password=not-to-publish\n" + line(1000, "ERR argument")
            return noise + (status_reply(request_id, values) if command == "status" else
                            default_reply(request_id, command, args))
        instance, serial, clock = client(reply, initial_id=0xFFFFFFFF, fragment=1)
        instance.identify()
        result = instance.request("status")
        self.assertEqual(result["render"]["frames"], 9876543210)
        self.assertEqual(result["uptime_ms"], 4294967300)
        self.assertNotIn("ssid", result["controls"])
        self.assertNotIn("scene_text", result["ui"])
        self.assertNotIn("credentials", result)
        self.assertEqual([write.split()[1] for write in serial.writes], [b"4294967295", b"1"])
        self.assertGreaterEqual(sum(clock.sleeps), 0.1)

    def test_status_missing_segments_invalid_json_or_identity_rejected(self):
        bad_values = status_values()
        bad_values["device"]["mac"] = "00:00:00:00:00:00"
        streams = [lambda ident: line(ident, "STATUS {\"device\":{}}"),
                   lambda ident: line(ident, "OK {}"),
                   lambda ident: line(ident, "STATUS {broken"),
                   lambda ident: line(ident, 'STATUS {"a":1,"a":2}'),
                   lambda ident: line(ident, 'STATUS {"a":NaN}'),
                   lambda ident: line(ident, "STATUS []"),
                   lambda ident: status_reply(ident, bad_values)]
        for stream in streams:
            with self.subTest(stream=stream):
                instance, _, _ = client(lambda ident, *_: stream(ident))
                instance.verified = True
                with self.assertRaises(diag.DiagnosticError):
                    instance.request("status")

    def test_oversized_logs_drained_and_matching_packet_rejected(self):
        instance, _, _ = client(lambda ident, *_: b"ignored" * 800 + b"\n" + line(ident, "OK"))
        instance.verified = True
        self.assertEqual(instance.request("back")["result"], "OK")
        instance, _, _ = client(lambda ident, *_: line(ident, "OK " + "x" * 512))
        instance.verified = True
        with self.assertRaises(diag.DiagnosticError):
            instance.request("back")

    def test_known_error_only_and_short_write(self):
        for payload, expected in (("ERR unsupported", "unsupported"),
                                  ("ERR busy 0x105", "busy"),
                                  ("ERR PrivatePasswordValue", "INVALID_ERROR_CODE")):
            instance, _, _ = client(lambda ident, *_: line(ident, payload))
            instance.verified = True
            with self.assertRaisesRegex(diag.DiagnosticError, expected) as raised:
                instance.request("back")
            self.assertNotIn("PrivatePasswordValue", str(raised.exception))
        instance, serial, _ = client()
        serial.write = lambda data: len(data) - 1
        with self.assertRaisesRegex(diag.DiagnosticError, "Short"):
            instance.identify()

    def test_input_bounds_and_abort(self):
        for command, args in (("tap", (-1, 10)), ("tap", (480, 0)), ("tap", (True, 2)),
                              ("drag", (0, 0, 479, 479, 49)), ("drag", (0, 0, 1, 1, 2001)),
                              ("status\nback", ()), ("back", (1,))):
            with self.assertRaises(diag.DiagnosticError):
                diag.command_line(1, command, args)
        for request_id in (0, -1, 0x100000000):
            with self.assertRaises(diag.DiagnosticError):
                diag.command_line(request_id, "ping")
        self.assertEqual(diag.command_line(1, "drag", (0, 0, 479, 479, 50)), b"@MOSAICO 1 drag 0 0 479 479 50\n")
        instance, _, _ = client()
        instance.verified = True
        self.assertEqual(instance.request("abort")["result"], "OK")

    def test_direct_open_has_fixed_routes_and_only_acknowledges_admission(self):
        instance, serial, _ = client()
        instance.identify()
        for name in diag.OPEN_APPS:
            result = instance.request("open", (name,))
            self.assertEqual(result["result"], "admitted")
            self.assertNotIn("app_name", result)
        for args in ((), ("/system/apps/settings",), ("settings\nback",), ("SETTINGS",),
                     ("settings", "works"), (True,), ("unknown",)):
            with self.assertRaises(diag.DiagnosticError):
                diag.command_line(1, "open", args)
        self.assertEqual(len(serial.writes), 5)
        instance, _, _ = client(lambda ident, *_: line(ident, "OK"))
        instance.verified = True
        with self.assertRaisesRegex(diag.DiagnosticError, "admission"):
            instance.request("open", ("settings",))

    def test_fragmented_tasks_publish_only_fixed_scalar_metadata(self):
        header = {"snapshot_ms": 654321, "total_tasks": 30, "listed_tasks": 14,
                  "counter_bits": 32, "counter_total_us": 0xFFFFFFFF, "pc_supported": False,
                  "raw_stack": "private"}
        rows = []
        for index, name in enumerate(diag.TASK_NAMES):
            row = {"name": name, "present": index < 6, "number": index + 1 if index < 6 else 0,
                   "state": "blocked" if index < 6 else "missing", "priority": 4 if index < 6 else 0,
                   "base_priority": 4 if index < 6 else 0, "stack_min_bytes": 1234 if index < 6 else 0,
                   "runtime_us": 0xFFFFFFFF if index < 6 else 0, "affinity": -1, "stack_pointer": 1234}
            rows.append(row)
        def reply(ident, *_):
            records = [("TASKS", header), *(("TASK", row) for row in rows), ("OK", {"tasks": 14})]
            return b"credential log discarded\n" + b"".join(
                line(ident, kind + " " + json.dumps(value, separators=(",", ":"))) for kind, value in records)
        instance, _, task_clock = client(reply, fragment=1)
        instance.verified = True
        result = instance.request("tasks")
        self.assertEqual(result["tasks"][0]["name"], "esp_gsp")
        self.assertEqual(result["tasks"][0]["stack_min_bytes"], 1234)
        self.assertEqual(result["counter_total_us"], 0xFFFFFFFF)
        self.assertNotIn("raw_stack", result)
        self.assertNotIn("stack_pointer", result["tasks"][0])
        self.assertFalse(result["pc_supported"])
        first_response = instance.last_tasks_response
        instance.request("tasks")
        self.assertGreaterEqual(task_clock.read() - first_response, diag.TASK_INTERVAL_SECONDS)
        # Wrong order, partial streams and unverifiable fields fail closed.
        for broken in (reply(1).replace(b'"esp_gsp"', b'"unexpected_task"'),
                       reply(1).replace(b'"blocked"', b'"private_wait_name"'),
                       reply(1).replace(b'"affinity":-1', b'"affinity":3'),
                       reply(1).replace(b'"pc_supported":false', b'"pc_supported":true'),
                       reply(1).replace(b'"tasks":14', b'"tasks":13'),
                       reply(1).replace(b'"runtime_us":4294967295', b'"runtime_us":true'),
                       reply(1).replace(b'"state":"missing"', b'"state":"ready"'),
                       reply(1).rsplit(b"@MOSAICO 1 OK", 1)[0]):
            instance, _, _ = client(lambda *_: broken, initial_id=1, fragment=1)
            instance.verified = True
            with self.assertRaises(diag.DiagnosticError):
                instance.request("tasks")

    def test_receive_jitter_and_delayed_responses_pace_all_commands(self):
        clock = Clock()
        class ScheduledSerial(Serial):
            def __init__(self):
                super().__init__(default_reply, fragment=64)
                self.ready_at = 0.0
                self.previous_receive_ms = None
                self.write_times = []
                self.rate_errors = 0

            def write(self, data):
                fields = data.decode().split()
                ident, command = int(fields[1]), fields[2]
                self.write_times.append(clock.read())
                # First RX waits for the reader; the next RX is prompt. A
                # write-to-write 100 ms wait then gives <100 ms at firmware.
                received = clock.read() + (0.040 if not self.writes else 0.001)
                received_ms = int(received * 1000)
                if self.previous_receive_ms is not None and received_ms - self.previous_receive_ms < 100:
                    self.rate_errors += 1
                    response = line(ident, "ERR rate")
                else:
                    self.previous_receive_ms = received_ms
                    if command == "capture":
                        response = frame_reply(ident, b"\x00\xf8\xe0\x07", 2, 1, 4)
                    else:
                        response = default_reply(ident, command, fields[3:])
                self.writes.append(data)
                self.pending.extend(response)
                self.ready_at = received + 0.010
                return len(data)

            def read(self, count):
                if clock.read() < self.ready_at:
                    return b""
                data = super().read(count)
                if data: clock.sleep(0.003)
                return data

        device = ScheduledSerial()
        instance = diag.DiagnosticClient(device, timeout=1, initial_id=1,
                                         clock=clock.read, sleep=clock.sleep)
        previous_response = None
        for action in (instance.identify, lambda: instance.request("status"), instance.capture,
                       lambda: instance.request("abort"), lambda: instance.request("status")):
            action()
            if previous_response is not None:
                self.assertGreaterEqual(device.write_times[-1] - previous_response, 0.120 - 1e-9)
            self.assertGreater(instance.last_response, device.write_times[-1])
            previous_response = instance.last_response
        self.assertEqual(device.rate_errors, 0)
        self.assertEqual([write.split()[2] for write in device.writes], [b"ping", b"status", b"capture", b"abort", b"status"])
        # Negative control reproduces the actual failure with the old anchor.
        clock = Clock()
        device = ScheduledSerial()
        instance = diag.DiagnosticClient(device, timeout=1, initial_id=1,
                                         clock=clock.read, sleep=clock.sleep)
        instance.identify()
        instance.last_response = None
        with patch.object(diag, "COMMAND_INTERVAL_SECONDS", 0.100):
            with self.assertRaisesRegex(diag.DiagnosticError, "rate"):
                instance.request("status")
        self.assertEqual(device.rate_errors, 1)

    def test_matching_error_updates_interval_and_unrelated_response_does_not(self):
        clock = Clock()
        send_times = []
        def reply(ident, command, args):
            send_times.append(clock.read())
            clock.sleep(0.250)
            return line(ident + 123, "OK unrelated") + line(ident, "ERR busy")
        device = Serial(reply)
        instance = diag.DiagnosticClient(device, timeout=1, initial_id=1,
                                         clock=clock.read, sleep=clock.sleep)
        instance.verified = True
        with self.assertRaisesRegex(diag.DiagnosticError, "busy"):
            instance.capture()
        response_time = instance.last_response
        with self.assertRaisesRegex(diag.DiagnosticError, "busy"):
            instance.request("abort")
        self.assertGreaterEqual(send_times[-1] - response_time, 0.120 - 1e-9)
        self.assertGreater(instance.last_response, response_time)


class CaptureTests(unittest.TestCase):
    def capture(self, pixels=b"\x00\xf8\xe0\x07\x1f\x00", width=3, height=1, stride=6,
                transform=lambda records: records):
        instance, _, _ = client(lambda ident, *_: frame_reply(ident, pixels, width, height, stride, transform))
        instance.verified = True
        return instance.capture()

    def test_native_size_capture_chunk_crc_and_png_pixels(self):
        pixels = bytes(range(256)) * 1800
        frame = self.capture(pixels, 480, 480, 960)
        self.assertEqual(frame.pixels, pixels)
        self.assertEqual(frame.crc32, zlib.crc32(pixels))
        frame = self.capture(b"\x00\xf8\xe0\x07\x1f\x00PAD" * 2, 3, 2, 9)
        encoded = diag.png_bytes(frame)
        self.assertEqual(encoded[:8], b"\x89PNG\r\n\x1a\n")
        offset, compressed, dimensions = 8, b"", None
        while offset < len(encoded):
            length = struct.unpack_from(">I", encoded, offset)[0]
            kind = encoded[offset + 4:offset + 8]
            data = encoded[offset + 8:offset + 8 + length]
            checksum = struct.unpack_from(">I", encoded, offset + 8 + length)[0]
            self.assertEqual(checksum, zlib.crc32(kind + data))
            if kind == b"IHDR": dimensions = struct.unpack_from(">II", data)
            if kind == b"IDAT": compressed += data
            offset += 12 + length
        self.assertEqual(dimensions, (3, 2))
        self.assertEqual(zlib.decompress(compressed), b"\x00\xff\x00\x00\x00\xff\x00\x00\x00\xff" * 2)

    def test_even_and_odd_row_padding_base64_tails(self):
        for width, height, stride in ((8, 2, 17), (8, 2, 16), (5, 3, 11)):
            pixels = bytes(range(stride * height))
            frame = self.capture(pixels, width, height, stride)
            self.assertEqual(frame.pixels, pixels)
            self.assertEqual(frame.crc32, zlib.crc32(pixels))
            self.assertTrue(diag.png_bytes(frame).startswith(b"\x89PNG"))

    def test_bad_chunk_final_crc_offset_base64_and_end_rejected(self):
        def replace(index, text):
            return lambda records: records[:index] + [text] + records[index + 1:]
        corruptions = [replace(1, "DATA 0 6 00000000 APjgBx8A"),
                       replace(1, "DATA 1 6 00ffffff APjgBx8A"),
                       replace(1, "DATA 0 193 00ffffff APjgBx8A"),
                       replace(1, "DATA 0 6 00ffffff @@@@"),
                       replace(2, "END 6 00000000"),
                       replace(2, "END 5 00000000"),
                       lambda records: records[:-1],
                       lambda records: [records[0], records[1], records[1], records[2]],
                       lambda records: [records[0], records[2]],
                       replace(0, "FRAME 481 1 962 RGB565LE 962"),
                       replace(0, "FRAME 3 1 5 RGB565LE 6"),
                       replace(0, "FRAME 3 1 1026 RGB565LE 1026"),
                       replace(0, "FRAME 480 480 2000 RGB565LE 960000")]
        for transform in corruptions:
            with self.subTest(transform=transform):
                with self.assertRaises(diag.DiagnosticError):
                    self.capture(transform=transform)

    def test_unsupported_capture_returns_no_frame(self):
        instance, _, _ = client(lambda ident, *_: line(ident, "ERR unsupported"))
        instance.verified = True
        with self.assertRaisesRegex(diag.DiagnosticError, "unsupported"):
            instance.capture()

    def test_private_outputs_only_under_ignored_tmp(self):
        root = diag.REPO / "tmp"
        root.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="diagnostics-test-", dir=root) as directory:
            frame = self.capture()
            paths = diag.save_frame(frame, Path(directory))
            for filename in paths.values():
                self.assertEqual(stat.S_IMODE(Path(filename).stat().st_mode), 0o600)
            self.assertEqual(Path(paths["raw"]).read_bytes(), frame.pixels)
            metadata = json.loads(Path(paths["metadata"]).read_text())
            self.assertEqual(metadata["source"], "device framebuffer")
            self.assertEqual(metadata["crc32"], f"{frame.crc32:08x}")
            broken = diag.Frame(frame.request_id, 3, 1, 6, frame.pixels[:-1], frame.crc32)
            before = set(Path(directory).iterdir())
            with self.assertRaises(diag.DiagnosticError):
                diag.save_frame(broken, Path(directory))
            self.assertEqual(set(Path(directory).iterdir()), before)
        with self.assertRaises(diag.DiagnosticError):
            diag.output_directory(diag.PROJECT)


class MainTests(unittest.TestCase):
    def modules(self, device, ports):
        serial = ModuleType("serial")
        serial.Serial = lambda **kwargs: device
        tools = ModuleType("serial.tools")
        listing = ModuleType("serial.tools.list_ports")
        listing.comports = lambda: ports
        tools.list_ports = listing
        return {"serial": serial, "serial.tools": tools, "serial.tools.list_ports": listing}

    def test_list_or_invalid_control_never_opens(self):
        device = Serial(default_reply)
        with patch.dict(sys.modules, self.modules(device, [port()])), redirect_stdout(io.StringIO()):
            self.assertEqual(diag.main(["--list"]), 0)
            with self.assertRaises(diag.DiagnosticError):
                diag.main(["tap", "480", "0"])
        self.assertEqual(device.opens, [])
        self.assertEqual(device.writes, [])

    def test_single_open_connected_idle_and_private_status_no_logs(self):
        def reply(ident, command, args):
            return b"password=private-value\n" + default_reply(ident, command, args)
        device = Serial(reply)
        diag.REPO.joinpath("tmp").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="diagnostics-cli-test-", dir=diag.REPO / "tmp") as directory:
            output = io.StringIO()
            with patch.dict(sys.modules, self.modules(device, [port(), port("/dev/cu.other", location="2-1.4.1")])):
                with redirect_stdout(output):
                    self.assertEqual(diag.main(["--output", directory, "status"]), 0)
            self.assertEqual(device.opens, [("/dev/cu.target", True, True)])
            self.assertTrue(device.closed)
            self.assertEqual(device.line_events[-3:], [("rts", False), ("dtr", False), ("close", None)])
            self.assertEqual(device.wire_states, [(True, True), (False, True), (False, False)])
            self.assertNotIn((True, False), device.wire_states)
            self.assertEqual([write.split()[2] for write in device.writes], [b"ping", b"status"])
            self.assertNotIn("private-value", output.getvalue())
            files = list(Path(directory).iterdir())
            self.assertEqual(len(files), 1)
            self.assertEqual(stat.S_IMODE(files[0].stat().st_mode), 0o600)

    def test_safe_cleanup_on_wrong_identity(self):
        device = Serial(lambda ident, *_: line(ident, "OK mac=00:00:00:00:00:00 protocol=1"))
        with patch.dict(sys.modules, self.modules(device, [port()])):
            with self.assertRaises(diag.DiagnosticError):
                diag.main(["ping"])
        self.assertEqual(device.line_events[-3:], [("rts", False), ("dtr", False), ("close", None)])
        self.assertNotIn((True, False), device.wire_states)
        self.assertTrue(device.closed)

    def test_control_line_failure_still_closes_without_unsafe_dtr(self):
        class FailingSerial(Serial):
            @Serial.rts.setter
            def rts(self, value):
                if self.is_open and value is False:
                    self.line_events.append(("rts", value))
                    raise OSError("RTS ioctl failed")
                Serial.rts.fset(self, value)
        device = FailingSerial(default_reply)
        with patch.dict(sys.modules, self.modules(device, [port()])), redirect_stdout(io.StringIO()):
            self.assertEqual(diag.main(["ping"]), 0)
        self.assertTrue(device.closed)
        self.assertEqual(device.line_events[-2:], [("rts", False), ("close", None)])
        self.assertNotIn(("dtr", False), device.line_events)
        self.assertNotIn((True, False), device.wire_states)

    def test_dtr_serial_exception_still_closes(self):
        class SerialException(OSError):
            pass
        class FailingSerial(Serial):
            @Serial.dtr.setter
            def dtr(self, value):
                if self.is_open and value is False:
                    self.line_events.append(("dtr", value))
                    raise SerialException("DTR ioctl failed")
                Serial.dtr.fset(self, value)
        device = FailingSerial(default_reply)
        with patch.dict(sys.modules, self.modules(device, [port()])), redirect_stdout(io.StringIO()):
            self.assertEqual(diag.main(["ping"]), 0)
        self.assertTrue(device.closed)
        self.assertEqual(device.line_events[-3:], [("rts", False), ("dtr", False), ("close", None)])
        self.assertNotIn((True, False), device.wire_states)


if __name__ == "__main__":
    unittest.main()

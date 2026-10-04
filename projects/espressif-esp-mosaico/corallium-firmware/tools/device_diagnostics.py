#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""USB-only diagnostics for the identified ESP-Mosaico; captures stay in tmp/."""
from __future__ import annotations

import argparse
import base64
import binascii
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import struct
import sys
import time
import zlib

TARGET_MAC = "1c:29:04:d0:90:36"
TARGET_LOCATION = "2-1.4.3"
TARGET_VID, TARGET_PID = 0x303A, 0x1001
PREFIX = b"@MOSAICO "
MAX_LINE_BYTES = 4096
MAX_FRAME_BYTES = 480 * 1024
COMMAND_INTERVAL_SECONDS = 0.120
TASK_INTERVAL_SECONDS = 1.020
ERROR_CODES = {"syntax", "argument", "command", "line", "rate", "busy", "unsupported", "ui",
               "status", "tasks", "asleep", "owner", "frame", "timeout", "cancelled", "overflow"}
OPEN_APPS = ("settings", "works", "album", "weather")
TASK_NAMES = ("esp_gsp", "gsp_decode", "mosaic_runtime", "screen_power", "works_runtime",
              "usb_diagnostics", "esp_timer", "Tmr Svc", "IDLE0", "IDLE1", "wifi_manager",
              "weather", "main", "sys_evt")
PROJECT = Path(__file__).resolve().parents[1]
REPO = PROJECT.parents[2]


class DiagnosticError(Exception):
    pass


def select_port(ports, requested: str | None = None):
    """Never probe another device to identify this one; topology selects first."""
    matches = [p for p in ports if p.vid == TARGET_VID and p.pid == TARGET_PID
               and p.location == TARGET_LOCATION]
    if len(matches) != 1:
        raise DiagnosticError(f"Expected one application CDC at {TARGET_LOCATION}; found {len(matches)}")
    selected = matches[0]
    if requested is not None and selected.device != requested:
        raise DiagnosticError("Requested port is not the identified target's application CDC")
    return selected


STATUS_FIELDS = {
    "device": {"chip", "mac"},
    "ui": {"app_name", "app_id", "owner", "width", "height", "rotation",
           "started", "asleep", "dimmed", "panel", "hub", "runtime", "quiesced",
           "paused", "drawer", "gesture", "queue", "capture_supported"},
    "render": {"frames", "busy_us", "errors", "last_error", "runtime_errors", "last_runtime_error",
               "last_input_error"},
    "controls": {"volume", "brightness", "panel_brightness_percent", "dim_ms", "sleep_ms", "poweroff_ms", "dim_charge", "sleep_charge",
                 "wifi_enabled", "wifi_connected", "bluetooth_enabled"},
    "battery": {"available", "charging"},
}


def public_status(status):
    """Publish only documented scalar fields; never echo arbitrary device JSON."""
    result = {}
    if type(status.get("uptime_ms")) is not int or status["uptime_ms"] < 0:
        raise DiagnosticError("Device status is missing valid uptime")
    result["uptime_ms"] = status["uptime_ms"]
    for section, fields in STATUS_FIELDS.items():
        values = status.get(section)
        if not isinstance(values, dict) or not fields <= values.keys():
            raise DiagnosticError("Device status is missing a required section")
        result[section] = {}
        for key in sorted(fields):
            value = values[key]
            string_field = key in ("chip", "mac", "app_name", "owner")
            if (string_field and (not isinstance(value, str) or len(value) > 64
                                  or any(ord(char) < 32 for char in value))
                    or not string_field and (type(value) not in (int, bool)
                                             or key not in ("last_error", "last_runtime_error", "last_input_error") and value < 0)):
                raise DiagnosticError("Invalid public device status value")
            result[section][key] = value
    if (result["device"].get("chip") != "esp32s31"
            or str(result["device"].get("mac", "")).lower() != TARGET_MAC):
        raise DiagnosticError("Status identity does not match the identified ESP-Mosaico")
    if result["ui"]["owner"] not in ("none", "gsp", "exclusive"):
        raise DiagnosticError("Unsupported device display owner")
    return result


def status_object(payload):
    def unique_pairs(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("Duplicate JSON key")
            result[key] = value
        return result
    try:
        value = json.loads(payload, object_pairs_hook=unique_pairs,
                           parse_constant=lambda _: (_ for _ in ()).throw(ValueError("Nonfinite JSON")))
    except (ValueError, RecursionError):
        raise DiagnosticError("Device returned invalid diagnostic status JSON") from None
    if not isinstance(value, dict):
        raise DiagnosticError("Device status must be an object")
    return value


def public_tasks(header, rows, end):
    """Expose fixed live task counters, without dereferencing device pointers."""
    fields = ("snapshot_ms", "total_tasks", "listed_tasks", "counter_bits", "counter_total_us")
    if (any(type(header.get(key)) is not int or header[key] < 0 for key in fields)
            or not 1 <= header["total_tasks"] <= 64
            or header["listed_tasks"] != len(TASK_NAMES) or header["counter_bits"] != 32
            or header["counter_total_us"] > 0xFFFFFFFF or header.get("pc_supported") is not False
            or type(end.get("tasks")) is not int or end["tasks"] != len(TASK_NAMES)
            or len(rows) != len(TASK_NAMES)):
        raise DiagnosticError("Invalid bounded task snapshot header/end")
    result = {key: header[key] for key in fields}
    result["pc_supported"] = False
    result["tasks"] = []
    numeric = ("number", "priority", "base_priority", "stack_min_bytes", "runtime_us", "affinity")
    states = {"running", "ready", "blocked", "suspended", "deleted", "invalid"}
    for expected, row in zip(TASK_NAMES, rows):
        if (row.get("name") != expected or type(row.get("present")) is not bool
                or any(type(row.get(key)) is not int for key in numeric)
                or any(not 0 <= row[key] <= 0xFFFFFFFF for key in numeric[:-1])
                or row["affinity"] not in (-1, 0, 1)
                or row.get("state") not in (states if row["present"] else {"missing"})
                or not row["present"] and (any(row[key] != 0 for key in numeric[:-1])
                                          or row["affinity"] != -1)):
            raise DiagnosticError("Invalid or out-of-order public task record")
        result["tasks"].append({key: row[key] for key in ("name", "present", "state", *numeric)})
    return result


def command_line(request_id: int, command: str, arguments=()) -> bytes:
    if not 1 <= request_id <= 0xFFFFFFFF:
        raise DiagnosticError("Request ID must be in 1..4294967295")
    args = tuple(arguments)
    if command in ("ping", "status", "tasks", "back", "capture", "abort"):
        valid = not args
    elif command == "open":
        valid = len(args) == 1 and type(args[0]) is str and args[0] in OPEN_APPS
    elif command == "tap":
        valid = len(args) == 2 and all(type(v) is int and 0 <= v <= 479 for v in args)
    elif command == "drag":
        valid = (len(args) == 5 and all(type(v) is int and 0 <= v <= 479 for v in args[:4])
                 and type(args[4]) is int and 50 <= args[4] <= 2000)
    else:
        valid = False
    if not valid:
        raise DiagnosticError("Unsupported or out-of-range diagnostic command")
    suffix = "".join(f" {value}" for value in args)
    encoded = f"@MOSAICO {request_id} {command}{suffix}\n".encode("ascii")
    if len(encoded) > 128:
        raise DiagnosticError("Diagnostic command exceeds firmware line limit")
    return encoded


@dataclass(frozen=True)
class Frame:
    request_id: int
    width: int
    height: int
    stride: int
    pixels: bytes
    crc32: int


class DiagnosticClient:
    def __init__(self, device, timeout=15.0, *, initial_id=None, clock=time.monotonic, sleep=time.sleep):
        self.device = device
        self.timeout = timeout
        self.clock, self.sleep = clock, sleep
        self.next_id = initial_id if initial_id is not None else secrets.randbelow(0xFFFFFFFF) + 1
        self.last_sent = None
        self.last_response = None
        self.last_tasks_sent = None
        self.last_tasks_response = None
        self.verified = False
        self.buffer = bytearray()
        self.discarding = False

    def _send(self, command, arguments=()):
        request_id = self.next_id
        encoded = command_line(request_id, command, arguments)
        self.next_id = 1 if request_id == 0xFFFFFFFF else request_id + 1
        references = [value for value in (self.last_sent, self.last_response) if value is not None]
        if references:
            # The firmware times receipt, after USB/reader scheduling. Anchor
            # the next command to its preceding response, with a 20 ms margin.
            remaining = COMMAND_INTERVAL_SECONDS - (self.clock() - max(references))
            if remaining > 0:
                self.sleep(remaining)
        if command == "tasks":
            task_times = [value for value in (self.last_tasks_sent, self.last_tasks_response) if value is not None]
            if task_times:
                remaining = TASK_INTERVAL_SECONDS - (self.clock() - max(task_times))
                if remaining > 0:
                    self.sleep(remaining)
        if self.device.write(encoded) != len(encoded):
            raise DiagnosticError("Short USB diagnostic request write")
        self.last_sent = self.clock()
        if command == "tasks":
            self.last_tasks_sent = self.last_sent
        return request_id

    def _lines(self, deadline):
        while self.clock() < deadline:
            split = self.buffer.find(b"\n")
            if split >= 0:
                line = bytes(self.buffer[:split]).rstrip(b"\r")
                del self.buffer[:split + 1]
                if self.discarding:
                    self.discarding = False
                    continue
                if len(line) > MAX_LINE_BYTES:
                    continue
                yield line
                continue
            if len(self.buffer) > MAX_LINE_BYTES:
                self.buffer.clear()
                self.discarding = True
            chunk = self.device.read(512)
            if chunk:
                self.buffer.extend(chunk)
            else:
                self.sleep(0.005)
        raise DiagnosticError("Timed out waiting for a complete device diagnostic response")

    def _responses(self, request_id, deadline):
        for line in self._lines(deadline):
            if not line.startswith(PREFIX):
                continue  # Do not print/persist mixed console logs, including credentials.
            fields = line.split(b" ", 3)
            if len(fields) < 3 or not re.fullmatch(rb"[0-9]{1,10}", fields[1]):
                continue
            if int(fields[1]) != request_id:
                continue
            if len(line) + 1 > 512:
                raise DiagnosticError("Diagnostic response exceeds firmware packet limit")
            try:
                kind = fields[2].decode("ascii")
                payload = fields[3].decode("ascii") if len(fields) == 4 else ""
            except UnicodeDecodeError:
                raise DiagnosticError("Non-ASCII diagnostic response") from None
            self.last_response = self.clock()
            if kind == "ERR":
                code = payload.split(" ", 1)[0]
                if code not in ERROR_CODES:
                    code = "INVALID_ERROR_CODE"
                raise DiagnosticError(f"Device diagnostics rejected the request: {code}")
            yield kind, payload

    def identify(self):
        self.verified = False
        request_id = self._send("ping")
        kind, payload = next(self._responses(request_id, self.clock() + self.timeout))
        fields = dict(re.findall(r"([a-z_]+)=([^ ]+)", payload))
        if kind != "OK" or fields.get("mac", "").lower() != TARGET_MAC or fields.get("protocol") != "1":
            raise DiagnosticError("Device identity/protocol does not match the identified ESP-Mosaico")
        self.verified = True
        return {"mac": TARGET_MAC, "protocol": 1}

    def request(self, command, arguments=()):
        if not self.verified:
            raise DiagnosticError("Verify target identity before sending diagnostic commands")
        if command == "capture":
            raise DiagnosticError("Use capture() for framebuffer transfer")
        request_id = self._send(command, arguments)
        responses = self._responses(request_id, self.clock() + self.timeout)
        if command == "status":
            status = {}
            for expected in ("STATUS", "STATUS", "STATUS", "OK"):
                kind, payload = next(responses)
                if kind != expected:
                    raise DiagnosticError("Incomplete or out-of-order diagnostic status")
                part = status_object(payload)
                if status.keys() & part.keys():
                    raise DiagnosticError("Duplicate diagnostic status section")
                status.update(part)
            return public_status(status)
        if command == "tasks":
            kind, payload = next(responses)
            if kind != "TASKS":
                raise DiagnosticError("Missing task snapshot header")
            header = status_object(payload)
            rows = []
            for _ in TASK_NAMES:
                kind, payload = next(responses)
                if kind != "TASK":
                    raise DiagnosticError("Incomplete task snapshot records")
                rows.append(status_object(payload))
            kind, payload = next(responses)
            if kind != "OK":
                raise DiagnosticError("Missing task snapshot end")
            result = public_tasks(header, rows, status_object(payload))
            self.last_tasks_response = self.clock()
            return result
        kind, payload = next(responses)
        if kind != "OK":
            raise DiagnosticError("Unexpected device diagnostic response")
        if command == "open":
            if payload != "admitted":
                raise DiagnosticError("Missing application queue admission acknowledgement")
            return {"request_id": request_id, "result": "admitted"}
        return {"request_id": request_id, "result": "OK"}

    def capture(self):
        if not self.verified:
            raise DiagnosticError("Verify target identity before framebuffer capture")
        request_id = self._send("capture")
        responses = self._responses(request_id, self.clock() + self.timeout)
        kind, header = next(responses)
        parts = header.split()
        if kind != "FRAME" or len(parts) != 5 or parts[3] != "RGB565LE":
            raise DiagnosticError("Unsupported framebuffer header")
        if not all(re.fullmatch(r"[0-9]{1,8}", parts[i]) for i in (0, 1, 2, 4)):
            raise DiagnosticError("Invalid framebuffer dimensions")
        width, height, stride, total = map(int, (parts[0], parts[1], parts[2], parts[4]))
        if (not 1 <= width <= 480 or not 1 <= height <= 480 or not width * 2 <= stride <= 1024
                or total != stride * height or not 0 < total <= MAX_FRAME_BYTES):
            raise DiagnosticError("Framebuffer exceeds bounded dimensions/stride")
        pixels = bytearray()
        for kind, payload in responses:
            parts = payload.split()
            if kind == "END":
                if (len(parts) != 2 or not re.fullmatch(r"[0-9]{1,8}", parts[0])
                        or not re.fullmatch(r"[0-9a-fA-F]{8}", parts[1])):
                    raise DiagnosticError("Invalid framebuffer END record")
                checksum = int(parts[1], 16)
                if int(parts[0]) != total or len(pixels) != total or zlib.crc32(pixels) != checksum:
                    raise DiagnosticError("Incomplete framebuffer or final CRC mismatch")
                return Frame(request_id, width, height, stride, bytes(pixels), checksum)
            if (kind != "DATA" or len(parts) != 4
                    or not re.fullmatch(r"[0-9]{1,8}", parts[0])
                    or not re.fullmatch(r"[0-9]{1,3}", parts[1])
                    or not re.fullmatch(r"[0-9a-fA-F]{8}", parts[2])):
                raise DiagnosticError("Invalid framebuffer DATA record")
            offset, count = int(parts[0]), int(parts[1])
            if offset != len(pixels) or not 1 <= count <= 192 or offset + count > total:
                raise DiagnosticError("Framebuffer DATA offset/length mismatch")
            try:
                decoded = base64.b64decode(parts[3], validate=True)
            except (ValueError, binascii.Error):
                raise DiagnosticError("Invalid framebuffer base64") from None
            if len(decoded) != count or zlib.crc32(decoded) != int(parts[2], 16):
                raise DiagnosticError("Framebuffer chunk CRC/length mismatch")
            pixels.extend(decoded)
        raise DiagnosticError("Missing framebuffer END record")


def png_bytes(frame: Frame) -> bytes:
    if (not 1 <= frame.width <= 480 or not 1 <= frame.height <= 480
            or not frame.width * 2 <= frame.stride <= 1024
            or len(frame.pixels) != frame.stride * frame.height
            or len(frame.pixels) > MAX_FRAME_BYTES or zlib.crc32(frame.pixels) != frame.crc32):
        raise DiagnosticError("Invalid framebuffer supplied for PNG encoding")
    def chunk(name, payload):
        return struct.pack(">I", len(payload)) + name + payload + struct.pack(">I", zlib.crc32(name + payload))
    rows = bytearray()
    for y in range(frame.height):
        rows.append(0)
        first = y * frame.stride
        for x in range(frame.width):
            offset = first + x * 2
            value = frame.pixels[offset] | frame.pixels[offset + 1] << 8
            rows.extend((((value >> 11) & 31) * 255 // 31,
                         ((value >> 5) & 63) * 255 // 63, (value & 31) * 255 // 31))
    ihdr = struct.pack(">IIBBBBB", frame.width, frame.height, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")


def output_directory(argument: Path | None) -> Path:
    root = (REPO / "tmp").resolve()
    directory = (argument if argument is not None else root / "mosaico/device-diagnostics").resolve()
    if not directory.is_relative_to(root):
        raise DiagnosticError("Diagnostic output must stay inside this repository's ignored tmp/")
    directory.mkdir(parents=True, exist_ok=True)
    return directory


def save_frame(frame: Frame, directory: Path):
    encoded_png = png_bytes(frame)  # Validate before creating any artifact.
    directory = output_directory(directory)
    stem = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ") + f"-{frame.request_id}"
    binary, png, metadata = (directory / f"{stem}{suffix}" for suffix in (".rgb565", ".png", ".json"))
    private_write(binary, frame.pixels)
    private_write(png, encoded_png)
    private_write(metadata, (json.dumps({"mac": TARGET_MAC, "location": TARGET_LOCATION,
        "source": "device framebuffer", "transport": "USB CDC diagnostics",
        "request_id": frame.request_id, "width": frame.width, "height": frame.height,
        "stride": frame.stride, "format": "RGB565LE", "bytes": len(frame.pixels),
        "crc32": f"{frame.crc32:08x}", "sha256": hashlib.sha256(frame.pixels).hexdigest()}, indent=2) + "\n").encode())
    return {"png": str(png), "raw": str(binary), "metadata": str(metadata)}


def private_write(path: Path, data: bytes):
    """Create private diagnostic artifacts without a temporary public mode."""
    descriptor = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    with os.fdopen(descriptor, "wb") as output:
        output.write(data)


def close_port(device):
    """Clear factory control lines in safe order, then close even on I/O failure."""
    try:
        if device.is_open:
            # (1,1) -> (0,1) -> (0,0); lowering DTR first would request reset.
            device.rts = False
            device.dtr = False
    except OSError:
        # SerialException is an OSError. If RTS failed, leave DTR alone.
        pass
    finally:
        device.close()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true", help="Enumerate target metadata without opening any port")
    parser.add_argument("--port", help="Optional exact port; it must match the target's USB identity/topology")
    parser.add_argument("--timeout", type=float, default=15.0)
    parser.add_argument("--output", type=Path, help="Capture/status output directory under repository tmp/")
    sub = parser.add_subparsers(dest="command")
    for name in ("ping", "status", "tasks", "back", "capture", "abort"):
        sub.add_parser(name)
    open_app = sub.add_parser("open")
    open_app.add_argument("app", choices=OPEN_APPS)
    tap = sub.add_parser("tap")
    tap.add_argument("x", type=int); tap.add_argument("y", type=int)
    drag = sub.add_parser("drag")
    for name in ("x0", "y0", "x1", "y1", "duration_ms"):
        drag.add_argument(name, type=int)
    args = parser.parse_args(argv)
    if not 0 < args.timeout <= 120:
        raise DiagnosticError("Timeout must be within 0..120 seconds")
    import serial
    from serial.tools import list_ports
    selected = select_port(list(list_ports.comports()), args.port)
    if args.list:
        print(json.dumps({"port": selected.device, "vid": "303a", "pid": "1001",
                          "location": TARGET_LOCATION, "expected_mac": TARGET_MAC}))
        return 0
    command = args.command or "status"
    arguments = ()
    if command == "tap": arguments = (args.x, args.y)
    if command == "drag": arguments = (args.x0, args.y0, args.x1, args.y1, args.duration_ms)
    if command == "open": arguments = (args.app,)
    command_line(1, command, arguments)  # Reject invalid controls before opening USB.
    directory = output_directory(args.output) if command in ("capture", "status", "tasks") else None
    device = serial.Serial(port=None, baudrate=115200, timeout=0.1, write_timeout=1, exclusive=True)
    # Factory (RTS,DTR)=(1,1) is inert and DTR=1 enables the CDC connection.
    # Preconfigure before open; never send the (1,0) reset state.
    try:
        device.dtr = True
        device.rts = True
        device.port = selected.device
        device.open()
        client = DiagnosticClient(device, args.timeout)
        identity = client.identify()
        if command == "capture":
            result = save_frame(client.capture(), directory)
        elif command == "ping":
            result = identity
        else:
            result = client.request(command, arguments)
            if command in ("status", "tasks"):
                filename = directory / (datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ") + f"-{command}.json")
                private_write(filename, (json.dumps(result, indent=2, ensure_ascii=False) + "\n").encode())
        print(json.dumps(result, ensure_ascii=False))
    finally:
        close_port(device)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (DiagnosticError, ImportError, OSError) as error:
        print(f"Device diagnostics: {error}", file=sys.stderr)
        raise SystemExit(1) from None

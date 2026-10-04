#!/usr/bin/env python3
"""Enumerate a compatible Micro HID interface and query its status without keys.

Install hidapi in a virtual environment, then run this script. By default it only
enumerates. --status sends device.status and waits for the matching response.
"""

import argparse
import json
import time

VID = 0x303A
PID = 0x8360
USAGE_PAGE = 0xFF00
REPORT_ID = 6
CHANNEL = 2


def reports(message):
    # The app sends a complete JSON value with no newline. Only firmware replies
    # and notifications are newline-terminated; match the real host here.
    data = json.dumps(message, separators=(",", ":")).encode("utf-8")
    for offset in range(0, len(data), 61):
        chunk = data[offset:offset + 61]
        yield bytes((REPORT_ID, CHANNEL, len(chunk))) + chunk.ljust(61, b"\0")


class Decoder:
    def __init__(self):
        self.pending = bytearray()

    def feed(self, report):
        if len(report) < 3 or report[0] != REPORT_ID or report[1] != CHANNEL:
            return []
        count = report[2]
        if count > 61 or len(report) < count + 3:
            raise ValueError("Invalid Micro HID chunk length")
        self.pending.extend(report[3:3 + count])
        if len(self.pending) > 8192:
            self.pending.clear()
            raise ValueError("Unterminated Micro HID message exceeds 8 KiB")
        messages = []
        while b"\n" in self.pending:
            line, _, rest = self.pending.partition(b"\n")
            self.pending = bytearray(rest)
            if line.strip():
                messages.append(json.loads(line))
        return messages


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--status", action="store_true")
    parser.add_argument("--path", help="Choose an enumerated path if multiple devices exist")
    parser.add_argument("--timeout", type=float, default=8)
    args = parser.parse_args()
    import hid

    devices = [d for d in hid.enumerate(VID, PID) if d.get("usage_page") == USAGE_PAGE]
    for device in devices:
        print(json.dumps({k: v.decode(errors="replace") if isinstance(v, bytes) else v
                          for k, v in device.items()}, ensure_ascii=False))
    if not devices:
        raise SystemExit("No Micro-compatible HID interface detected. Pair Core2 in Bluetooth settings.")
    if not args.status:
        return
    if args.path:
        devices = [d for d in devices if d["path"].decode() == args.path]
    if len(devices) != 1:
        raise SystemExit("Choose exactly one device with --path before querying status.")
    device = hid.device()
    try:
        device.open_path(devices[0]["path"])
        request_id = 701
        for report in reports({"method": "device.status", "params": None, "id": request_id}):
            device.write(report)
        decoder = Decoder()
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline:
            for response in decoder.feed(bytes(device.read(64, timeout_ms=100))):
                print(json.dumps(response, ensure_ascii=False))
                if response.get("id") == request_id:
                    if "error" in response:
                        raise SystemExit("Device returned an RPC error")
                    result = response.get("result")
                    if not isinstance(result, dict) or not isinstance(result.get("version"), str):
                        raise SystemExit("Invalid device.status response")
                    return
        raise SystemExit("Timed out waiting for device.status response")
    finally:
        device.close()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Sync the Watch RTC to this computer's local time over serial.

Sends the device's existing `rtc set YYYY-MM-DDTHH:MM:SS` diagnostic command
(verified write, see specs/watch-clock-continuity.md) and then reads the clock
back to report the residual skew. No firmware timezone conversion happens on
the device; this sends local wall time as-is.

The command retries for a few seconds, but a dozed Keys-only watch drops all
serial input while asleep — wake it with a touch or button press and re-run.

Usage: python3 tools/sync_rtc.py [--port /dev/cu.usbmodemXXXX]
"""

from __future__ import annotations

import argparse
import glob
import re
import sys
from datetime import datetime

import serial

BAUD = 115200
BOOT_TIMEOUT_S = 6.0
REPLY_TIMEOUT_S = 3.0
BOOT_BANNER = "bot-ux-watch ready"


def detect_port(explicit: str | None) -> str:
    if explicit:
        return explicit
    candidates = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not candidates:
        sys.exit("No /dev/cu.usbmodem* port found; pass --port explicitly.")
    if len(candidates) > 1:
        print("Multiple usbmodem ports found:", file=sys.stderr)
        for c in candidates:
            print(f"  {c}", file=sys.stderr)
    return candidates[0]


def open_port(path: str) -> serial.Serial:
    port = serial.Serial()
    port.port = path
    port.baudrate = BAUD
    port.timeout = 0.2
    # Deassert the USB-CDC reset lines before open so a plain sync does not
    # reboot the watch (a reset would still be harmless, just slower).
    port.dtr = False
    port.rts = False
    port.open()
    return port


def wait_for_boot(port: serial.Serial) -> None:
    """Drain early output. A reset-triggering open prints the boot banner; a
    non-resetting open on a dozed watch prints nothing, which is fine — the
    command exchanges below retry across the doze wake windows."""
    deadline = datetime.now().timestamp() + BOOT_TIMEOUT_S
    while datetime.now().timestamp() < deadline:
        if BOOT_BANNER in port.read(4096).decode(errors="replace"):
            return


def exchange(port: serial.Serial, command: str, attempts: int = 1) -> list[str]:
    """Send a command, retrying because a dozed Keys-only watch sleeps with a
    one-second timer poll and silently drops bytes sent while asleep."""
    for attempt in range(attempts):
        port.reset_input_buffer()
        port.write((command + "\n").encode())
        deadline = datetime.now().timestamp() + REPLY_TIMEOUT_S
        lines: list[str] = []
        while datetime.now().timestamp() < deadline:
            chunk = port.read(1024).decode(errors="replace")
            for line in chunk.splitlines():
                line = line.strip()
                if line:
                    lines.append(line)
            if any(line.startswith(("RTC set=", "RTC read=")) for line in lines):
                return lines
    return []


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", help="serial device (default: first /dev/cu.usbmodem*)")
    args = parser.parse_args()

    path = detect_port(args.port)
    print(f"Port: {path}")
    port = open_port(path)
    try:
        wait_for_boot(port)

        # Capture local wall time as late as possible to minimize skew; retries
        # of a dropped command reuse the same captured second.
        local = datetime.now().strftime("%Y-%m-%dT%H:%M:%S")
        set_lines = exchange(port, f"rtc set {local}", attempts=8)
        if not set_lines:
            sys.exit("No reply — the watch is dozed (Keys-only wake drops serial "
                     "input while asleep). Wake it with a touch or button press, "
                     "then re-run.")
        print(" ".join(set_lines))
        if not any(line == "RTC set=1" for line in set_lines):
            sys.exit("Device rejected the time write.")
        # The firmware only reports set=1 after its own verified calendar
        # readback, so the write is already confirmed at this point.

        read_lines = exchange(port, "rtc", attempts=8)
        read_text = " ".join(read_lines)
        match = re.search(r"time=(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d)", read_text)
        if match and "valid=1" in read_text:
            print(read_text)
            device_time = datetime.strptime(match.group(1), "%Y-%m-%dT%H:%M:%S")
            skew = (device_time - datetime.now()).total_seconds()
            print(f"Device clock {match.group(1)} (skew {skew:+.0f}s vs host) — synced.")
        else:
            # Expected when the watch has dozed into Keys-only light sleep.
            print("Write verified on-device; live readback unavailable "
                  "(watch likely dozed — wake it to read the clock).")
    finally:
        port.close()


if __name__ == "__main__":
    main()

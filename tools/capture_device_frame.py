#!/usr/bin/env python3
"""Capture a diagnostic RGB565 canvas from firmware, preserving native font pixels.

Requires pyserial. This captures the firmware render target, not a photograph of
the panel. The device pauses rendering during this explicit diagnostic transfer.
"""

import argparse
import re
from pathlib import Path
import struct
import time
import zlib


def png_chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def write_png(path, width, height, pixels, endian):
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            offset = (y * width + x) * 2
            value = int.from_bytes(pixels[offset:offset + 2], endian)
            r, g, b = (value >> 11) & 31, (value >> 5) & 63, value & 31
            rows.extend(((r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31))
    result = b"\x89PNG\r\n\x1a\n"
    result += png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    result += png_chunk(b"IDAT", zlib.compress(rows))
    result += png_chunk(b"IEND", b"")
    path.write_bytes(result)


def diagnostic_page(value):
    if not re.fullmatch(r"v?[0-9]{1,2}", value):
        raise argparse.ArgumentTypeError("Use a page ID 0..99, or v0..v99 for StopWatch")
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("output", type=Path)
    parser.add_argument("--timeout", type=float, default=60)
    parser.add_argument("--boot-wait", type=float, default=2,
                        help="Seconds for initialization after serial open (Core2 BLE can need 8)")
    parser.add_argument("--screen", type=diagnostic_page, metavar="ID",
                        help="Firmware diagnostic page: Core2 0..99; StopWatch v0..v99")
    args = parser.parse_args()
    import serial

    device = serial.Serial(port=None, baudrate=115200, timeout=0.3)
    device.dtr = False
    device.rts = False
    device.port = args.port
    try:
        device.open()
        # Native USB can reset when the host opens the interface. Wait for boot,
        # then request one frame. No settings or host input actions are changed.
        time.sleep(args.boot_wait)
        device.reset_input_buffer()
        if args.screen is not None:
            device.write(f"{args.screen}\n".encode())
            time.sleep(0.5)
        device.write(b"c\n")
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline:
            line = device.readline().decode(errors="replace").strip()
            if line.startswith("FRAME "):
                break
        else:
            raise SystemExit("No FRAME header from device")
        _, width, height, encoding, size = line.split()
        width, height, size = int(width), int(height), int(size)
        if encoding not in ("RGB565LE", "RGB565BE") or size != width * height * 2 or size > 2000000:
            raise SystemExit("Invalid frame header")
        pixels = bytearray()
        while len(pixels) < size and time.monotonic() < deadline:
            pixels.extend(device.read(min(8192, size - len(pixels))))
        if len(pixels) != size:
            raise SystemExit(f"Incomplete frame: {len(pixels)}/{size} bytes")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        write_png(args.output, width, height, pixels, "little" if encoding.endswith("LE") else "big")
        print(f"Saved {width}x{height} native canvas: {args.output}")
        if args.screen is not None:
            device.write(b"0\n")
    finally:
        device.close()


if __name__ == "__main__":
    main()

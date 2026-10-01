#!/usr/bin/env python3
"""Test unloaded PPS output through the demo's 115200-baud serial console."""

import argparse
import json
import math
from pathlib import Path
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--empty-output", action="store_true", required=True,
                        help="Confirm that nothing is connected to the banana outputs")
    parser.add_argument("--log", type=Path, required=True)
    args = parser.parse_args()
    args.log.parent.mkdir(parents=True, exist_ok=True)

    # Opening a Core2 console should not pulse its automatic programming pins.
    connection = serial.Serial()
    connection.port = args.port
    connection.baudrate = 115200
    connection.timeout = 0.3
    connection.dtr = False
    connection.rts = False
    connection.open()
    results = []

    with args.log.open("w") as log:
        def record(line):
            print(line, flush=True)
            log.write(line + "\n")
            log.flush()

        def line_before(deadline):
            while time.monotonic() < deadline:
                line = connection.readline().decode("utf-8", "replace").strip()
                if line:
                    record(line)
                    return line
            raise TimeoutError("No serial response before timeout")

        def command(text):
            connection.reset_input_buffer()
            record("> " + text)
            connection.write((text + "\n").encode())
            deadline = time.monotonic() + 5
            while True:
                line = line_before(deadline)
                if line.startswith("ERROR"):
                    raise RuntimeError(line)
                if line.startswith("OK"):
                    return

        def status():
            connection.reset_input_buffer()
            connection.write(b"STATUS\n")
            deadline = time.monotonic() + 5
            while True:
                line = line_before(deadline)
                if line.startswith("{"):
                    data = json.loads(line)
                    if not data.get("ready") or data.get("id") != 0x1041:
                        raise RuntimeError("PPS 0x1041 not detected")
                    for key in ("vin", "vout", "iout", "v_set", "i_set"):
                        if not math.isfinite(data[key]):
                            raise RuntimeError("Invalid telemetry: " + key)
                    return data

        try:
            # Wait for startup telemetry before sending an output command.
            status()
            command("OFF")
            initial = status()
            if not 9 <= initial["vin"] <= 36:
                raise RuntimeError("PPS input must be DC 9-36V")
            for voltage in (1.0, 3.3, 5.0, 8.0, 5.0, 3.3, 1.0):
                if voltage > initial["vin"] - 1:
                    continue
                command(f"SET {voltage:.3f} 0.100")
                command("ON")
                time.sleep(1.5)
                samples = []
                for _ in range(3):
                    sample = status()
                    if not sample["enabled"] or sample["mode"] != 1:
                        raise RuntimeError("Expected enabled constant-voltage output")
                    if abs(sample["v_set"] - voltage) > 0.002 or abs(sample["i_set"] - 0.1) > 0.002:
                        raise RuntimeError("Setpoint readback mismatch")
                    if abs(sample["vout"] - voltage) > 0.08:
                        raise RuntimeError("Output readback differs by more than 80mV")
                    if abs(sample["iout"]) > 0.02:
                        raise RuntimeError("Unexpected current on unloaded output")
                    samples.append(sample)
                    time.sleep(0.35)
                mean_voltage = sum(sample["vout"] for sample in samples) / len(samples)
                results.append({"set_v": voltage, "mean_vout": round(mean_voltage, 4)})
                record(f"PASS {voltage:.3f}V: mean readback {mean_voltage:.4f}V")
                command("OFF")
        finally:
            try:
                command("OFF")
                time.sleep(1)
                final = status()
                if final["enabled"] or final["mode"] != 0:
                    raise RuntimeError("Output disable was not confirmed")
                record("OUTPUT OFF CONFIRMED")
            finally:
                connection.close()

        record("RESULT " + json.dumps(results))
        record("PASS unloaded sweep; CC/load regulation and independent meter accuracy untested")


if __name__ == "__main__":
    main()

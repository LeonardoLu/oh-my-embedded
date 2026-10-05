#!/usr/bin/env python3
"""Sanitize the real parser/task with bounded CDC/UI boundary doubles.

This never opens a serial port. Generated transcripts exercise transport only;
their deterministic bytes are not device screenshots or hardware acceptance.
"""

import argparse
import base64
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zlib


PROJECT = Path(__file__).resolve().parents[1]
REPO = PROJECT.parents[2]


def run(command, **kwargs):
    subprocess.run([str(item) for item in command], check=True, **kwargs)


def check_frames(path):
    active = None
    completed = 0
    for line in path.read_bytes().splitlines():
        if not line:
            continue
        assert len(line) + 4 <= 512
        match = re.fullmatch(rb"@MOSAICO ([0-9]+) (.*)", line)
        assert match, "Unexpected transport line"
        request_id, body = int(match[1]), match[2].split(b" ")
        if body[0] == b"FRAME":
            assert active is None and len(body) == 6
            width, height, stride, total = map(int, (body[1], body[2], body[3], body[5]))
            assert body[4] == b"RGB565LE" and 1 <= width <= 480 and 1 <= height <= 480
            assert width * 2 <= stride <= 1024 and total == stride * height <= 491520
            active = {"id": request_id, "size": total, "data": bytearray()}
        elif body[0] == b"DATA":
            assert active and request_id == active["id"] and len(body) == 5
            offset, size = int(body[1]), int(body[2])
            assert offset == len(active["data"]) and 0 < size <= 192
            assert re.fullmatch(rb"[0-9a-f]{8}", body[3])
            raw = base64.b64decode(body[4], validate=True)
            assert len(raw) == size and zlib.crc32(raw) == int(body[3], 16)
            assert offset + size <= active["size"]
            active["data"].extend(raw)
        elif body[0] == b"END":
            assert active and request_id == active["id"] and len(body) == 3
            assert int(body[1]) == len(active["data"]) == active["size"]
            assert zlib.crc32(active["data"]) == int(body[2], 16)
            expected = bytes((offset * 29 + 7) & 255 for offset in range(active["size"]))
            assert active["data"] == expected
            active = None
            completed += 1
        else:
            raise AssertionError("Unexpected capture response")
    assert active is None and completed == 4
    print("Frame transcript: 491520-byte bound, row padding, base64 tails and independent CRC passed")


def check_status(path):
    merged = {}
    kinds = []
    for line in path.read_bytes().splitlines():
        if not line:
            continue
        assert len(line) + 4 <= 512
        match = re.fullmatch(rb"@MOSAICO 4294967295 (STATUS|OK) (.*)", line)
        assert match
        kinds.append(match[1])
        section = json.loads(match[2])
        assert not merged.keys() & section.keys()
        merged.update(section)
    assert kinds == [b"STATUS", b"STATUS", b"STATUS", b"OK"]
    assert merged["device"] == {"chip": "esp32s31", "mac": "1c:29:04:d0:90:36"}
    assert merged["uptime_ms"] == 2**64 - 1
    assert merged["ui"]["app_name"] == "Settings" and merged["ui"]["app_id"] == 3
    assert merged["controls"]["brightness"] == 80
    assert merged["controls"]["panel_brightness_percent"] == 15
    assert {"frames", "busy_us", "errors", "last_error", "runtime_errors",
            "last_runtime_error", "last_input_error"} == merged["render"].keys()
    assert set(merged) == {"device", "uptime_ms", "ui", "render", "controls", "battery"}
    encoded = json.dumps(merged)
    assert not re.search(r"ssid|password|credential|secret|token", encoded, re.I)
    print("Status transcript: four complete JSON sections, identity and saved/panel brightness passed")


def check_tasks(path):
    names = ["esp_gsp", "gsp_decode", "mosaic_runtime", "screen_power", "works_runtime",
             "usb_diagnostics", "esp_timer", "Tmr Svc", "IDLE0", "IDLE1", "wifi_manager",
             "weather", "main", "sys_evt"]
    entries = []
    kinds = []
    for line in path.read_bytes().splitlines():
        if not line:
            continue
        assert len(line) + 4 <= 512
        match = re.fullmatch(rb"@MOSAICO 81 (TASKS|TASK|OK) (.*)", line)
        assert match
        kind, item = match[1], json.loads(match[2])
        kinds.append(kind)
        if kind == b"TASKS":
            assert item == {"snapshot_ms": 9000, "total_tasks": 4, "listed_tasks": 14,
                            "counter_bits": 32, "counter_total_us": 2**32 - 1,
                            "pc_supported": False}
        elif kind == b"TASK":
            assert set(item) == {"name", "present", "number", "state", "priority", "base_priority",
                                 "stack_min_bytes", "runtime_us", "affinity"}
            assert item["name"] == names[len(entries)] and type(item["present"]) is bool
            if not item["present"]:
                assert item["state"] == "missing" and item["affinity"] == -1
                assert all(item[key] == 0 for key in ("number", "priority", "base_priority",
                                                    "stack_min_bytes", "runtime_us"))
            entries.append(item)
        else:
            assert item == {"tasks": 14}
    assert kinds == [b"TASKS", *([b"TASK"] * 14), b"OK"]
    assert entries[0]["state"] == "blocked" and entries[0]["priority"] == 5
    assert entries[0]["base_priority"] == 4 and entries[0]["stack_min_bytes"] == 768
    assert entries[0]["runtime_us"] == 2**32 - 1 and entries[0]["affinity"] == -1
    assert entries[2]["affinity"] == 1
    assert not re.search(rb"ssid|password|credential|secret|token|0x[0-9a-f]+", path.read_bytes(), re.I)
    print("Tasks transcript: fixed 14 names, whole JSON packets, counter width, affinity and no raw addresses passed")


def check_client_compatibility(frames_path, status_path, tasks_path):
    # This transport is memory only, with split reads; it cannot open a device.
    class Transcript:
        def __init__(self, path):
            self.stream = io.BytesIO(path.read_bytes())
            self.requests = []

        def read(self, size):
            return self.stream.read(min(size, 37))

        def write(self, data):
            self.requests.append(bytes(data))
            return len(data)

    tool = PROJECT / "tools/device_diagnostics.py"
    spec = importlib.util.spec_from_file_location("diagnostics_client_host_fixture", tool)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    transcript = Transcript(frames_path)
    client = module.DiagnosticClient(transcript, initial_id=77)
    client.verified = True  # Only deterministic host fixtures bypass live MAC verification.
    for request_id, size in zip(range(77, 81), (491520, 34, 32, 33)):
        frame = client.capture()
        assert frame.request_id == request_id and len(frame.pixels) == size
        assert frame.crc32 == zlib.crc32(frame.pixels)
    assert len(transcript.requests) == 4
    client = module.DiagnosticClient(Transcript(status_path), initial_id=4294967295)
    client.verified = True  # Same memory-only fixture boundary as capture above.
    status = client.request("status")
    assert status["uptime_ms"] == 2**64 - 1
    assert status["controls"]["brightness"] == 80
    assert status["controls"]["panel_brightness_percent"] == 15
    client = module.DiagnosticClient(Transcript(tasks_path), initial_id=81)
    client.verified = True  # Fixed C task fixture, no port or device handle.
    tasks = client.request("tasks")
    assert tasks["counter_total_us"] == 2**32 - 1 and tasks["pc_supported"] is False
    assert len(tasks["tasks"]) == 14 and tasks["tasks"][0]["state"] == "blocked"
    assert tasks["tasks"][0]["stack_min_bytes"] == 768 and tasks["tasks"][2]["affinity"] == 1
    print("Client compatibility: real C formatter, split memory reads, padded frames and merged status passed")


def check_eui64_negative_control(compiler, flags, component, output):
    negative_dir = output / "eui64-negative"
    negative_dir.mkdir(parents=True, exist_ok=True)
    source = (component / "mosaico_diagnostics.c").read_text()
    correct_call = "esp_read_mac(mac, ESP_MAC_EFUSE_FACTORY)"
    assert source.count(correct_call) == 1
    (negative_dir / "mosaico_diagnostics.c").write_text(
        source.replace(correct_call, "esp_efuse_mac_get_default(mac)"))
    binary = negative_dir / "test-eui64-overflow"
    run([compiler, "-I", negative_dir, *flags, PROJECT / "tests/test_diagnostics_transport.c",
         component / "mosaico_diagnostics_core.c", component / "mosaico_diagnostics_tasks.c",
         PROJECT / "tests/diagnostics_tasks_fixture.c", "-o", binary])
    env = os.environ.copy()
    env["ASAN_OPTIONS"] = "abort_on_error=1:detect_leaks=0"
    result = subprocess.run([str(binary), "--legacy-negative"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=20, env=env)
    (negative_dir / "asan.log").write_bytes(result.stdout)
    assert result.returncode != 0, "Legacy EUI-64 call unexpectedly passed"
    assert b"AddressSanitizer: stack-buffer-overflow" in result.stdout
    assert b"WRITE of size 8" in result.stdout
    print("MAC negative control: S31 EUI-64 write to MAC-48 buffer caught by ASan")


def check_picolibc_negative_control(compiler, flags, component, output):
    source = (component / "mosaico_diagnostics.c").read_text()
    assert not re.search(r"\b(?:ftrylockfile|flockfile|funlockfile)\s*\(", source)
    start = source.index("static size_t send_packet(")
    end = source.index("\nstatic void diagnostics_task(", start)
    legacy = (PROJECT / "tests/diagnostics_picolibc_legacy_transport.inc").read_text()
    negative_dir = output / "picolibc-negative"
    negative_dir.mkdir(parents=True, exist_ok=True)
    (negative_dir / "mosaico_diagnostics.c").write_text(source[:start] + legacy + source[end:])
    binary = negative_dir / "test-global-lock-leak"
    run([compiler, "-I", negative_dir, *flags, "-DMOSAICO_DIAGNOSTICS_PICOLIBC_NEGATIVE=1",
         PROJECT / "tests/test_diagnostics_transport.c", component / "mosaico_diagnostics_core.c",
         component / "mosaico_diagnostics_tasks.c", PROJECT / "tests/diagnostics_tasks_fixture.c",
         "-o", binary])
    result = subprocess.run([str(binary), "--picolibc-negative"], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=20)
    (negative_dir / "negative.log").write_bytes(result.stdout)
    assert result.returncode != 0, "Former FILE API pair unexpectedly balanced the Picolibc global lock"
    assert b"PICOLIBC_GLOBAL_LOCK_LEAK" in result.stdout
    print("Picolibc negative control: former transport leaks global lock under reviewed linked ABI model")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--upstream", type=Path,
                        help="Use the prepared component rather than the tracked overlay")
    parser.add_argument("--output", type=Path, default=REPO / "tmp/mosaico/diagnostics-host-tests")
    parser.add_argument("--idf", type=Path, default=REPO / "tmp/mosaico/esp-idf",
                        help="Use the pinned SDK esp_mac.h when available")
    args = parser.parse_args()
    component = ((args.upstream.resolve() / "components/mosaico_diagnostics") if args.upstream else
                 PROJECT / "overlay/components/mosaico_diagnostics")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-g", "-fsanitize=address,undefined",
             "-I", PROJECT / "tests/diagnostics_stubs", "-I", component / "include", "-I", component]
    compiler = os.environ.get("CC", "clang")
    sdk_header = args.idf.resolve() / "components/esp_hw_support/include"
    transport_flags = flags
    if (sdk_header / "esp_mac.h").is_file():
        transport_flags = ["-I", sdk_header, *flags]
        mac_source = sdk_header.parent / "mac_addr.c"
        if mac_source.is_file():
            assert re.search(r"\{\s*ESP_MAC_EFUSE_FACTORY\s*,\s*STATE_INIT\s*,\s*6\s*,",
                             mac_source.read_text()), "Pinned factory MAC-48 table contract changed"
        print("MAC interface: compiling against pinned SDK esp_mac.h and esp_mac_type_t", flush=True)
    else:
        print("MAC interface: portable enum stub only; pinned SDK header unavailable", flush=True)
    core = output / "test-core"
    task = output / "test-transport"
    tasks_sources = [component / "mosaico_diagnostics_tasks.c", PROJECT / "tests/diagnostics_tasks_fixture.c"]
    run([compiler, *flags, PROJECT / "tests/test_diagnostics_core.c",
         component / "mosaico_diagnostics_core.c", *tasks_sources, "-o", core])
    run([core])
    run([compiler, *transport_flags, PROJECT / "tests/test_diagnostics_transport.c",
         component / "mosaico_diagnostics_core.c", *tasks_sources, "-o", task])
    run([task])
    for bits in (32, 64):
        tasks_binary = output / f"test-tasks-u{bits}"
        run([compiler, *flags, f"-DconfigRUN_TIME_COUNTER_TYPE=uint{bits}_t",
             PROJECT / "tests/test_diagnostics_tasks.c", *tasks_sources, "-o", tasks_binary])
        run([tasks_binary])
    check_eui64_negative_control(compiler, transport_flags, component, output)
    check_picolibc_negative_control(compiler, transport_flags, component, output)
    disabled_source = output / "disabled.c"
    disabled_source.write_text('#include <assert.h>\n#include "mosaico_diagnostics.h"\n'
                               '#include "mosaico_diagnostics_tasks.h"\n'
                               'int main(void) { assert(mosaico_diagnostics_start(0) == '
                               'ESP_ERR_NOT_SUPPORTED); assert(mosaico_diagnostics_tasks_snapshot(0) == '
                               'ESP_ERR_NOT_SUPPORTED); return 0; }\n')
    disabled = output / "test-disabled"
    run([compiler, *flags, "-DCONFIG_MOSAICO_USB_DIAGNOSTICS=0", "-DconfigUSE_TRACE_FACILITY=0",
         "-DconfigGENERATE_RUN_TIME_STATS=0", disabled_source, component / "mosaico_diagnostics.c",
         component / "mosaico_diagnostics_tasks.c", "-o", disabled])
    run([disabled])
    frames = output / "frame-transcript.txt"
    with frames.open("wb") as stream:
        run([core, "--transcript"], stdout=stream)
    check_frames(frames)
    status = output / "status-transcript.txt"
    with status.open("wb") as stream:
        run([core, "--status-transcript"], stdout=stream)
    check_status(status)
    tasks = output / "tasks-transcript.txt"
    with tasks.open("wb") as stream:
        run([core, "--tasks-transcript"], stdout=stream)
    check_tasks(tasks)
    check_client_compatibility(frames, status, tasks)
    print("USB diagnostics host checks passed; no serial/device operations were performed")


if __name__ == "__main__":
    main()

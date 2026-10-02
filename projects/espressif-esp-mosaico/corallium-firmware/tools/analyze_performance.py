#!/usr/bin/env python3
"""Summarize actual native renderer samples; does not infer device acceptance."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("log", type=Path)
args = parser.parse_args()
samples = []
for line in args.log.read_text(errors="replace").splitlines():
    if "mosaico_perf," not in line:
        continue
    sample = json.loads(line.split("mosaico_perf,", 1)[1])
    if sample["elapsed_ms"] > 0 and sample["frames"] >= 0:
        samples.append(sample)
if not samples:
    raise SystemExit("No native renderer samples; enable CONFIG_CORALLIUM_PERF_LOG and capture a device log.")
elapsed = sum(s["elapsed_ms"] for s in samples)
frames = sum(s["frames"] for s in samples)
print(json.dumps({"samples": len(samples), "observed_seconds": elapsed / 1000,
                  "frames": frames, "observed_fps": frames * 1000 / elapsed,
                  "mean_frame_render_ms": sum(s["render_us"] for s in samples) / frames / 1000 if frames else None,
                  "note": "Includes idle time; compare identical interaction traces and power conditions."}, indent=2))

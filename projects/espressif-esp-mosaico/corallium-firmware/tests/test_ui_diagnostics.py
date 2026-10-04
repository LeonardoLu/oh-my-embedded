#!/usr/bin/env python3
"""Compile real loader/capture code; fake only SDK hardware and task boundaries."""
import argparse
import os
from pathlib import Path
import re
import subprocess

PROJECT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--upstream", type=Path, required=True)
parser.add_argument("--output", type=Path)
args = parser.parse_args()
upstream = args.upstream.resolve()
output = (args.output or PROJECT.parents[2] / "tmp/mosaico/host-tests/ui-diagnostics").resolve()
output.mkdir(parents=True, exist_ok=True)
ui = upstream / "components/mosaic_ui"
gsp = upstream / "managed_components/espressif__esp-gsp"
presenter = upstream / "managed_components/espressif__esp_display_present"
source = (ui / "mosaic_ui.c").read_text()
# Compile each selected UI entry verbatim, preserving its original source line.
# The unrelated startup/ASR/module code stays outside this focused harness.
functions = ("screen_apply_wake", "prepare_simulated_tap", "mosaic_ui_simulate_tap",
             "mosaic_ui_simulate_drag", "mosaic_ui_get_diagnostics", "mosaic_ui_back",
             "mosaic_ui_open_app", "mosaic_ui_capture_begin", "mosaic_ui_capture_read",
             "mosaic_ui_capture_end")
fragments = []
# Keep registry lookup real too; the harness supplies catalog metadata only.
catalog = ui / "common/mosaic_app_catalog.c"
catalog_source = catalog.read_text()
catalog_match = re.search(r"^const mosaic_app_descriptor_t \*mosaic_app_descriptor_for_name\(",
                          catalog_source, re.M)
if not catalog_match:
    raise SystemExit("Missing real App catalog lookup")
catalog_end = catalog_source.index("\n}\n", catalog_match.start()) + 3
catalog_line = catalog_source.count("\n", 0, catalog_match.start()) + 1
fragments.append(f'#line {catalog_line} "{catalog}"\n' +
                 catalog_source[catalog_match.start():catalog_end])
for name in functions:
    match = re.search(r"^(?:static )?(?:esp_err_t|bool|void) " + name + r"\(", source, re.M)
    if not match:
        raise SystemExit(f"Missing real UI entry point: {name}")
    end = source.index("\n}\n", match.start()) + 3
    line = source.count("\n", 0, match.start()) + 1
    fragments.append(f'#line {line} "{ui / "mosaic_ui.c"}"\n' + source[match.start():end])
(output / "ui_diagnostics_under_test.inc").write_text("\n".join(fragments))
command = [os.environ.get("CC", "clang"), "-std=c11", "-Wall", "-Wextra", "-Werror",
           "-Wno-unused-parameter", "-fsanitize=address,undefined", "-pthread"]
for include in (PROJECT / "tests/ui_diagnostics_stubs", output, ui / "include", ui / "core",
                ui / "common", ui / "runtime", ui / "hub", gsp / "include",
                presenter / "include", upstream / "components/corallium/include"):
    command.extend(["-I", str(include)])
capture_object = output / "capture.o"
subprocess.run([*command, "-Dmemcpy=mosaic_test_memcpy", "-c",
                str(ui / "core/mosaic_frame_capture.c"), "-o", str(capture_object)], check=True)
subprocess.run([*command, str(PROJECT / "tests/test_ui_diagnostics.c"),
                str(capture_object), "-o", str(output / "test-ui-diagnostics")], check=True)
subprocess.run([str(output / "test-ui-diagnostics")], check=True)

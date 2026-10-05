#!/usr/bin/env python3
"""Execute the real Back/activity functions with explicit queue/router doubles.

This checks that an awake, dimmed screen receives REARM before Back dispatch.
The existing display-idle service test covers dim/brightness restoration;
this regression does not run FreeRTOS, the physical panel or shutdown GPIO.
"""

import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r"(?:void|esp_err_t) " + re.escape(name) +
                      r"\([^;]*?\)\s*\{", source)
    if not match:
        raise ValueError(f"Missing real function: {name}")
    start = match.start()
    depth = 1
    end = match.end()
    while depth:
        if end == len(source):
            raise ValueError(f"Unterminated function: {name}")
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


HARNESS = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef int esp_err_t;
enum {
    ESP_OK = 0,
    ESP_ERR_NOT_FOUND = 0x105,
    ESP_ERR_TIMEOUT = 0x107,
    SCREEN_CMD_REARM = 1U << 2,
};
static unsigned s_calls[3];
static unsigned s_count;
static bool s_dimmed;
static bool s_asleep;
static bool s_rearm_pending;
static esp_err_t s_exit_result;
static esp_err_t s_back_result;

static void screen_post(uint32_t command, bool from_isr)
{
    assert(s_dimmed && !s_asleep);
    assert(command == SCREEN_CMD_REARM && !from_isr);
    assert(s_count == 0);
    s_calls[s_count++] = 1;
    s_rearm_pending = true;
}

static esp_err_t display_service_request_exit(void)
{
    assert(s_rearm_pending && s_count == 1);
    s_calls[s_count++] = 2;
    return s_exit_result;
}

static esp_err_t mosaic_loader_request_back(void)
{
    assert(s_rearm_pending && s_count == 2);
    assert(s_exit_result == ESP_ERR_NOT_FOUND);
    s_calls[s_count++] = 3;
    return s_back_result;
}

/* Exact extracted implementations follow. */
"""

CASES = r"""
static void check(esp_err_t exit_result, esp_err_t back_result)
{
    s_count = 0;
    s_rearm_pending = false;
    s_dimmed = true;
    s_asleep = false;
    s_exit_result = exit_result;
    s_back_result = back_result;
    const esp_err_t result = mosaic_ui_back();
    assert(s_calls[0] == 1 && s_calls[1] == 2 && s_rearm_pending);
    if (exit_result != ESP_ERR_NOT_FOUND) {
        assert(s_count == 2 && result == exit_result);
    } else {
        assert(s_count == 3 && s_calls[2] == 3 && result == back_result);
    }
}

int main(void)
{
    check(ESP_OK, ESP_ERR_TIMEOUT);
    check(ESP_ERR_TIMEOUT, ESP_OK);
    check(ESP_ERR_NOT_FOUND, ESP_OK);
    check(ESP_ERR_NOT_FOUND, ESP_ERR_TIMEOUT);
    puts("PASS: real Back queues activity on an awake dimmed screen before "
         "exit/loader routing; exit priority and errors preserved");
    return 0;
}
"""


def check(upstream, output):
    source = (upstream / "components/mosaic_ui/mosaic_ui.c").read_text()
    native = output / "display_back.c"
    native.write_text(HARNESS +
                      function(source, "mosaic_ui_note_screen_activity") +
                      "\n" + function(source, "mosaic_ui_back") + "\n" + CASES)
    binary = output / "display-back"
    subprocess.run([os.environ.get("CC", "clang"), "-std=c11", "-Wall",
                    "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-omit-frame-pointer", str(native), "-o", str(binary)],
                   check=True)
    subprocess.run([str(binary)], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.output:
        output = args.output.resolve()
        output.mkdir(parents=True, exist_ok=True)
        check(args.upstream.resolve(), output)
    else:
        with tempfile.TemporaryDirectory(prefix="display-back-") as directory:
            check(args.upstream.resolve(), Path(directory))


if __name__ == "__main__":
    main()

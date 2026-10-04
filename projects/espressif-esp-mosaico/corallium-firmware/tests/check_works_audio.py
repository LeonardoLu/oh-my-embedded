#!/usr/bin/env python3
"""Run the real output facade/worker in the firmware's bundled 32-bit Lua VM."""
import argparse
import os
from pathlib import Path
import re
import subprocess

PROJECT = Path(__file__).resolve().parents[1]
REPO = PROJECT.parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--upstream', required=True, type=Path)
args = parser.parse_args()
upstream = args.upstream.resolve()
vm = upstream / 'managed_components/georgik__lua'
if not (vm / 'lua/lapi.c').is_file():
    raise SystemExit('Configure firmware dependencies first; bundled Lua is unavailable.')
output = REPO / 'tmp/mosaico/host-tests/works-audio'
output.mkdir(parents=True, exist_ok=True)
compiler = os.environ.get('CC', 'clang')
sources = [vm / source for source in re.findall(r'"(lua/[^"\n]+\.c)"',
                                               (vm / 'CMakeLists.txt').read_text())]
maxstack = re.search(r'^CONFIG_LUA_MAXSTACK=(\d+)$', (upstream / 'sdkconfig').read_text(), re.M)
if not maxstack or not sources:
    raise SystemExit('Configured bundled Lua build inputs are unavailable.')
overlay = PROJECT / 'overlay/components'
command = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
           '-Wno-unused-parameter', '-Wno-implicit-fallthrough',
           '-fsanitize=address,undefined', '-DLUA_32BITS=1',
           f'-DCONFIG_LUA_MAXSTACK={maxstack.group(1)}',
           '-I', str(PROJECT / 'tests/audio_stubs'), '-I', str(vm / 'include'),
           '-I', str(vm / 'lua'),
           '-I', str(overlay / 'mosaico_audio/include'),
           '-I', str(overlay / 'mosaic_ui/common'),
           str(PROJECT / 'tests/test_works_audio.c'),
           str(overlay / 'mosaico_audio/mosaico_audio.c'),
           str(overlay / 'mosaic_ui/common/works_audio.c'),
           *map(str, sources), '-lm', '-o', str(output / 'test-works-audio')]
compiled = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
(output / 'compile.log').write_text(compiled.stdout)
if compiled.returncode:
    print('\n'.join(compiled.stdout.splitlines()[:24]))
    raise SystemExit(f'Works audio host compile failed; full diagnostics: {output / "compile.log"}')
subprocess.run([str(output / 'test-works-audio'), str(PROJECT / 'tests/test_works_audio.lua'),
                str(upstream / 'fatfs_image/system/apps/flappybird/scripts/flappybird.lua')], check=True)

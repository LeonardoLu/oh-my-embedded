#!/usr/bin/env python3
"""Run fluid tests with the firmware's bundled float32/int32 Lua VM.

From the repository root, after firmware dependency resolution:
python3 projects/espressif-esp-mosaico/corallium-firmware/tests/check_fluid_runtime.py \
    --lua-component tmp/mosaico/corallium-firmware/managed_components/georgik__lua --sanitize

Only the Lua core is hosted. The tests execute the shipped physics and runner
with a fake display/policy boundary; this does not establish device performance
or hardware presenter/power acceptance.
"""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

PROJECT = Path(__file__).resolve().parents[1]
REPO = PROJECT.parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lua-component", type=Path, required=True)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    component = args.lua_component.resolve()
    config = component / "include/luaconf.h"
    if not config.exists() or not re.search(r"#define\s+LUA_32BITS\s+1", config.read_text()):
        parser.error("pass the resolved firmware georgik__lua component with LUA_32BITS=1")
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        parser.error("a host C compiler is required")
    sources = re.findall(r'^\s*"(lua/[^\"]+\.c)"',
                         (component / "CMakeLists.txt").read_text(), re.MULTILINE)
    if not sources or not (component / "lua/lua.c").exists():
        parser.error("the resolved Lua component is incomplete")
    sources.append("lua/lua.c")
    scratch = REPO / "tmp/mosaico"
    scratch.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="fluid-lua32-", dir=scratch))
    executable = output / "lua"
    command = [compiler, "-O1" if args.sanitize else "-O2",
               "-DCONFIG_LUA_MAXSTACK=1000000", f"-I{component / 'include'}",
               f"-I{component / 'lua'}"]
    if args.sanitize:
        command += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    "-fno-omit-frame-pointer"]
    command += [str(component / source) for source in sources]
    command += ["-lm", "-o", str(executable)]
    log = output / "build.log"
    with log.open("w") as stream:
        result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        raise SystemExit(f"Lua host build failed; diagnostics: {log}")
    representation = "assert(math.maxinteger == 2147483647 and 1.0 + 1e-8 == 1.0, 'expected ESP int32/float32 Lua')"
    subprocess.run([str(executable), "-e", representation,
                    str(PROJECT / "tests/test_fluid.lua")], cwd=REPO, check=True)
    print(f"Bundled Lua host validation passed{' with ASan/UBSan' if args.sanitize else ''}: {output}")


if __name__ == "__main__":
    main()

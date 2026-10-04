#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../../../../.." && pwd)
OUT=${1:-"$ROOT/tmp/botux-orb-motion"}
mkdir -p "$OUT"

c++ -std=c++11 -Wall -Wextra -Werror \
  -I"$HERE/include" -I"$HERE/../../src" \
  "$HERE/orb_motion_test.cpp" "$HERE/../../src/BotUx.cpp" \
  -o "$OUT/orb-motion-test"
"$OUT/orb-motion-test" "$OUT"

#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../../../../.." && pwd)
OUT=${1:-"$ROOT/tmp/botux-preview"}
mkdir -p "$OUT"

c++ -std=c++11 -Wall -Wextra -Werror \
  -I"$HERE/include" -I"$HERE/../../src" \
  "$HERE/preview.cpp" "$HERE/../../src/BotUx.cpp" \
  -o "$OUT/botux-preview"
"$OUT/botux-preview" "$OUT"

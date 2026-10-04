#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
PROJECT="$ROOT/projects/m5stack-stopwatch/bot-ux-watch"
OUT="$ROOT/tmp/watch-lvgl-host"
LVGL="$PROJECT/.pio/libdeps/m5stack-stopwatch/lvgl"
mkdir -p "$OUT"
# pio run installs the pinned LVGL release; native tests compile that same code.
cmake -S "$LVGL" -B "$OUT/build" \
  -DLV_CONF_PATH="$PROJECT/include/lv_conf.h" \
  -DCMAKE_C_FLAGS="-I$PROJECT/test/support/lvgl" \
  -DCMAKE_CXX_FLAGS="-I$PROJECT/test/support/lvgl" \
  -DLV_CONF_INCLUDE_SIMPLE=ON > "$OUT/configure.log" 2>&1
cmake --build "$OUT/build" --target lvgl -j 8 > "$OUT/build.log" 2>&1
c++ -std=c++17 -Wall -Wextra \
  -I"$PROJECT/test/support/lvgl" -I"$PROJECT/include" -I"$LVGL" \
  -I"$ROOT/projects/shared-libs/ux-components/src" -DLV_CONF_INCLUDE_SIMPLE \
  "$PROJECT/src/WatchLvgl.cpp" "$PROJECT/src/WatchLvglFont.cpp" "$PROJECT/test/test_lvgl_settings.cpp" \
  "$ROOT/projects/shared-libs/ux-components/src/FontLatin24.cpp" \
  "$ROOT/projects/shared-libs/ux-components/src/FontCjk24.cpp" \
  "$OUT/build/lib/liblvgl.a" -o "$OUT/test_lvgl_settings"
"$OUT/test_lvgl_settings" "$OUT/settings.ppm" "$OUT/editor.ppm" "$OUT/name.ppm"
c++ -std=c++11 -Wall -Wextra -Werror -I"$PROJECT/include" \
  "$PROJECT/test/test_corallium_time.cpp" -o "$OUT/test_corallium_time"
"$OUT/test_corallium_time"
echo 'PASS Corallium RTC time conversion'

c++ -std=c++11 -Wall -Wextra -Werror -I"$PROJECT/include" \
  -I"$PROJECT/.pio/libdeps/m5stack-stopwatch/ArduinoJson/src" \
  "$PROJECT/test/test_corallium_validation.cpp" -o "$OUT/test_corallium_validation"
"$OUT/test_corallium_validation"
echo 'PASS Corallium UTF-8 and trailing input validation'
c++ -std=c++11 -Wall -Wextra -Werror -I"$PROJECT/include" \
  "$PROJECT/test/test_corallium_clock_transaction.cpp" -o "$OUT/test_corallium_clock_transaction"
"$OUT/test_corallium_clock_transaction"
echo 'PASS Corallium RTC/NVS transaction failure boundaries'
c++ -std=c++11 -Wall -Wextra -Werror -I"$PROJECT/include" \
  "$PROJECT/test/test_corallium_event_gate.cpp" -o "$OUT/test_corallium_event_gate"
"$OUT/test_corallium_event_gate"
echo 'PASS Corallium BLE handshake event gate and reconnect cadence'

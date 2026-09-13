#!/bin/sh
# Run platform-independent contracts; artifacts stay in ignored tmp/.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
OUT="$ROOT/tmp/host-checks"
mkdir -p "$OUT"
c++ -std=c++11 -Wall -Wextra -Werror \
  -Istopwatch/bot-ux-watch/test/support/rtc -Istopwatch/bot-ux-watch/include \
  stopwatch/bot-ux-watch/test/test_rtc_clock.cpp stopwatch/bot-ux-watch/src/RtcClock.cpp \
  -o "$OUT/test_rtc_clock"
"$OUT/test_rtc_clock"
for name in test_calendar_math test_input_semantics test_timed_state test_power_policy test_watch_interaction test_companion_controls test_companion_presets test_ui_controls test_touch_affine test_touch_contact test_touch_trace_buffer test_watch_strings test_watch_button_feedback test_watch_button_feedback_masks test_watch_edge_geometry test_watch_feedback_patch; do
  c++ -std=c++11 -Wall -Wextra -Werror -Istopwatch/bot-ux-watch/include -Ilib/ux-components/src \
    "stopwatch/bot-ux-watch/test/$name.cpp" -o "$OUT/$name"
  "$OUT/$name"
  echo "PASS $name"
done
c++ -std=c++11 -Wall -Wextra -Werror \
  -Istopwatch/bot-ux-watch/test/support -Istopwatch/bot-ux-watch/include -Ilib/bot-ux/src \
  stopwatch/bot-ux-watch/test/test_settings_timeouts.cpp stopwatch/bot-ux-watch/src/Settings.cpp \
  -o "$OUT/test_settings_timeouts"
"$OUT/test_settings_timeouts"
echo 'PASS test_settings_timeouts'
c++ -std=c++11 -O2 -Wall -Wextra -Werror -Istopwatch/bot-ux-watch/include \
  stopwatch/bot-ux-watch/tools/generate_button_feedback_masks.cpp \
  -o "$OUT/generate_button_feedback_masks"
"$OUT/generate_button_feedback_masks" "$OUT/WatchButtonFeedbackMasks.generated.h"
cmp "$OUT/WatchButtonFeedbackMasks.generated.h" \
  stopwatch/bot-ux-watch/include/WatchButtonFeedbackMasks.generated.h
echo 'PASS generated button-feedback masks'
c++ -std=c++11 -Wall -Wextra -Werror -Istopwatch/bot-ux-watch/include -Ilib/ux-components/src \
  stopwatch/bot-ux-watch/test/test_watch_typography.cpp \
  lib/ux-components/src/FontLatin18.cpp lib/ux-components/src/FontLatin24.cpp \
  lib/ux-components/src/FontLatin28.cpp \
  lib/ux-components/src/FontCjk18.cpp \
  lib/ux-components/src/FontCjk22.cpp lib/ux-components/src/FontCjk24.cpp \
  lib/ux-components/src/FontCjk28.cpp -o "$OUT/test_watch_typography"
"$OUT/test_watch_typography" "$OUT/watch-typography.ppm"
echo 'PASS test_watch_typography'
for name in hid_framing analog_input battery_double_tap agent_signal bottom_led_frame feedback_level fresh_reply_attention; do
  case "$name" in
    hid_framing) source=core2/bot-ux-codex-core2/src/HidFraming.cpp ;;
    analog_input) source=core2/bot-ux-codex-core2/src/AnalogInput.cpp ;;
    *) source= ;;
  esac
  c++ -std=c++11 -Wall -Wextra -Werror -Icore2/bot-ux-codex-core2/include \
    $source "core2/bot-ux-codex-core2/test/${name}_test.cpp" -o "$OUT/$name"
  "$OUT/$name"
  echo "PASS $name"
done
c++ -std=c++11 -Wall -Wextra -Werror -Ilib/ux-components/src \
  lib/ux-components/test/components_test.cpp lib/ux-components/src/Font*.cpp -o "$OUT/ux-components"
"$OUT/ux-components" "$OUT/ux-components.ppm"
echo 'PASS ux-components'
c++ -std=c++11 -Wall -Wextra -Werror -Ilib/ux-components/src lib/ux-components/test/pointer_test.cpp -o "$OUT/pointer"
"$OUT/pointer"
echo 'PASS pointer'
mkdir -p "$OUT/sound"
c++ -std=c++11 -O2 -Wall -Wextra -Werror -Ilib/ux-components/src \
  lib/ux-components/test/sound/sound_test.cpp lib/ux-components/src/UxSound.cpp \
  lib/ux-components/src/UxSoundPcm.cpp -o "$OUT/sound/test"
"$OUT/sound/test" "$OUT/sound"
echo 'PASS sound'
c++ -std=c++11 -O2 -Wall -Wextra -Werror -pthread -Ilib/ux-components/src \
  lib/ux-components/test/sound/sender_test.cpp -o "$OUT/sound/sender"
"$OUT/sound/sender"
echo 'PASS sound-sender'
sh lib/bot-ux/tools/host-preview/render.sh "$OUT/bot-preview"
echo 'PASS bot-preview'
sh lib/bot-ux/tools/host-preview/test-orb-motion.sh "$OUT/bot-orb-motion"
echo 'PASS bot-orb-motion'

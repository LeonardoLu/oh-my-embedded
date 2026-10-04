#!/bin/sh
# Run platform-independent contracts; artifacts stay in ignored tmp/.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
OUT="$ROOT/tmp/host-checks"
mkdir -p "$OUT"
c++ -std=c++11 -Wall -Wextra -Werror \
  -Iprojects/m5stack-stopwatch/bot-ux-watch/test/support/rtc -Iprojects/m5stack-stopwatch/bot-ux-watch/include \
  projects/m5stack-stopwatch/bot-ux-watch/test/test_rtc_clock.cpp projects/m5stack-stopwatch/bot-ux-watch/src/RtcClock.cpp \
  -o "$OUT/test_rtc_clock"
"$OUT/test_rtc_clock"
for name in test_calendar_math test_input_semantics test_timed_state test_power_policy test_watch_interaction test_companion_controls test_companion_presets test_ui_controls test_touch_affine test_touch_contact test_touch_trace_buffer test_watch_strings test_watch_button_feedback test_watch_button_feedback_masks test_watch_edge_geometry test_watch_feedback_patch; do
  c++ -std=c++11 -Wall -Wextra -Werror -Iprojects/m5stack-stopwatch/bot-ux-watch/include -Iprojects/shared-libs/ux-components/src \
    "projects/m5stack-stopwatch/bot-ux-watch/test/$name.cpp" -o "$OUT/$name"
  "$OUT/$name"
  echo "PASS $name"
done
c++ -std=c++11 -Wall -Wextra -Werror \
  -Iprojects/m5stack-stopwatch/bot-ux-watch/test/support -Iprojects/m5stack-stopwatch/bot-ux-watch/include -Iprojects/shared-libs/bot-ux/src \
  projects/m5stack-stopwatch/bot-ux-watch/test/test_settings_timeouts.cpp projects/m5stack-stopwatch/bot-ux-watch/src/Settings.cpp \
  -o "$OUT/test_settings_timeouts"
"$OUT/test_settings_timeouts"
echo 'PASS test_settings_timeouts'
c++ -std=c++11 -O2 -Wall -Wextra -Werror -Iprojects/m5stack-stopwatch/bot-ux-watch/include \
  projects/m5stack-stopwatch/bot-ux-watch/tools/generate_button_feedback_masks.cpp \
  -o "$OUT/generate_button_feedback_masks"
"$OUT/generate_button_feedback_masks" "$OUT/WatchButtonFeedbackMasks.generated.h"
cmp "$OUT/WatchButtonFeedbackMasks.generated.h" \
  projects/m5stack-stopwatch/bot-ux-watch/include/WatchButtonFeedbackMasks.generated.h
echo 'PASS generated button-feedback masks'
c++ -std=c++11 -Wall -Wextra -Werror -Iprojects/m5stack-stopwatch/bot-ux-watch/include -Iprojects/shared-libs/ux-components/src \
  projects/m5stack-stopwatch/bot-ux-watch/test/test_watch_typography.cpp \
  projects/shared-libs/ux-components/src/FontLatin18.cpp projects/shared-libs/ux-components/src/FontLatin24.cpp \
  projects/shared-libs/ux-components/src/FontLatin28.cpp \
  projects/shared-libs/ux-components/src/FontCjk18.cpp \
  projects/shared-libs/ux-components/src/FontCjk22.cpp projects/shared-libs/ux-components/src/FontCjk24.cpp \
  projects/shared-libs/ux-components/src/FontCjk28.cpp -o "$OUT/test_watch_typography"
"$OUT/test_watch_typography" "$OUT/watch-typography.ppm"
echo 'PASS test_watch_typography'
c++ -std=c++11 -Wall -Wextra -Werror -Iprojects/m5stack-stopwatch/bot-ux-watch/include \
  projects/m5stack-stopwatch/bot-ux-watch/test/test_settings_icon_assets.cpp \
  -o "$OUT/test_settings_icon_assets"
"$OUT/test_settings_icon_assets" "$OUT/settings-icons.ppm"
echo 'PASS test_settings_icon_assets'
mkdir -p "$OUT/settings-carousel"
c++ -std=c++11 -Wall -Wextra -Werror \
  -Iprojects/m5stack-stopwatch/bot-ux-watch/include -Iprojects/shared-libs/ux-components/src \
  projects/m5stack-stopwatch/bot-ux-watch/test/test_settings_carousel_render.cpp \
  projects/shared-libs/ux-components/src/FontLatin24.cpp projects/shared-libs/ux-components/src/FontLatin28.cpp \
  projects/shared-libs/ux-components/src/FontCjk24.cpp projects/shared-libs/ux-components/src/FontCjk28.cpp \
  -o "$OUT/test_settings_carousel_render"
"$OUT/test_settings_carousel_render" "$OUT/settings-carousel"
echo 'PASS test_settings_carousel_render'
for name in hid_framing analog_input battery_double_tap agent_signal bottom_led_frame feedback_level fresh_reply_attention; do
  case "$name" in
    hid_framing) source=projects/m5stack-core2/bot-ux-codex-core2/src/HidFraming.cpp ;;
    analog_input) source=projects/m5stack-core2/bot-ux-codex-core2/src/AnalogInput.cpp ;;
    *) source= ;;
  esac
  c++ -std=c++11 -Wall -Wextra -Werror -Iprojects/m5stack-core2/bot-ux-codex-core2/include \
    $source "projects/m5stack-core2/bot-ux-codex-core2/test/${name}_test.cpp" -o "$OUT/$name"
  "$OUT/$name"
  echo "PASS $name"
done
c++ -std=c++11 -Wall -Wextra -Werror -Iprojects/shared-libs/ux-components/src \
  projects/shared-libs/ux-components/test/components_test.cpp projects/shared-libs/ux-components/src/Font*.cpp -o "$OUT/ux-components"
"$OUT/ux-components" "$OUT/ux-components.ppm"
echo 'PASS ux-components'
c++ -std=c++11 -Wall -Wextra -Werror -Iprojects/shared-libs/ux-components/src projects/shared-libs/ux-components/test/pointer_test.cpp -o "$OUT/pointer"
"$OUT/pointer"
echo 'PASS pointer'
mkdir -p "$OUT/sound"
c++ -std=c++11 -O2 -Wall -Wextra -Werror -Iprojects/shared-libs/ux-components/src \
  projects/shared-libs/ux-components/test/sound/sound_test.cpp projects/shared-libs/ux-components/src/UxSound.cpp \
  projects/shared-libs/ux-components/src/UxSoundPcm.cpp -o "$OUT/sound/test"
"$OUT/sound/test" "$OUT/sound"
echo 'PASS sound'
c++ -std=c++11 -O2 -Wall -Wextra -Werror -pthread -Iprojects/shared-libs/ux-components/src \
  projects/shared-libs/ux-components/test/sound/sender_test.cpp -o "$OUT/sound/sender"
"$OUT/sound/sender"
echo 'PASS sound-sender'
sh projects/shared-libs/bot-ux/tools/host-preview/render.sh "$OUT/bot-preview"
echo 'PASS bot-preview'
sh projects/shared-libs/bot-ux/tools/host-preview/test-orb-motion.sh "$OUT/bot-orb-motion"
echo 'PASS bot-orb-motion'

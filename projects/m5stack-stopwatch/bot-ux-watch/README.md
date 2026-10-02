# bot-ux-watch — StopWatch 陪伴时钟

RTC 时钟、日期、电量/充电、Bot 陪伴及触摸设置。没有秒表计时功能。

[设备资料](../README.md) · [硬件约束](../specs/hardware.md) ·
[项目规格与验收](specs/README.md) · [开发约束](AGENTS.md)

## 构建与验证

以下命令从仓库根目录运行。依赖版本以本工程 `platformio.ini` 为准；
共享库通过 `lib_extra_dirs = ../../shared-libs` 发现。

```sh
pio run -d projects/m5stack-stopwatch/bot-ux-watch
sh tools/check_host.sh
sh projects/m5stack-stopwatch/bot-ux-watch/tools/check_lvgl.sh
```

安装到设备时，先用 `pio device list` 确认端口，再执行：

```sh
pio run -d projects/m5stack-stopwatch/bot-ux-watch -t upload --upload-port <StopWatch-port>
```

校时工具：`python3 projects/m5stack-stopwatch/bot-ux-watch/tools/sync_rtc.py --help`，
使用安装了 `pyserial` 的 Python 环境（例如 PlatformIO 的 Python 环境）。

主机检查不等同于实机触摸、声学或灯效验收；各项行为与验证边界见项目 specs。

## 功能与操作

Press A to choose a non-repeating random mood from the complete shared set. B
advances to the next mood; double-click B within 320 ms to return to automatic
Idle. A face touch temporarily looks straight ahead when inside the bot and
toward the contact when outside it, then returns to the selected mood. Hold A+B
together for three seconds to open settings. Bot personalization is available
from the Bot page in Settings. Hold B to dim the face;
the next touch/button press wakes it without activating a control.

The watch samples raw display contacts every 8 ms. The face retains its contact
and gesture state machine; every settings screen uses LVGL 8.4 native pointer
input, vertical scrolling, buttons, sliders and text entry. A drag never also
activates a row. The Time, Bot, Display, Sound, Power and Connection entries share
a vertical card list. Editors keep fixed Cancel/Done buttons outside the list and
retain live Bot previews. A/B remain shortcuts; Home cancels unsaved edits.

Swipe down from the top edge for battery/charging, or double-tap the visible time
for settings. Connection → Bluetooth explicitly opens a five-minute BLE window;
the Bluetooth name is StopWatch. Connection also shows status and the remaining
window, with a separate protocol information page.
The macOS/iOS app can synchronize the RTC and read battery/status using
[Corallium v1](../../../protocols/corallium-v1/README.md). Serial `ble on`/`ble off`
are diagnostic equivalents. This ESP32-S3/SDK combination does not provide the
required Apple-compatible Wi-Fi Aware, so this firmware has no Wi-Fi functionality.
See [connection behavior and limitations](specs/connection.md).

Time and `yyyy/mm/dd {weekday}` share one region. A hideable Bot description
occupies the opposite region; Layout swaps the two. Time/date use the RTC;
preferences use NVS. Auto starts with Idle for 5–15 seconds, then LookingAround for 30–60 seconds,
then a safe ambient mood for 5–15 seconds. Interaction restarts or delays this
sequence according to the current ambient specification. Filtered wrist motion adds subtle movement with a cooldown
on shake reactions.

The companion is **Milo** by default. Both devices support a 16-character name,
English/Chinese UI and descriptions, and independent live selectors for all
14 moods × 10 expressions × 8 animations (1,120 combinations). Preview choices do
not overwrite the persistent expression/action or Core2 host state. Gaze offers
Auto plus nine explicit directions: Center, Left, Right, Up, Down and the four
diagonals. Temporary screen-directed gaze returns to that preference; normal
presets retain continuous motion. Idle rests at the canonical upper-right pose;
LookingAround explores the full gaze field, and FaceSide controls automatic or
fixed left/right mirroring. Watch contact and gaze validation is tracked in
[the current gaze specification](specs/companion.md) and
[verification methods and limits](../../shared-libs/specs/verification.md).

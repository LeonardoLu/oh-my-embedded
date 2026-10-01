# bot-ux-codex-core2 — Codex Micro BLE 控制器

在 Core2 + Battery Bottom2 上实现触摸命令面板，通过兼容的 BLE HID vendor report 与 Codex 桌面应用通信。

[设备资料](../README.md) · [硬件约束](../specs/hardware.md) ·
[项目规格与验收](specs/README.md) · [开发约束](AGENTS.md)

## 构建与验证

以下命令从仓库根目录运行。依赖版本以本工程 `platformio.ini` 为准；
共享库通过 `lib_extra_dirs = ../../shared-libs` 发现。

```sh
pio run -d projects/m5stack-core2/bot-ux-codex-core2
sh tools/check_host.sh
```

安装到设备时，先用 `pio device list` 确认端口，再执行：

```sh
pio run -d projects/m5stack-core2/bot-ux-codex-core2 -t upload --upload-port <Core2-port>
```

主机检查不等同于实机触摸、声学或灯效验收；历史证据与限制见项目 specs。

## 功能与操作

The 320×240 command deck exposes six agent keys, Fast/Approve/Decline/Fork/Mic/Send,
encoder navigation, encoder press, and a touch joystick. These are the installed
Codex app's default mappings; the app can remap them. Pair **Core2 Codex Micro** in
macOS Bluetooth settings, then enable/connect Micro in Codex. Core2's USB connector
is a serial bridge, so host HID communication uses Bluetooth.

On macOS, Codex needs Input Monitoring permission to open this composite HID
interface. After enabling it in Privacy & Security, fully quit and reopen Codex.
If the device is paired but stays at `BLE`, check this permission and restart;
the green app-ready icon indicates that the RPC transport has initialized.

The header uses separate Bluetooth and app-ready icons to distinguish pairing
from an initialized RPC transport. Status glyphs are right-aligned, with the
battery number inside its icon. Agent cards use the exact host color, a corner
number and the corresponding lighting status. The selected Bot follows that
slot. Blue means Working, orange Needs input, green New reply, red Error and
white Idle. Green does not establish completion; unknown signals stay Unknown.
A fresh Working → New reply transition adds an attention highlight for up to
30 seconds. Screen or button interaction dismisses it; persistent green does
not restart the window. Authoritative host colors and states remain intact.
Meaningful transitions trigger a bounded highlight and optional sound, with silent
initial/reconnect baselines. See [the signal contract](specs/agent-signal-contract.md). End-to-end connection
validation is tracked in [the iteration record](specs/interaction-validation.md).

Holding MIC sends push-to-talk press/release events; the Mac captures the audio.
Releasing or dragging off stops the hold, and the host decides whether to submit.
Other buttons activate on release in the original target. The bottom left/right
halves switch pages around a compact page count. Double-tap the battery to open
settings for brightness, audio, theme, animation, motion, reduced motion and LED
brightness. The Lights page offers Off, Host and Alive modes plus transition
notifications. Alive preserves host hues with gentle breathing, a traveling
highlight and selection feedback; Reduced Motion keeps it static. Bot
personalization includes naming, language, complete combination preview and live RGB sliders for body, eyes and
accent; valid releases save in NVS and dragging outside cancels the color edit.

Bottom2 uses ten SK6812 LEDs on GPIO25 and replaces the stock Core2 bottom. Screen,
LEDs and optional sound provide feedback. Official Micro firmware updates are
unsupported because Core2 is different hardware.

Both devices share 16 synthesized cues with six timbres, envelopes, glides and
scales; optional signed/unsigned 8-bit PCM is a separate product. A fixed-buffer
sender isolates the SDK playback queue from the UI loop. Sound preferences apply
to new cues, and queued audio drains smoothly. See [the sound guide](../../shared-libs/ux-components/SOUND.md).

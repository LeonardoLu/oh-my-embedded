# Core2 硬件与装配

设备数量、准确版本以及下列装配是否使用同一台主机，待用户补充。
Codex 目标标注的版本与 PPS 检测到的 PMIC 不足以证明主机身份相同，按装配分别描述。

## Codex 控制器装配

- 工程目标：M5Stack Core2 v1.3，320×240 横屏，ILI9342C 与 FT6336U 触控。
- M5GO Battery Bottom2 v1.3 替换原底座，GPIO25 控制 10 颗 SK6812，GRB 顺序。
- 该装配不使用原底座振动马达，反馈来自屏幕、扬声器和侧边灯。
- USB 用于串口；Codex HID 使用 BLE。连接与应用握手规则见
  [控制器规格](../bot-ux-codex-core2/specs/README.md)。

## PPS 装配

- 适用装配：Core2 + Module13.2 PPS + Base Bottom v1.1。
- 此装配检测结果：ESP32-D0WDQ6-V3 revision v3.1，16 MB Flash，AXP192。
- 内部 I2C SDA21 / SCL22，PPS 地址 `0x35`，PMIC `0x34`，触控 `0x38`，RTC `0x51`。
- PPS DC 输入经独立隔离辅助电源供给 M-Bus；演示配置关闭主机总线升压和充电。
- PPS 辅助供电与香蕉可调输出是两条用途不同的通路；详细供电条件、
  实测范围和冷启动结论见 [PPS README](../pps/README.md) 与
  [供电与输出规格](../pps/specs/power-and-output.md)。

不能从某个工程的成功运行推断另一种底座、负载或供电配置已经验收。

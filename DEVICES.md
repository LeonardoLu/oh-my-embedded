# 设备清单

本清单先根据已有工程、实机记录和本次提到的设备建立。数量、购入信息、
序列号及尚未确认的硬件版本之后补充；同一型号下的不同装配不代表多台设备。

## 主设备

| 品牌 | 型号 | 已知信息 | 工程与记录 |
| --- | --- | --- | --- |
| M5Stack | StopWatch | 已有实机记录；ESP32-S3R8，16 MB Flash / 8 MB PSRAM，466×466 圆形 AMOLED，触摸、A/B 按钮、RTC | [设备](projects/m5stack-stopwatch/README.md) · [陪伴时钟](projects/m5stack-stopwatch/bot-ux-watch/README.md) |
| M5Stack | Core2 | 已有两个工程及不同装配记录；Codex 原始配置写为 v1.3，PPS 实测为 AXP192 / 16 MB Flash；是否同一台、实际版本和数量待补充 | [设备](projects/m5stack-core2/README.md) · [硬件记录](projects/m5stack-core2/specs/hardware.md) |
| Espressif（乐鑫） | ESP-Mosaico | 已建官方固件衍生工程；持有版本、模块、配置和实机验收待确认 | [设备](projects/espressif-esp-mosaico/README.md) · [官方硬件资料](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp-mosaico/index.html) |
| Metalio | ink-4（按本次提供名称） | 本次提到，工程待建；完整型号、硬件参数和持有配置待补充 | 后续目录可用 `projects/metalio-ink-4/`，建项前确认型号 |

## 配件与装配

| 品牌 | 配件 | 已知用途 | 依据 |
| --- | --- | --- | --- |
| M5Stack | M5GO Battery Bottom2 v1.3（Core2） | Codex 控制器底座；10 颗 SK6812，GPIO25；替换原底座后不使用原振动马达 | [Core2 硬件与装配](projects/m5stack-core2/specs/hardware.md) |
| M5Stack | Module13.2 PPS | Core2 可编程电源模块，内部 I2C 地址 `0x35`；已有 12V DC 空载测试记录 | [PPS 工程](projects/m5stack-core2/pps/README.md) |
| M5Stack | Base Bottom v1.1 | PPS 装配中的电池/引脚扩展底座，与 Battery Bottom2 区分 | [PPS 供电与输出](projects/m5stack-core2/pps/specs/power-and-output.md) |

## 后续补充

每台设备可补充：准确型号与版本、数量/编号、Flash/PSRAM、屏幕、输入、
电源/电池、扩展模块、官方资料、使用状态和工程用途。
串口路径属于连接环境，具体值保留在测试记录中，不用作设备永久编号。

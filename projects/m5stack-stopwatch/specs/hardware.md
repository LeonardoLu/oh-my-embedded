# StopWatch 硬件与平台

从现有工程约束整理；设备数量和板卡版本待清单补充。

| 项目 | 当前工程使用的硬件/配置 |
| --- | --- |
| SoC / 内存 | ESP32-S3R8，16 MB Flash，8 MB PSRAM |
| 显示 | 1.75 英寸圆形 AMOLED，466×466，CO5300 / QSPI |
| 输入 | CST820B 电容触摸；可编程 A/B 按钮；电源按钮 |
| 电源 | M5PM1；VBAT 滤波推算电量，VIN 与低有效 GPIO2 联合判断充电 |
| 声音 | ES8311 codec、1W 扬声器 |
| RTC / 运动 | RX8130CE（M5.Rtc）、BMI270 |
| PlatformIO | `esp32s3box`，`qio_opi`，16 MB 分区；具体引脚与补丁由工程管理 |

当前适配见 [platformio.ini](../bot-ux-watch/platformio.ini) 与
[补丁工具](../bot-ux-watch/tools/patch_m5gfx.py)。电源寄存器、RTC 连续性和
显示休眠的实现与验收在 [工程规格](../bot-ux-watch/specs/README.md)，
不得仅由控制器型号推断实机行为。

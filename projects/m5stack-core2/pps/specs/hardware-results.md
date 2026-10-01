# 实机记录 - 2026-09-30（Asia/Shanghai）

## 设备与固件

- 用户装配：Core2 + Module13.2 PPS + Base Bottom v1.1，香蕉输出端空载。
- 用户把 DC 电源从 6V 换为 12V；PPS 实际回读约 12.232-12.246V。
- 串口：`/dev/cu.usbserial-5B1F0084761`，CH9102 / CH34x VID:PID `1A86:55D4`。
- esptool 检测：ESP32-D0WDQ6-V3，revision v3.1，16MB Flash。
- M5Unified 检测：`board=2`（Core2），`pmic=2`（AXP192），SDA 21 / SCL 22。
- PPS 地址 `0x35`，模块 ID `0x1041`（4161）。
- 最终 I2C 扫描：`0x34`（PMIC）、`0x35`（PPS）、`0x38`（触控）、`0x51`（RTC）。
- 来源是官方 PPS UserDemo，加入本项目 README 列出的适配和串口诊断。
- 编译通过；PlatformIO 串口写入完成，esptool 确认每段数据 hash 验证成功。
- 本次只烧录 Core2 ESP32。PPS STM32 固件及出厂校准没有写入。

## 空载可变输出

每档设置限流 0.100A，开启后等待 1.5 秒，再读取三次 PPS 内部输出回读。
以下数字不是外部万用表测量值。

| 顺序 | 电压设定 | 平均电压回读 | 相对设定偏差 | 模式 |
|---|---:|---:|---:|---|
| 1 | 1.000V | 0.9990V | -1.0mV | CV |
| 2 | 3.300V | 3.2980V | -2.0mV | CV |
| 3 | 5.000V | 4.9973V | -2.7mV | CV |
| 4 | 8.000V | 7.9933V | -6.7mV | CV |
| 5 | 5.000V | 4.9987V | -1.3mV | CV |
| 6 | 3.300V | 3.3017V | +1.7mV | CV |
| 7 | 1.000V | 1.0020V | +2.0mV | CV |

所有档位的电压与限流设定均正确回读；输出电流回读约 -0.001 到 +0.001A，
符合空载状态。PPS MCU 温度约 35-36°C。脚本验证升、降设定均正常。

结束时确认 `enabled=0, mode=0`；等待 1 秒后输出电压回读 -0.022V，
该残余偏差在官方回读精度范围内。设定保留在 1.000V / 0.100A，输出关闭。

未测试：真实负载输出、CC 限流、满载功率、外部仪表精度、长期温升。

最终固件复核：启动为关闭状态；未设置有效 V/I 时拒绝 `ON`，拒绝
`SET nan 0.1`；3.300V / 0.100A 设定回读正确，输出回读 3.298V；
再次关闭输出并恢复 1.000V / 0.100A 设定。

## 单根供电线

已确认的硬件资料：PPS 的 DC 输入经固定 5V 降压和 B0505S-1WR3 隔离
DC/DC 接到 M-Bus pin 28。该辅助通路额定约 5V / 200mA，独立于香蕉输出。
Core2 官方 SDK 支持 M-Bus 输入供电；本 demo 设置 `bus_boost=0`，关闭充电，
亮度 96/255。在 USB 连接时测得 VBUS 约 5.341-5.344V。

**USB 连接时的读数不能单独证明去掉 USB 后可稳定运行。**
底座电池电压读取约 0V（偶有 0.130V 无电池 ADC 残值），未检测到可用电池供电。

用户执行了以下实物测试并确认结果：

1. 将 Bottom 电池开关置 OFF，拔掉 Core2 USB，仅保留 PPS 12V 输入，
   观察 30 秒后设备持续运行。
2. 关闭并重新打开 PPS 的 DC 输入，仅接 DC 能冷启动。

用户回复为“持续运行，且仅接 DC 能冷启动”。因此，当前固件配置已确认可以
只保留 PPS 的 DC 供电线。串口断开阶段的屏幕与启动结果由用户现场确认，
未通过电脑连续采样。未测试高亮度、Wi-Fi、扬声器大负载及边运行边充电。

## 右上角 ON/OFF 标志优化

- 待机页与设置页共用常亮状态标志，填满右上角缺口、移除黑色内边，文字居中。
  删除闪烁定时器与空白状态图；仅在输出状态变化时刷新，进入页面立即同步。
- 使用项目同版本 LVGL 8.3.10 / RGB565 在电脑上绘制 OFF、ON、再 OFF，
  检查缺口斜边、填充和文字布局。本次没有开启香蕉输出来检查 ON 外观。
- PlatformIO 编译与烧录成功，esptool 写入 hash 验证通过。实机启动日志连续
  四次确认 `ready=true, enabled=0, mode=0, bus_boost=0`，输入回读约 12.21V。
- 此次 `firmware.bin` 为 1,480,720 字节，SHA-256：
  `3795b2cc3475a88cc81497b429819a773241963a764dc707672c79290ba03a75`。

## 备份与原始日志

临时文件均在仓库 gitignored `tmp/pps/`：

- `core2-before-pps-2mb.bin`：原 Flash 前 2MiB，覆盖本次改写范围，
  不是整片 16MiB 备份。esptool 读出时完成数据摘要校验。
  SHA-256：`5c10b4a623db0b2e03ae6cc2707a1863a876eed1dcd23a65fd161a28857780d4`。
- `official-ui-core2.bin`：仅适配构建配置时编译的官方界面版本。
- `build.log`、`upload.log`、`upload-retry.log`、`boot-and-probe.log`、
  `output-test.log`、`final-console-check.log`：原始日志。
- `badge-build.log`、`badge-upload.log`、`badge-before-upload.log`、
  `badge-boot-check.log`：状态标志优化的编译、烧录与串口日志；
  `badge-preview/` 为 LVGL 软件绘制预览。

原分区布局与当前 `default_16MB.csv` 一致：app0 `0x10000 / 0x640000`，
app1 `0x650000 / 0x640000`，SPIFFS `0xc90000 / 0x360000`。
921600 / 460800 波特率曾出现传输错误，最终使用 115200 完成备份和烧录。
首次适配版本的最后一次烧录曾中断，重试后写入及 hash 验证成功，随后完成串口复核。
该版 `firmware.bin`：1,483,904 字节，SHA-256：
`5f5dfe2709c1dd174b1f1e4e6952dcff050ee195fd659476e3587aaacc5bdbc8`。

若要恢复烧录前被覆盖的内容，在仓库根目录运行：

```sh
/Users/leonardo.lu/.platformio/penv/bin/python \
  /Users/leonardo.lu/.platformio/packages/tool-esptoolpy/esptool.py \
  --chip esp32 --port /dev/cu.usbserial-5B1F0084761 --baud 115200 \
  write_flash 0 tmp/pps/core2-before-pps-2mb.bin
```

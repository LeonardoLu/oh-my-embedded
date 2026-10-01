# 时钟来源与连续性

RX8130CE 是唯一日历时间来源。固件不以编译时间自动校时，也不通过帧计数推算日历。
panel sleep、light sleep 和页面切换不写 RTC；恢复显示时重新读取，避免把冻结画面当成时间停止。

## 有效性与错误处理

[RtcClock.cpp](../src/RtcClock.cpp) 用一次七字节事务读取日历，前后检查 VLF。
BCD、星期、秒、月份天数、闰年及 2020–2099 年范围均需合法。
总线读失败保留最后有效快照并继续重试；快照不自行走时。首次发现失败最多每秒重试一次。
日历无效或 VLF 置位时快照失效，界面显示横线，不制造一个看似有效的时间。

区分三种状态的原因是：有效快照可以用于解释读失败前的显示，但只有最新 Ready 读数
能驱动 [强制息屏](power.md) 等基于当前时刻的决策。

## 编辑与校准

无效时钟要求用户显式确认日期和时间，顺序不限。第一次 Done 转到另一编辑器，
两部分均确认前不写硬件；取消或 Home 丢弃确认草稿。
有效时钟只改其中一部分时，保留新读到的另一部分，避免跨午夜覆盖日期。

完整写入检查驱动返回值与日历回读，允许跨午夜最多一秒的自然推进。确认后用 `0xbd`
清 VLF，保留其余 write-zero-to-clear 标志，再次检查日历和标志。写失败保持编辑器并显示 Error。
`getVoltLow()` 检查的是 VBLF `0x80`，不能代替振荡有效性 VLF `0x02`。

## 保电决策

`Power::update()` 保持 M5PM1 `0x06` bit2 的 LDO_EN 和 `0x07` bit5 的 LDO hold，
以读改写保留 LED、充电及其他电源位，并周期性回读、失败重试。
RTC 的 VDD 来自 `3V3_L1`，VBAT 连接电容而非独立备用电池，因此关机保时需要保留 L1。
L1 同时供给 IMU，这是保时与关机功耗之间的取舍；电池耗尽或断开仍可能丢失时间。
寄存器正确不等于已经测得实体双击关机后的保时持续时间或续航。

## 诊断与检查

- `rtc` 返回读取结果、快照有效性、状态、标志、日历与保电状态。
- `rtc set YYYY-MM-DDTHH:MM:SS` 显式设定本地时间，仅验证写入成功返回 `RTC set=1`。
- [sync_rtc.py](../tools/sync_rtc.py) 发送主机本地时间并读取偏差；固件不做时区转换。
  Keys-only 息屏期间串口不可依赖，先用相应实体键唤醒再校时。
- `test_rtc_clock` 覆盖 VLF、BCD、读取/写入失败、两种确认顺序、取消、闰日与跨午夜。
  `test_power_policy` 覆盖保电位读改写及重试。

硬件依据：[StopWatch 原理图](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1242/C152-SCH_Stopwatch_PRJ_Main_VA_20251201_2026_04_24_17_46_22.pdf)、
[RX8130CE 寄存器说明](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1132/RX8130CE_cn-Register-Datasheet.pdf)。

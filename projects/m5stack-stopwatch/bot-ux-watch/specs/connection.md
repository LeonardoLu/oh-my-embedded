# 设备连接与协议

实现 [Corallium v1](../../../../protocols/corallium-v1/README.md) 的 BLE 与 USB 串口信道。
[CoralliumDevice.cpp](../src/CoralliumDevice.cpp) 在主循环执行 RTC 和 Preferences 操作；
NimBLE 回调仅收齐 UTF-8 JSONL 并投递有界队列。固定 2048 字节帧上限，溢出丢弃至 LF，
断连清除半帧与待发送数据，TX 默认 20 字节分片。不把调试文本发送到 BLE TX。

## 本地设置与连接窗口

日常设置入口名为 Connection / 连接，蓝牙广播名为 StopWatch。连接页提供原生蓝牙开关、
连接状态、剩余窗口时间和单独的协议说明入口；只有协议说明页列出 Corallium v1、
支持的 time.set/battery 能力与 ble/serial 信道。应用名称不作为设备日常 UI 名称。

用户明确开启蓝牙后开放五分钟窗口，默认不开广播；串口 `ble on` / `ble off` 是诊断入口。
开关即时生效，Done/返回只离开页面；到期停止广播并断连，需要再次开启。使用 NimBLE 1.4.3，
最多一个连接，RX 的 WRITE_ENC 权限要求加密链路。系统 Just Works 配对提供链路加密，
不提供抗主动中间人认证。窗口仅为近旁配置场景，不能视为远程身份验证。
禁用库的断连自动广播；主循环也停止关闭窗口后连接失败事件重新触发的广播。
关闭窗口立即停止协议通知、清空请求队列，并拒绝晚到的配对完成回调。
BLE 窗口活动期间暂停按键模式的 ESP32 light sleep，面板仍按原策略变暗/息屏。

## 能力边界

`device.info` 仅声明 `time.set`、`battery`，型号为 `m5stack-stopwatch`，稳定 ID 来自
ESP eFuse MAC，固件版本为 `bot-ux-watch/0.3.0`。
[Espressif wifi_aware 0.1.0 的官方支持表](https://components.espressif.com/components/espressif/wifi_aware/versions/0.1.0/readme?language=en)
列出 ESP32/S2/S31/C5/C61，没有本机 ESP32-S3；iOS 互通要求 IDF 6.1 及以上，而本工程
锁定 IDF 4.4.7。这是当前组件和 SDK 的支持边界，不推断芯片永久不能实现 NAN。
当前固件无法提供所要求的 Apple App 互通，因此没有 Wi-Fi 后端、配置/凭据存储、Wi-Fi UI 或
NTP 逻辑，不链接或初始化 Arduino WiFi；wifi.set / wifi.forget 返回 unsupported。
通用 DeviceStatus 保留 wifi 对象，其 state 固定为 disconnected，其余字段均为 null。
普通 Wi-Fi station 能力不能替代用户要求的 Wi-Fi Aware。

## 时间与状态

`time.set` 接收 UTC 毫秒及分钟偏移，用确定性日历转换写入本地 RX8130CE，复用已有写入/
回读验证。偏移写入 `corallium/utcOffset`。若本地时间编辑器正在打开，返回 busy，不覆盖草稿。
RTC 或偏移保存失败返回 internal。写 RTC 前先持久化 `utcValid=false`，写入 RTC/偏移
完成后才写 `utcValid=true`；任一步失败或掉电都不会在重启后把新 RTC 按旧偏移报告同步。
尚无偏移记录时不把已有本地日历猜成 UTC，协议 time.valid 为 false，首次 App 校时后
建立 UTC 对应关系。本地 RTC 只支持 2020–2099，UTC 合法但经偏移越出本地区间也拒绝。
状态回读同样校验换算后的 UTC；本地手动设定或自然走时超出协议范围时，返回
valid=false、source/quality=unset、unix_ms=null，不发布越界时间。

状态每五秒经已加密 BLE 发送，也可显式请求。uptime_ms 使用 ESP 64-bit 单调定时器，
不会在 32-bit millis 的 49 天边界归零。电池百分比、电压和充电来自 Power；未获得电压
时各项为 null。没有可靠电流计量，不宣称 power/runtime 能力，相关字段保持 null。
串口允许本地物理配置，不要求 BLE 窗口。

## 内存与验证

主循环栈显式为 16 KiB。完整请求使用实例固定缓冲，JSON 文档按请求/五秒状态事件在堆上
分配，不把多个 2 KiB 临时对象叠到现有 LVGL/SDK 调用栈。`ui` 输出
`STACK loop_min_free_bytes` 报告实际运行期间栈的最小剩余量。

PlatformIO 构建检查真实 ESP32、NimBLE、ArduinoJson 和 LVGL 集成。`tools/check_lvgl.sh`
包含 UTC/时区边界、闰日、2038 年之后、UTF-8、JSON 尾部输入及各 RTC/NVS 事务失败边界。
最终固件已在 StopWatch 上通过 USB JSONL 验证设备信息、时间、电池与不支持的 Wi-Fi
请求；原生屏幕抓取验证连接页、滚动和协议页。软件复位后观察到有效 RTC 时间保留。
最终版本的 Corallium mac app BLE 配对及往返验收仍待系统蓝牙授权，不能由串口或模拟
响应替代。RTC 本次复位连续性不等同于耗尽电池后的保时承诺；真实手指触摸和完整续航
仍是独立的硬件验证边界。

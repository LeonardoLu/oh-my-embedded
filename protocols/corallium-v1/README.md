# Corallium v1

## 信道与边界

| 信道 | 声明 | 用途与限制 |
| --- | --- | --- |
| BLE GATT | `ble` | macOS/iOS 共用，App 为 Central，设备为 Peripheral |
| USB 串口 | `serial` | 设备可选的本地诊断信道；当前 App 不以串口作为连接入口 |

BLE Service 为 `7D2A0001-7E6D-4C55-A8D2-6B6D5E8F9010`。
RX 为 `7D2A0002-7E6D-4C55-A8D2-6B6D5E8F9010`，支持 Write with response，
要求链路加密；TX 为 `7D2A0003-7E6D-4C55-A8D2-6B6D5E8F9010`，支持 Notify。
UUID 声明见 [channels.json](channels.json)。先订阅 TX，再发送 `device.info`。

每条消息是一行 UTF-8 JSON 对象，以 LF (`0x0a`) 结束，不含 BOM。长度上限为
2048 **字节**（不含 LF），JSON 字符串内部换行必须转义。传输可在任意字节
处分片，包括多字节字符内部；只有完整收齐一行后才解码 UTF-8。BLE packet
不是消息边界。固件 TX 默认 20 字节分片，App RX 必须累积；App 写入片段不超过
系统 `maximumWriteValueLength(for: .withResponse)`，每片收到写确认才继续。
超过长度上限后丢弃至下一个 LF，并恢复读取，不能把尾片误认为新消息。
断连清空收发队列、半行和在途请求；重新连接必须重新握手。

每个连接只允许一个请求在途，响应必须回显 `id` 与 `op`。`id` 是 1–64 个
ASCII 字符 `[A-Za-z0-9._-]`，在一个连接中不得重复。App 请求超时为 10 秒；
超时并不表示设备未执行，变更命令不自动重试。迟到响应不能被匹配给新请求。
无法解析、缺失有效 id 的输入可丢弃；能识别请求时应返回明确错误。

串口实现如同端口也输出诊断文本，协议行必须独立以 `{` 开头、以 LF 结束，
消费端只接受结构完整且 `v == 1` 的协议对象；日志不得插入 JSON 行内部。
BLE TX 上不得混入普通日志。

设备须遵守 `channels.json` 中各型号的本机 BLE 开关策略：StopWatch 通过物理设置
开启 300 秒连接窗口，重启后默认关闭；ESP-Mosaico 默认关闭，首次需用户本机开启，
之后持久保存开关并在重启时恢复。Mosaico 开启后没有时间窗口，未连接时持续可发现，
断连后重新广播，直到用户关闭；关闭立即停止协议交换、停广播并断链，恢复出厂也回到关闭。
开关保存失败不得显示已成功切换。两者均在本会话成功回复 `device.info` 后才开始
异步状态推送，已有绑定或恢复 CCCD 不代表新会话的 App 已准备收齐完整帧。

加密要求由 GATT 权限执行，不能依赖 App 的本地标记。当前允许系统的 Just Works
配对；这提供链路加密，但不提供抗主动中间人身份验证。支持长期绑定的固件仍须遵守
自身连接开关。Wi-Fi 密码不出现在响应、
状态、日志或提交的测试向量中；空密码表示开放网络。App 不持久保存密码。

## 消息

请求：`{"v":1,"id":"1","op":"device.info","payload":{}}`

成功：`{"v":1,"id":"1","op":"device.info","ok":true,"payload":{...}}`

失败：`{"v":1,"id":"1","op":"time.set","ok":false,"error":{"code":"invalid_request","message":"Invalid time"}}`

状态事件无 id、无 ok：`{"v":1,"op":"device.status","payload":{...}}`。
状态事件可穿插在响应间，不能结束在途请求。所有时间单位在字段名称中声明；
未知读数为 null。接收者忽略未知字段、未知能力和未知事件，未知请求 op 回复
`unsupported`，不支持的协议版本不执行变更。

错误码：`invalid_request`（结构、类型或范围错误）、`unsupported`（未实现操作）、
`busy`（操作冲突）、`internal`（保存/硬件失败）、`not_authorized`（本机连接开关、窗口或
安全条件未满足）。message 用于诊断，App 不能以字符串内容判断控制流程。

## 操作和结构

| 操作 | 请求 payload | 成功 payload | 能力 |
| --- | --- | --- | --- |
| `device.info` | `{}` | DeviceInfo | 所有设备必须支持 |
| `device.status` | `{}` | DeviceStatus | 所有设备必须支持 |
| `time.set` | `unix_ms`, `utc_offset_min` | TimeStatus | `time.set` |
| `wifi.set` | `ssid`, `password` | `{"state":"connecting"}` | `wifi.set` |
| `wifi.forget` | `{}` | `{"state":"disconnected"}` | `wifi.forget` |

DeviceInfo 包含 `device_id`（设备内稳定标识，不使用易变串口路径）、`model`、
`name`、`firmware`（版本字符串）、`capabilities`（字符串数组）和 `channels`
（字符串数组）。当前型号为 `m5stack-stopwatch`、`espressif-esp-mosaico`。
能力还包括 `battery`、`power`、`runtime`；后两者意味着可能提供对应读数，
不代表每种供电/采样状态都有有效值。

StopWatch 仅声明 `time.set`、`battery`，通过 BLE 校时和读取状态，不提供
普通 Wi-Fi 配网或 Wi-Fi Aware。ESP-Mosaico 提供 `wifi.set`、`wifi.forget`
以及可用的电池功率/续航能力。App 必须按能力显示操作，不能根据通用状态结构
推断设备支持 Wi-Fi。`session.jsonl` 展示 Mosaico 的完整配网会话，
`stopwatch-info.json` 展示 StopWatch 的能力边界。

DeviceStatus 包含 `time`、`wifi`、`battery` 和 `uptime_ms`（启动以来单调毫秒）。

| TimeStatus 字段 | 类型与含义 |
| --- | --- |
| `unix_ms` | UTC Unix 毫秒整数，未知为 null |
| `utc_offset_min` | 本地显示相对 UTC 分钟，-720…840 |
| `valid` | 当前时刻是否可信；false 时 App 不显示“已同步” |
| `source` | `rtc` / `network` / `app` / `unset` / `checkpoint` |
| `quality` | `synchronized` / `estimated` / `unset` |

time.set 只接受 UTC `1577836800000 <= unix_ms < 4102444800000`（2020–2099），
整数偏移。设备实际精度可为秒，回复实际写入值。StopWatch 硬件 RTC 现有存储为
本地日历时间，设备端负责偏移换算并保存偏移；App 一律发送 UTC。偏移是当前
固定值而不是时区规则，夏令时切换后需再次同步。RTC 没有有效值时不能用编译
时间或持久化旧时间冒充准确时间。断电时长未知的 checkpoint 只能标 estimated，
不能通过 valid=true 暗示连续走时。
持续供电且由 RTC 连续走时的时间可以 valid=true；SoC RTC 的未校准漂移可通过
quality=estimated 表达。App 必须同时尊重有效性与质量，不能把 estimated 标为
“刚刚同步”，也不能把暖重启保时推广成完全断电后仍可保时。

WiFiStatus 为 `{state, ssid, ip, rssi}`：state 为 `disconnected`、`connecting`、
`connected` 或 `failed`；其余三个未知时为 null，rssi 单位 dBm。`wifi.set`
SSID 长度为 1–32 UTF-8 字节；password 为 0 或 8–63 UTF-8 字节。v1 仅支持
开放或个人密码网络，不支持企业认证和 64 位 hex PSK。必须先验证再修改存储。
成功回复表示已接收异步连接请求；只有后续 status.connected 表示联网成功。
forget 清除设备存储的配置并断开 Wi-Fi，不改变 BLE 连接和已同步 RTC。
未声明 Wi-Fi 能力的设备保留该结构以兼容 v1，返回 `disconnected` 及三个 null；
App 隐藏其网络入口和状态卡。

BatteryStatus 为 `{percent, charging, millivolts, power_mw, runtime_min}`，每项
可为 null；percent 为 0–100，charging 为布尔，电压单位 mV。power_mw 表示
电池放电功率（非负数）；充电、外部供电或未有可靠电流时应为 null。runtime_min
是按当前负载估计的剩余分钟，不是承诺；充电或无法估计时为 null。App 应展示
“估算”与未知读数，不能用 0 分钟或 0 mW 代替缺测。

## 验证资料

- [JSON Schema](message.schema.json) 描述消息和数据结构。
- [examples](examples/) 是虚构设备的互通向量；SSID/password 不是实际凭据。
- [check_contract.py](../tests/check_contract.py) 检查 schema、字节边界和 fixture。
- 各固件 README 记录实际 SDK/build 证据；Corallium README 记录 mac 运行与模拟
  测试证据。跨平台构建和模拟成功不等同 BLE 实机互通。

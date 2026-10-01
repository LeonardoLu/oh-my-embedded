# Codex Micro 兼容连接与命令

Core2 的 USB 为串口桥，固件通过 BLE HOGP 提供兼容 HID vendor report。
广播名为 `Core2 Codex Micro`，明确设备身份；兼容 VID/PID 不代表官方 Micro 硬件或认证。
以下描述仓库实现支持的协议，不保证任意未来宿主版本兼容。

## 发现、分帧与就绪

| 字段 | 值 |
| --- | --- |
| VID / PID | `0x303A / 0x8360` |
| Usage page / Report ID | `0xFF00 / 6` |
| Vendor payload | 63 字节：RPC 通道 2、chunk 长度、最多 61 字节 UTF-8 数据 |
| 请求边界 | 接受完整 JSON，无需结尾换行；固定接收容量 1,024 字节 |
| 响应/通知边界 | 以换行结束，长消息分多个 report |
| MTU | 请求 67；控制就绪要求 peer MTU 至少 `63+3=66` |

标准键盘 report 仅帮助操作系统展示 BLE 配对入口；固件不发送键盘快捷键代替 vendor 事件。
BLE 配对/连接、有效 RPC、通知订阅和足够 MTU 是不同条件。`controlReady` 要求全部满足；
状态展示还需本次连接的新 `thstatus`，不能把之前缓存的颜色当成当前主机反馈。

BLE 回调只锁存连接/MTU 事件，主循环管理 RPC、重连与待释放事件，避免两个执行上下文
同时改写状态。设备页分别显示 Bluetooth 与应用就绪图标。

## RPC 与事件

| 方法 | 用途 |
| --- | --- |
| `sys.version` | 返回字符串版本 |
| `device.status` | 版本、profile/layer 索引、电池百分比与充电状态 |
| `v.oai.thstatus` | 六槽灯光；每项 `id/c/b/e/s` 描述槽位、RGB、亮度、效果、速度 |
| `v.oai.rgbcfg` | keys 与 ambient 灯光配置 |

输入 RPC 读取 `method`（兼容 `m`）与 `id`（兼容 `i`）；参数使用 `params`。
响应回显 `id` 并携带 `result` 或 `error`。未知方法返回错误，`sys.bootloader`
和 `sys.selftest` 明确不支持，因为官方 Micro 固件不适用于 Core2。

按键通知为 `{"m":"v.oai.hid","p":{"k":"…","act":1,"ag":0}}`，
`act` 的 1/0/2 分别表示按下/释放/编码器步进，`ag` 可选。

| 固件键 | 默认交互含义 |
| --- | --- |
| `AG00`–`AG05` | 六个代理槽位 |
| `ACT06/07/08/09` | Fast / Approve / Decline / Fork |
| `ACT10` | 按住说话 |
| `ACT12` | Send |
| `ENC_CW/ENC_CC/ENC_PRESS` | 编码器顺/逆方向与按压 |

宿主可重映射动作。Fast 发送切换请求，但协议不返回开关值，UI 不显示虚构开关状态。
摇杆通过 `v.oai.rad` 发送 `a`（0..1 圈）与 `d`（0..1 距离）：0 向右、0.25 向下。
HOLD MIC 成功发送按下后才显示 MIC ACTIVE，释放或移出时释放；音频由 Mac 采集，
Core2 不采集或上传麦克风流。

## 操作与检查

普通控件在原目标有效释放时触发，主要触摸目标至少 40 px；底部左右半区翻页。
电池在 420 ms 内两次有效点击打开设置，单击不执行动作。
macOS 需要允许宿主访问该 HID 接口；若配对成功而应用未就绪，应分别检查权限与握手条件。

[CodexLink.cpp](../src/CodexLink.cpp)、[HidFraming.h](../include/HidFraming.h)
和 [main.cpp](../src/main.cpp) 是实现依据。分帧、模拟输入与电池双击有纯主机测试；
BLE 订阅/MTU、真实宿主接收及重连需与目标宿主版本联合验证。

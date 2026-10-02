# Corallium 行为与边界

## 原生客户端

一个 SwiftUI target 覆盖 macOS 与 iOS。DeviceStore 在主 actor 串行处理 CoreBluetooth
回调及 UI 状态，EventLog 同样串行追加文件；没有外部依赖、云账户或后台服务器。
macOS WindowGroup 与 MenuBarExtra 共享同一 DeviceStore，不创建第二条连接。
iOS 使用系统自适应 NavigationSplitView 与分享面板；未声明蓝牙后台执行模式。

协议定义归根目录 `protocols/corallium-v1` 所有。仅支持当前两种已知 model，
实际操作按钮来自固件能力数组。未知能力不启用额外功能，未知事件忽略，错误
字段与数值范围在读取后验证。电池未知值保持“未提供”，不由型号推算功耗或续航。
当前 StopWatch 只提供时间同步、电池信息，不展示无线网络卡片或配置入口。
Mosaico 通过 BLE 设置普通 Wi-Fi；本 App 不实现 Wi-Fi Aware。任何设备未声明
`wifi.set` 时均隐藏 Wi-Fi 界面，断开或能力变更会关闭已打开的配置弹窗。
时间显示为最近一次设备快照，UTC 毫秒按设备固定偏移转换；不会伪装为实时设备走时。
checkpoint 的估算状态与尚未校准明确展示；持续供电 RTC 可同时声明 valid 与
estimated，冷启动 checkpoint 则不能声明有效连续走时。夏令时变更需要再次同步固定偏移。

## BLE 会话

由用户点击扫描才创建 Central 并请求蓝牙权限。扫描只匹配 Corallium service，
15 秒自动停止；用户从真实发现的列表中选择连接对象，不按设备名自动连接。
连接及 service/characteristic/notify 初始化共用 20 秒上限。先订阅 TX，然后读取
`device.info` 和 `device.status`，握手无效时关闭连接。

请求最多一个在途，ID 在 App 运行期单调递增。每片 Write with response 成功后继续；
片长不超过系统协商的 write limit，且限制为 180 字节。TX 按 UTF-8 原始字节累积至
LF 才解码，2048 字节上限；超长输入在 framer 中丢弃至 LF，客户端遇到非法帧主动
断连以恢复一致会话。协议没有重传变更机制。

请求等待 10 秒，超时会断连并说明结果未知。所有断连清空待发片段、半行、
关联 ID 与旧设备状态；迟到或错误 ID/op 响应不能结束新请求。正常 status 事件
可更新状态但不能结束在途请求。Wi-Fi 配置收到 connecting 后短时刷新状态，
只有 status.connected 才显示联网成功，手动刷新与设备推送也可更新。

加密由固件 GATT RX 权限强制，系统配对交互由 CoreBluetooth 触发。App 不宣称
Just Works 可验证设备身份；各设备的 BLE 开关、连接窗口与安全边界见根协议。密码仅保留在输入和
发包所需临时内存，发送/取消时清输入，断连或写完成后清队列；不写偏好设置、
Keychain 或日志。日志采用固定事件名称和元数据键白名单，不序列化整个请求。
Debug 构建遇到非法帧时额外记录协议校验类别、已知字段路径和消息字节数；不记录
原始 JSON、字段值或设备提供的未知字段名。
通知订阅失败的系统错误仅记录已知 NSError domain 与数字 code，不记录错误描述或 userInfo。

## 验证模式

演示模式是显式动作（侧栏或 `--demo`）。不使用 Central，响应仍通过真实
JSON 编码、每 7 字节分片、framer 与 response decoder 回流；允许无硬件检查 UI。
StopWatch 演示只声明 `time.set` 和 `battery`，Wi-Fi 模拟验证使用 Mosaico。
演示异步操作携带会话标识，切换或断开后旧操作不会污染新设备。
`--integration-test` 自动完成演示序列并退出；`--self-test` 为边界与存储测试。

无设备时展示连接流程和官方图片，状态/权限错误可见；不虚构扫描结果。
不存在真实硬件时，测试不得将模拟状态写为硬件验收。

# 客户端验证

## 可重复入口

从仓库根执行 `bash apps/Corallium/tools/build.sh macos`、`ios` 或 `simulator`。
执行 `bash apps/Corallium/tools/test.sh` 验证协议与模拟流程。临时 Xcode
DerivedData、完整构建输出、测试输出与截图保存在忽略的 `tmp/`。

已用 Xcode 27 完成 macOS arm64、iOS arm64 真机目标（未签名）与 iOS Simulator 编译。macOS codec/storage
自测已通过，包括根协议的 7 条 Mosaico 入站会话向量、未知读数与 StopWatch 能力向量、UTF-8 任意分片、
超长行恢复、非法 envelope、字节边界、时间范围、RTC estimated 有效时间与冷启动
checkpoint 拒绝、单请求关联、迟到/重复响应、
两天 JSONL 追加与凭据排除。mac App 模拟集成序列已通过，包括时间同步、
StopWatch 时间同步、Mosaico Wi-Fi connecting→connected、移除网络、型号切换与会话清理。
StopWatch 无 Wi-Fi 能力时不发送配置请求。异步断言等待实际状态，
每步具有 5 秒上限，避免依赖窗口初始化所占用的主线程时长。

## macOS 窗口与 StopWatch 实机

已通过 macOS GUI 打开 ad-hoc 签名的沙盒 App，检查主页官方设备图片、侧栏与
窗口。扫描实际发现 StopWatch，连接后成功读取 `bot-ux-watch/0.3.0`、
设备声明的能力、电量和电压。点击“与本机同步时间”收到设备成功回复，
界面显示硬件 RTC 与 UTC+08:00。此证据覆盖客户端到 StopWatch 的真实 BLE
发现、握手、状态读取和时间设置；不由模拟测试推导。

设备重启后再次读到 RTC valid=true 与 UTC+08:00，确认当前 StopWatch 的校时结果
能够跨本次复位保留。已在活动日志查看成功请求并通过 Finder 确认沙盒 Application
Support/<bundleid>/logs 下每日 JSONL 文件存在；日志 UI 未显示 Wi-Fi 凭据。

StopWatch 当前产品能力不包含 Wi-Fi，网络 UI 由固件声明隐藏，真实网络验收仅适用于
保留普通 Wi-Fi 的 ESP-Mosaico。iOS 真机运行、ESP-Mosaico 实机互通、Wi-Fi 配网
与设备功耗/续航需要分别验证。
不将构建成功或串口枚举当作硬件验收。

## macOS 开发签名

默认 macOS 构建使用 Automatic / Apple Development，匹配现有证书的 OU 与项目团队
`ZZM746LVXC`，保留原 bundle ID。缺少匹配证书时直接失败；显式 `macos-adhoc`
用于单独目录的编译检查。无需修改系统隐私数据库或自定义放宽的签名要求。

已构建 Debug 与 Release 两个 SHA-256 不同的 macOS 二进制，导出各自 designated
requirement，分别通过 `codesign --verify --deep --strict -R` 校验另一个构建。
两者均满足对方要求，签名条件包含 bundle ID、Apple 信任链和开发证书身份，
不绑定某次构建的 cdhash。开发签名 App 的 sandbox 与 Bluetooth entitlement 均为 true；
完整客户端自测与模拟集成通过。

此验证覆盖签名要求的双向兼容性。TCC 首次允许与跨重编译授权保留是独立的系统
实测项；从旧 ad-hoc App 切换到证书签名身份后，仍需用户首次允许蓝牙访问。
依据：[Apple TN3127](https://developer.apple.com/documentation/technotes/tn3127-inside-code-signing-requirements)。

## 菜单栏验证边界

macOS 使用原生 SwiftUI MenuBarExtra，与窗口共享连接状态，包含打开窗口、校时、
刷新、断开与退出命令；已随 macOS target 构建。当前 CUA 只返回普通窗口和左侧
应用菜单，不能枚举此状态栏项；SystemUIServer/ControlCenter 表面超时。
因此菜单栏点击与关窗后的重新打开尚未取得自动化 GUI 证据，不以代码存在代替验收。

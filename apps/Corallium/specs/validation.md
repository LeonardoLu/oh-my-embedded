# 客户端验证

## 可重复入口

从仓库根执行 `bash apps/Corallium/tools/build.sh macos`、`ios` 或 `simulator`。
执行 `bash apps/Corallium/tools/test.sh` 验证协议与模拟流程。临时 Xcode
DerivedData、完整构建输出、测试输出与截图保存在忽略的 `tmp/`。

已用 Xcode 27 完成 macOS arm64、iOS arm64 真机目标（未签名）与 iOS Simulator 编译。macOS codec/storage
自测已通过，包括根协议的 7 条入站会话向量及未知读数向量、UTF-8 任意分片、
超长行恢复、非法 envelope、字节边界、时间范围、单请求关联、迟到/重复响应、
两天 JSONL 追加与凭据排除。mac App 模拟集成序列已通过，包括时间同步、
Wi-Fi configuring→connected、移除网络、型号切换与会话清理。

## macOS 窗口与 StopWatch 实机

已通过 macOS GUI 打开 ad-hoc 签名的沙盒 App，检查主页官方设备图片、侧栏与
窗口。扫描实际发现 Corallium Watch，连接后成功读取 `bot-ux-watch/0.3.0`、
设备声明的 4 项能力、电量和电压。点击“与本机同步时间”收到设备成功回复，
界面显示硬件 RTC 与 UTC+08:00。此证据覆盖客户端到 StopWatch 的真实 BLE
发现、握手、状态读取和时间设置；不由模拟测试推导。

设备重启后再次读到 RTC valid=true 与 UTC+08:00，确认当前 StopWatch 的校时结果
能够跨本次复位保留。已在活动日志查看成功请求并通过 Finder 确认沙盒 Application
Support/<bundleid>/logs 下每日 JSONL 文件存在；日志 UI 未显示 Wi-Fi 凭据。

首次真实 Wi-Fi 配置触发设备固件任务栈溢出重启，客户端正确显示通信中断、结果
未知且未崩溃；这不算 Wi-Fi 配置成功。该固件问题需修复后重新验证。
iOS 真机运行、ESP-Mosaico 实机互通与设备功耗/续航需要分别验证。
不将构建成功或串口枚举当作硬件验收。

## 菜单栏验证边界

macOS 使用原生 SwiftUI MenuBarExtra，与窗口共享连接状态，包含打开窗口、校时、
刷新、断开与退出命令；已随 macOS target 构建。当前 CUA 只返回普通窗口和左侧
应用菜单，不能枚举此状态栏项；SystemUIServer/ControlCenter 表面超时。
因此菜单栏点击与关窗后的重新打开尚未取得自动化 GUI 证据，不以代码存在代替验收。

# 客户端验证

## 可重复入口

从仓库根执行 `bash apps/Corallium/tools/build.sh macos`、`ios` 或 `simulator`。
执行 `bash apps/Corallium/tools/test.sh` 验证协议与模拟流程。临时 Xcode
DerivedData、完整构建输出、测试输出与截图保存在忽略的 `tmp/`。

已用 Xcode 27 完成 macOS arm64、iOS arm64 真机目标（未签名）与 iOS Simulator 编译。macOS codec/storage
自测已通过，包括根协议的 7 条 Mosaico 入站会话向量、未知读数与 StopWatch 能力向量、UTF-8 任意分片、
超长行恢复、非法 envelope、字节边界、时间范围、RTC estimated 有效时间与冷启动
checkpoint 拒绝、单请求关联、迟到/重复响应、
两天 JSONL 追加与凭据排除。mac App 模拟集成序列已通过，包括
StopWatch 时间同步、Mosaico Wi-Fi connecting→connected、移除网络、型号切换与会话清理。
StopWatch 无 Wi-Fi 能力时不发送配置请求。异步断言等待实际状态，
每步具有 5 秒上限，避免依赖窗口初始化所占用的主线程时长。

## macOS 窗口与 StopWatch 实机

已通过 macOS GUI 打开 Apple Development 签名的沙盒 App，检查主页官方设备图片、侧栏与
窗口。扫描实际发现 StopWatch，连接后成功读取 `bot-ux-watch/0.3.0`、
设备声明的 time.set/battery 两项能力、电量和电压，页面没有 Wi-Fi 卡片或入口。
点击“与本机同步时间”收到设备成功回复，
界面显示硬件 RTC 与 UTC+08:00。此证据覆盖客户端到 StopWatch 的真实 BLE
发现、握手、状态读取和时间设置；不由模拟测试推导。

保留已有系统配对后重新连接已通过。固件等待本次 device.info 握手再开始异步状态推送，
避免恢复旧 CCCD 时在客户端接收就绪前发送首帧。测试保持同一 App 与已有绑定，
修复前首帧缺少六个 20 字节分片，修复后握手与后续操作正常；客户端严格 JSON 校验
没有放宽。通过设备本地关闭接口停止蓝牙后，App 断开且完整扫描未发现设备；重新开启
后再次握手成功；不手动刷新时，设备时间快照仍按周期状态通知继续更新。
这覆盖已有绑定的重连，不代表首次配对或所有链路异常场景。

设备重启后再次读到 RTC valid=true 与 UTC+08:00，确认当前 StopWatch 的校时结果
能够跨本次复位保留。已在活动日志查看成功请求并通过 Finder 确认沙盒 Application
Support/<bundleid>/logs 下每日 JSONL 文件存在；日志 UI 未显示 Wi-Fi 凭据。

StopWatch 当前产品能力不包含 Wi-Fi，网络 UI 由固件声明隐藏，真实网络验收仅适用于
保留普通 Wi-Fi 的 ESP-Mosaico。iOS 真机运行与设备功耗/续航需要分别验证。
不将构建成功或串口枚举当作硬件验收。

## ESP-Mosaico 实机

同一签名的 macOS App 已发现并连接实际 Mosaico，读取设备身份、六项声明能力与
周期状态。当前 Mosaico 使用普通 GATT，固件日志显示通知订阅成功；用户确认系统
配对弹窗不再出现，主动断开后重新连接也成功。通过界面发送 time.set 后，
设备回包成功，显示 Corallium 时间来源和 UTC+08:00；发送用户指定的 Wi-Fi 配置后，
状态由 connecting 变为 connected 并带有 DHCP 地址。充电状态的功耗/续航显示未提供，
客户端没有用名义电池容量推算。活动日志显示请求成功，未显示 Wi-Fi 凭据。

蓝牙开启超过两分钟后主动断开并再次扫描，设备仍可发现、连接并返回状态；其中一次
完整扫描未发现设备；另有一次早期连接在初始回复后断开，后续会话持续完成配网和状态
更新。尚不作扫描成功率、距离或长期链路稳定性承诺。Mosaico 软件重启后
再次发现并连接成功，网络及 UTC+08:00 保留；此时网络校时已完成，不能据此证明
离线 RTC 保持。用户已确认本地首页快捷入口、天气卡片跳转、居中图标按钮和音量
确认声；长期稳定性和功耗测量仍是独立验证项。

## macOS 开发签名

默认 macOS 构建使用 Automatic / Apple Development，匹配现有证书的 OU 与项目团队
`ZZM746LVXC`，保留原 bundle ID。缺少匹配证书时直接失败；显式 `macos-adhoc`
用于单独目录的编译检查。无需修改系统隐私数据库或自定义放宽的签名要求。

已构建 Debug 与 Release 两个 SHA-256 不同的 macOS 二进制，导出各自 designated
requirement，分别通过 `codesign --verify --deep --strict -R` 校验另一个构建。
两者均满足对方要求，签名条件包含 bundle ID、Apple 信任链和开发证书身份，
不绑定某次构建的 cdhash。开发签名 App 的 sandbox 与 Bluetooth entitlement 均为 true；
完整客户端自测与模拟集成通过。

签名要求的双向兼容性与 TCC 实际授权保留分别取得了证据。用户首次允许开发签名
Debug 的蓝牙访问后，正常退出该 App，通过 CUA 启动独立目录中的 Release。
两个二进制 SHA-256 不同；Release 点击扫描后立即进入 poweredOn（state=5），
发现真实 StopWatch，未出现新的授权等待。相应系统日志时间段也未出现 Corallium
TCC 授权提示或签名要求不匹配记录，确认本次 Debug→Release 实际复用了已授予的权限。
从旧 ad-hoc App 切换到证书签名身份仍需用户首次允许蓝牙访问。
依据：[Apple TN3127](https://developer.apple.com/documentation/technotes/tn3127-inside-code-signing-requirements)。

## 菜单栏验证边界

macOS 使用原生 SwiftUI MenuBarExtra，与窗口共享连接状态，包含打开窗口、校时、
刷新、断开与退出命令；已随 macOS target 构建。当前 CUA 只返回普通窗口和左侧
应用菜单，不能枚举此状态栏项；SystemUIServer/ControlCenter 表面超时。
因此菜单栏点击与关窗后的重新打开尚未取得自动化 GUI 证据，不以代码存在代替验收。

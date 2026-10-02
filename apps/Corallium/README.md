# Corallium

原生 SwiftUI macOS / iOS 设备伴侣，连接使用 [Corallium v1](../../protocols/corallium-v1/README.md)
的 M5Stack StopWatch 与 ESP-Mosaico 固件。macOS 同时提供普通窗口与菜单栏入口；
关闭窗口后可从菜单栏重新打开，退出菜单会结束进程。最低 macOS 14 / iOS 17，
图标由 Xcode 27 的原生 Icon Composer 文档生成。

## 构建与运行

以下命令从仓库根运行，需完整 Xcode 27；不依赖第三方 Swift package。
保留项目原有 bundle ID `com.github.leonardolu.Corallium` 与签名团队。

```sh
# 本地 macOS ad-hoc 签名构建（保留 App Sandbox 与 Bluetooth entitlement）
bash apps/Corallium/tools/build.sh macos
open tmp/corallium-build/Build/Products/Debug/Corallium.app

# iOS 真机目标（编译验证，未签名、不可直接安装）
bash apps/Corallium/tools/build.sh ios
# iOS Simulator
bash apps/Corallium/tools/build.sh simulator
```

iPhone 安装需在 Xcode 中使用用户的有效开发签名与真机目标。项目使用
`NSBluetoothAlwaysUsageDescription`，macOS 开启 Bluetooth entitlement。
首次扫描时系统请求蓝牙权限；若拒绝，App 显示系统设置中的恢复路径。
本地 ad-hoc 构建在源码变化后代码签名哈希会改变，macOS 可能重新请求蓝牙授权；
需在系统弹窗中允许当前构建，再进行真实扫描。

设备端打开蓝牙连接窗口，在 Corallium 点击“扫描附近设备”，再点击实际发现的设备。
App 只扫描指定 service UUID，握手后以 `capabilities` 决定可用操作；官方原厂或旧版
固件不因型号相同自动兼容。当前 StopWatch 仅开放时间同步和电池状态，没有 Wi-Fi
入口。ESP-Mosaico 保留普通 2.4 GHz Wi-Fi 配置：通过 BLE 发送开放/个人密码网络
凭据，不使用 Wi-Fi Aware；开放网络必须显式打开“无密码”开关。macOS/iOS
不需要读取本机 Wi-Fi 密码。

## 自动验证与演示

```sh
bash apps/Corallium/tools/test.sh

# 保持模拟窗口供人工交互；始终显示“演示模式”
open -n tmp/corallium-build/Build/Products/Debug/Corallium.app --args --demo
```

`--self-test` 检查 JSONL/UTF-8 分片、帧长限制、非法消息、时间/SSID 字节范围、
迟到与重复响应关联、每日日志轮转及凭据排除。
测试脚本还编译相同源文件的宿主检查器，消费根协议示例；这样无需给沙盒 App
开放仓库读取权限。未沙盒化的检查器也接受 `--fixtures protocols/corallium-v1/examples`。
`--integration-test` 在运行中的 mac App 内完成 StopWatch 时间同步及不支持 Wi-Fi
的能力检查，再切换 Mosaico 验证 Wi-Fi 配置与异步状态、移除网络及断开；结束后退出。
演示不实例化蓝牙 Central，
不会操作真实设备。模拟的 IP 使用文档地址 `192.0.2.10`。

构建/自动测试通过只证明客户端实现与模拟路径，不等同 BLE 配对、设备 RTC、
真实 Wi-Fi 联网或续航验收。实机证据由项目 [验证说明](specs/validation.md) 单独记录。

## 日志与素材

日志为每日 `YYYY-MM-DD.jsonl`，存放于系统返回的
`Application Support/com.github.leonardolu.Corallium/logs/`。日桶采用当前本地时区，
每条记录为带时区的 ISO 8601 时间。macOS 沙盒构建通常位于：

```text
~/Library/Containers/com.github.leonardolu.Corallium/Data/Library/Application Support/com.github.leonardolu.Corallium/logs/
```

非沙盒本地构建位于 `~/Library/Application Support/com.github.leonardolu.Corallium/logs/`。
App 的“活动日志”展示实际路径；macOS 可用 Finder 打开，iOS 可分享今日日志。
密码、SSID、原始协议帧及设备错误文本不写入日志。文件权限 `0600`，目录 `0700`；
完整每日文件保留，界面仅显示本次启动最近 200 条。写入失败在界面显示。

设备照片来自 [M5Stack](https://docs.m5stack.com/en/core/StopWatch) 与
[Espressif](https://mosaico.espressif.com/)，来源、获取日期与权利情况见
[素材说明](specs/assets.md)。图标原文件为 `Corallium/AppIcon.icon`。

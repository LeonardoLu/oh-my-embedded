# oh-my-embedded

个人嵌入式设备工程仓库。原名 `oh-my-m5stack`，现在按品牌和型号组织设备，
容纳 M5Stack、ESP-Mosaico、Metalio 等设备上的固件、实验和共享组件。

从 [设备清单](DEVICES.md) 查看已知设备与待补充信息，从
[工程索引](projects/README.md) 进入具体项目。

## 目录

```text
DEVICES.md                         设备与配件清单
projects/
  README.md / AGENTS.md             工程导航与公共约定
  m5stack-stopwatch/
    README.md / AGENTS.md           设备入口与硬件约束
    specs/                         设备硬件资料
    bot-ux-watch/                   陪伴时钟工程、代码与 specs
  m5stack-core2/
    README.md / AGENTS.md
    specs/
    bot-ux-codex-core2/             Codex Micro BLE 控制器
    pps/                           PPS 电源演示
  shared-libs/
    bot-ux/                        Bot 动画组件及规格
    ux-components/                 UI、输入、字体和声音组件及规格
    specs/                         两个陪伴应用的共同迭代与验收历史
tools/                             跨工程检查、抓帧与 HID 诊断
wiki/bot-ux/                        双语原生动画图鉴
tmp/                               临时文件与构建记录（gitignored）
```

设备目录使用小写 `品牌-型号`；设备下可有多个独立工程。
设备与工程层均有 `README.md`、`AGENTS.md`，规格和验收记录随其归属存放。
尚未开始开发的设备先记录在清单中，建立工程时再增加目录。

## 构建与检查

现有三个固件均使用 PlatformIO + Arduino；具体依赖固定在各自的
`platformio.ini`。在仓库根目录运行：

```sh
pio run -d projects/m5stack-stopwatch/bot-ux-watch
pio run -d projects/m5stack-core2/bot-ux-codex-core2
pio run -d projects/m5stack-core2/pps
sh tools/check_host.sh
```

两个陪伴应用通过 `lib_extra_dirs = ../../shared-libs` 使用共享库。
PPS 保留项目私有的上游 `lib/M5Module-PPS` 驱动。
新设备根据其 SDK 选择工具链，仓库不要求所有设备使用 M5Unified 或 PlatformIO。

主机检查覆盖输入时序、RTC、字体/形状、动画、HID 报文、灯效与声音；
产物在 `tmp/host-checks/`。实机验证和烧录方式见各工程 README 与 specs。

## 动画图鉴

[BotUx 组件](projects/shared-libs/bot-ux/README.md) 使用调用方提供的画布，
由宿主驱动更新和绘制。[离线 HTML 图鉴](wiki/bot-ux/intro.html) 和
[Markdown 图鉴](wiki/bot-ux/intro.md) 包含双语说明及原生渲染动画。

## License

MIT。第三方代码、字体和素材保留各自的许可证。

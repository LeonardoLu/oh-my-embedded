# 工程索引

工程按 `品牌-型号/项目` 组织，共享代码集中在 `shared-libs/`。
设备持有情况与未来开发设备见根目录 [DEVICES.md](../DEVICES.md)。

| 设备 | 工程 | 用途 | 规格 |
| --- | --- | --- | --- |
| [M5Stack StopWatch](m5stack-stopwatch/README.md) | [bot-ux-watch](m5stack-stopwatch/bot-ux-watch/README.md) | RTC 陪伴时钟、触摸设置、功耗管理 | [项目规格](m5stack-stopwatch/bot-ux-watch/specs/README.md) |
| [M5Stack Core2](m5stack-core2/README.md) | [bot-ux-codex-core2](m5stack-core2/bot-ux-codex-core2/README.md) | Codex Micro 兼容 BLE HID 控制器 | [项目规格](m5stack-core2/bot-ux-codex-core2/specs/README.md) |
| [M5Stack Core2](m5stack-core2/README.md) | [pps](m5stack-core2/pps/README.md) | 官方 PPS 演示适配、串口诊断与供电验证 | [项目记录](m5stack-core2/pps/specs/README.md) |
| 跨设备 | [shared-libs](shared-libs/README.md) | BotUx、UI、字体、手势与声音组件 | [共同迭代历史](shared-libs/specs/README.md) |

设备层 `specs/` 记录硬件与装配；工程层 `specs/` 记录功能和验证；共享库的
规格随库保存。旧的跨设备迭代记录完整归档在 `shared-libs/specs/`，各项目
索引链接到相关记录，不复制成多份真相源。

## 新增设备与工程

1. 在 [设备清单](../DEVICES.md) 登记已知型号与待确认信息。
2. 建立 `<brand-model>/README.md`、`AGENTS.md` 和必要的硬件 `specs/`。
3. 在设备下建立独立工程，补齐 README、AGENTS、构建配置、项目 specs。
4. 有跨工程复用价值的库放入 `shared-libs/`；项目私有驱动可留在项目内。
5. 更新本索引与设备 README，记录构建和验证方法。

## 迁移说明

原 `stopwatch/`、`core2/` 分别迁到 `m5stack-stopwatch/`、`m5stack-core2/`；
根目录 `lib/` 迁到 `shared-libs/`；原根目录 `specs/` 按上述归属分发。
`wiki/`、跨工程 `tools/` 和忽略的 `tmp/` 保留在仓库根目录。
原 PPS `hardware-results.md` 迁入其项目 `specs/`，保留历史日期与证据边界。

在仓库根目录运行三个 `pio run -d projects/<设备>/<工程>` 以及
`sh tools/check_host.sh`，验证迁移后的依赖发现、编译与源码路径。
2026-10-02 的迁移检查结果见 [验证记录](shared-libs/specs/library-path-migration-validation.md)。

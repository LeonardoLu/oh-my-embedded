# 工程索引

工程按 `品牌-型号/项目` 组织，共享代码集中在 `shared-libs/`。
设备持有情况与未来开发设备见根目录 [DEVICES.md](../DEVICES.md)。

| 设备 | 工程 | 用途 | 规格 |
| --- | --- | --- | --- |
| [M5Stack StopWatch](m5stack-stopwatch/README.md) | [bot-ux-watch](m5stack-stopwatch/bot-ux-watch/README.md) | RTC 陪伴时钟、触摸设置、功耗管理 | [项目规格](m5stack-stopwatch/bot-ux-watch/specs/README.md) |
| [M5Stack Core2](m5stack-core2/README.md) | [bot-ux-codex-core2](m5stack-core2/bot-ux-codex-core2/README.md) | Codex Micro 兼容 BLE HID 控制器 | [项目规格](m5stack-core2/bot-ux-codex-core2/specs/README.md) |
| [M5Stack Core2](m5stack-core2/README.md) | [pps](m5stack-core2/pps/README.md) | 官方 PPS 演示适配、串口诊断与供电验证 | [项目规格](m5stack-core2/pps/specs/README.md) |
| 跨设备 | [shared-libs](shared-libs/README.md) | BotUx、UI、字体、手势与声音组件 | [共享集成规格](shared-libs/specs/README.md) |

设备层 `specs/` 记录硬件与装配；工程层 `specs/` 记录功能和验证；共享库的
规格随库保存。共享职责与验证边界集中在 `shared-libs/specs/`，各项目索引
链接到所属规则。规格描述当前事实、决策与原因，过程和被替代设计由 Git 历史承载。

## 新增设备与工程

1. 在 [设备清单](../DEVICES.md) 登记已知型号与待确认信息。
2. 建立 `<brand-model>/README.md`、`AGENTS.md` 和必要的硬件 `specs/`。
3. 在设备下建立独立工程，补齐 README、AGENTS、构建配置、项目 specs。
4. 有跨工程复用价值的库放入 `shared-libs/`；项目私有驱动可留在项目内。
5. 更新本索引与设备 README，记录构建和验证方法。

## 检查入口

构建命令见各工程 README，共享代码检查见
[验证方法与边界](shared-libs/specs/verification.md)。运行输出与调查文件放在根目录
忽略的 `tmp/`；有长期价值的结论连同适用条件写入对应主题规格。

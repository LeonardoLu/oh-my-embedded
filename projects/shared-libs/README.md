# Shared libraries

多个工程复用的 PlatformIO 库。
当前组件依赖 Arduino / M5GFX 或提供纯 C++ 部分；新设备需要验证相应平台适配。

| 库 | 内容 | 规格 |
| --- | --- | --- |
| [bot-ux](bot-ux/README.md) | Bot 表情、情绪、动画、视线和个性化；宿主提供画布与帧循环 | [Bot 规格](bot-ux/specs/README.md) |
| [ux-components](ux-components/README.md) | 字体、抗锯齿形状、指针、滚动、键盘、合成声音和 PCM | [UX 规格](ux-components/specs/README.md) |

位于 `projects/<brand-model>/<project>/` 的 PlatformIO 工程使用：

```ini
lib_extra_dirs = ../../shared-libs
```

项目私有的上游驱动仍可放在项目自己的 `lib/`，例如 PPS 的 `M5Module-PPS`。
库之间不强制合并，宿主按需链接组件。

组件与宿主的职责、集成决策和验证边界见 [共享集成规格](specs/README.md)。
具体行为在所属工程/组件规格与源码中定义，避免多份重复契约。

在仓库根目录运行 `sh tools/check_host.sh`；共享代码更改还需构建所有使用它的工程。

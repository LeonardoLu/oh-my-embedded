# bot-ux

在调用方的 M5Canvas 中绘制可控角色的 PlatformIO 库。宿主创建 sprite，驱动
`update()`/`draw()` 并提交像素；组件不拥有显示器、设备输入或持久化。

[共享库索引](../README.md) · [规格索引](specs/README.md) · [公开 API](src/BotUx.h)

## Quick start

```cpp
#include <M5Unified.h>
#include <BotUx.h>

M5Canvas canvas(&M5.Display);
botux::BotUx bot;

void setup() {
  M5.begin();
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  bot.begin(&canvas);
  bot.setBattery(87);
  bot.setBatteryVisible(false); // let the host own status UI
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) bot.poke();   // surprise -> happy reaction
  bot.update(millis());
  bot.draw();
  canvas.pushSprite(0, 0);
}
```

## 集成

设备工程从 `projects/<brand-model>/<project>/` 通过以下配置发现库：

```ini
lib_extra_dirs = ../../shared-libs
```

M5Unified/M5GFX 的具体版本由宿主 `platformio.ini` 固定。BotUx 当前依赖 M5Canvas，
接入其他 SDK 时需要适配绘图接口并验证实际输出。

## API 与当前语义

| 接口 | 作用 |
| --- | --- |
| `begin(canvas)` | 绑定调用方创建且检查成功的画布 |
| `setStyle(style)` | RGB565 背景、身体、眼睛/强调色，身体/眼形、尺寸和眨眼参数 |
| `setMood(mood)` | 选择 14 个持续情绪之一 |
| `setExpression(expression)` | 选择 Auto 或九种显式表情，可控制过渡时间 |
| `setAnimation(animation)` | 选择 Auto 或七种动作 |
| `setAnimationSpeed / setMotionAmount / setReducedMotion` | 独立调节速度、幅度与减少动态效果 |
| `setGazeDirection / gazeAt / clearGaze` | 持久方向、有限时长的临时目标与取消 |
| `setFaceSide` | Auto/Left/Right 面部手性，独立于眼组位置 |
| `poke / setTalking / setMotion` | 瞬态反应、讲话脉动、归一化 IMU 输入 |
| `setName / describe` | 有界名称与 UTF-8 安全的英/中文自然说明 |
| `setBattery / setSignal / setTime / setLabel` | 可由宿主选择显示的覆盖层 |
| `update(nowMs) / draw()` | 先推进时间状态，再绘制 |

`moodCount/expressionCount/animationCount` 驱动 14×10×8=1,120 组合预览，
名称函数提供双语标签。`mood/expression/animation` 返回选择，
`effectiveMood/effectiveExpression` 反映 Auto 和瞬态反应。
预览应使用独立实例，以免试选覆盖应用状态。

Mood 决定身份，Expression 决定脸，Animation 决定动作。Thinking/Working 使用点阵球，
Blocked 使用感叹号；这些造型不显示眼睛，但保留显式表情选择。Happy 使用笑脸与面颊，
Done 使用独立勾号；它们的含义由调用方决定，组件不推断外部任务完成。

名字上限 16 个受支持 ASCII 字符，空值回退 Milo。语言影响标签和说明，不引入字体或
中文输入法。宿主拥有颜色编辑、文字绘制与持久化。
`presetCount/preset/applyPreset/nextPreset/randomPreset` 提供八个 curated 组合；
`resetToIdle()` 清除反应并恢复 Idle/Auto/Auto，保留样式、名称、速度与幅度。
这些辅助 API 不规定设备按钮必须如何映射。

## 设计契约

- [行为与边界](specs/behavior.md)：状态、名称、时间推进和抗锯齿。
- [视线](specs/gaze.md)：Idle/UpRight 一致性、临时覆盖与面部镜像。
- [点阵球](specs/orb-motion.md)：Thinking/Working 的尺度、运动和有界渲染。
- [Happy/Done](specs/happy-done.md)：情绪标记、动作组合与小尺寸可读性。

这些规格解释当前决策与原因；历史变更可通过 Git 查阅。

## 原生预览与图鉴

[主机预览工具](tools/host-preview/README.md) 编译真实 BotUx 源码并运行几何、组合和
时间检查。[HTML 图鉴](../../../wiki/bot-ux/intro.html) 与
[Markdown 图鉴](../../../wiki/bot-ux/intro.md) 展示 32 个枚举项的双语原生动画。
浏览器播放生成帧，不代替组件计算动画。主机近似图元与有限 GIF 采样不声明面板观感或设备 FPS。

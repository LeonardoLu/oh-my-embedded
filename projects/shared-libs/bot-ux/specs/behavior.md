# BotUx 状态、表情与组件边界

BotUx 在调用方提供的 `M5Canvas*` 上绘制，不拥有显示器，不调用 `M5.update()`，
也不提交像素。宿主控制输入、持久化、设备电源及帧循环。这一边界允许同一角色用于
表盘主角、设置预览和小型工具栏，同时不把某台设备的业务状态写入组件。

## 三个独立选择

| 维度 | 数量 | 作用 |
| --- | --- | --- |
| Mood | 14 | 持续语义：Idle、Listening、Thinking、Speaking、Happy、Sad、Sleepy、Surprised、Working、Waiting、Blocked、Done、Asleep、LookingAround |
| Expression | 10（含 Auto） | 眼睛与面部表情；Auto 跟随有效情绪 |
| Animation | 8（含 Auto） | 身体运动；Auto 使用情绪对应的动作 |

公共枚举数值保持稳定，新值追加；宿主使用计数/名称函数枚举 1,120 个组合。
显式选择会被保存，但 Thinking、Working 和 Blocked 使用整体状态造型，因此表情不会强行画回眼睛。
`mood/expression/animation` 返回选择，`effectiveMood/effectiveExpression` 反映 Auto 与瞬态反应。

`poke()` 暂时呈现惊讶→开心，随后回到调用方选择；它不持久改写情绪或表情。
`setTalking` 驱动说话脉动，`setMotion` 提供归一化 IMU 输入。
八个 curated preset 是额外组合接口，不是情绪全集，也不决定设备 A/B 键的行为。

## 视觉决策

普通表情共享 Neutral 的比例，用开合、弯曲、倾斜和连续插值表达差异，以保持角色辨识度。
Joy 使用一次覆盖率求并的圆滑曲线，Wink 连续闭合一眼，Alarmed 收短并张圆同一眼形。
显式 Neutral 恢复基础脸型；Auto 保留情绪的窄眼或耐心神态。

Waiting 保持清醒并缓慢观察；Sleepy 窄眼、动作迟缓；Asleep 闭眼、深呼吸并有三个淡出的 z。
Thinking 和 Working 的点阵球见 [orb-motion.md](orb-motion.md)，
Happy 与 Done 的区分见 [happy-done.md](happy-done.md)，方向和镜像见 [gaze.md](gaze.md)。

默认椭圆身体和胶囊眼形采用 RGB565 覆盖率抗锯齿，实心内部按跨度绘制、边界混色。
重叠边缘读取实际底像素，避免背景色边缘割裂相邻形状。RoundedSquare、Hexagon 等
替代轮廓仍使用原生 M5GFX 图元。绘制只用固定状态、局部变量及有界栈空间，不逐帧分配堆内存。

## 名称、说明与时间

名字最多 16 个 ASCII 字母、数字、空格、连字符或撇号，去除首尾空白，空值回退 Milo。
语言提供英/中文标签和自然说明；Idle 的显式表情可决定说明，其他情绪使用有效情绪语义。
`describe` 返回所需字节数，对非空缓冲区写终止符且不截断 UTF-8 字符。
字体与文字绘制属于宿主，组件不隐式引入字体资源。

`update(nowMs)` 推进时间状态，`draw()` 仅绘制；动画基于时间而非帧数。
速度、幅度和 reduced motion 是独立控制，零幅度冻结相应动作但保留状态标记。
宿主需要独立预览实例，避免组合试选覆盖主机反馈或持久设置。

## 实现与验证

[BotUx.h](../src/BotUx.h) 是公开 API；[BotUx.cpp](../src/BotUx.cpp) 定义状态解析与绘制。
[原生主机工具](../tools/host-preview/README.md) 检查组合、晚时间窗口、过渡、低帧率、
覆盖率和多尺寸包含关系，并生成 [双语图鉴](../../../../wiki/bot-ux/intro.md)。
主机图元近似与 GIF 采样不能代替真实 M5GFX 面板画面或设备帧时。

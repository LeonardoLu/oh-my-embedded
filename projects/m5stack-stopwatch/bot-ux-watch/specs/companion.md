# 陪伴行为与状态优先级

表盘拥有时间、日期、电池与布局；BotUx 拥有角色动画。Bot 的时间、电量和信号覆盖层
在此宿主隐藏，避免重复信息及并不存在的联网指示。默认名称为 Milo，语言与个性化写入 NVS。

## 自动与手动状态

自动会话从 Idle 开始，停留随机 5–15 秒；随后 LookingAround 停留 30–60 秒，
再从 Idle、Listening、Thinking、Happy、Working、Waiting、Done 中选择一个停留 5–15 秒。
之后在 LookingAround 与安全情绪之间交替。它是本地陪伴表现，不是外部任务进度。

[AmbientCycle](../include/WatchInteraction.h) 每次 update 最多切换一个状态。
迟到的帧从当前时刻安排完整下一阶段，不追赶不可见的中间状态，因体验需要可感知的停留。
比较支持 `millis()` 回绕。

手动情绪、非 Auto 表情、设置/编辑页面、省电、低电量、poke/临时反应、临时视线及活动手势
优先于自动轮播。轮播在这些状态下暂停；重新满足条件时从 Idle 重新计时。
A/B 可以手动选择全部 14 个情绪，并持续保持到 B 双击。双击同时将表情与动作恢复 Auto。
手动 LookingAround
只暂停宿主的情绪轮播，不关闭该情绪内部的自主视线。

## 触摸视线

表盘接触临时呈现 Idle/Auto，并刷新 2,200 ms 的方向保持。真实 Bot 身体内的接触
看正面，身体外取中心到触点的单位向量。到期恢复手动情绪，或开始新的自动 Idle 阶段。
持久化视线设置独立存在，临时覆盖结束后恢复；共用几何和镜像规则见
[BotUx 视线](../../../shared-libs/bot-ux/specs/gaze.md)。

把临时接触与持久选择分开，是为了让触摸有即时回应，又不悄悄改写用户的个性化。
设置预览与表盘 Bot 使用不同实例，组合预览不覆盖正在运行的状态。

## 动作与腕动

BMI270 输入经过低通和死区，摇动使用迟滞及 7 秒冷却后触发 poke。
腕动开关只决定 IMU 输入，不关闭角色本身的动作，也不隐式启用 reduced motion。
动画速度、幅度、表达与方向是独立控制；Thinking/Working 视觉规则见
[点阵球](../../../shared-libs/bot-ux/specs/orb-motion.md)。

## 实现依据

[CompanionPresets.h](../include/CompanionPresets.h)、[WatchInteraction.h](../include/WatchInteraction.h)
和 [WatchFace.cpp](../src/WatchFace.cpp) 定义宿主状态选择。
`test_companion_presets` 与 `test_watch_interaction` 检查 14 情绪手动集、安全自动集、
临时覆盖恢复、暂停/恢复、时长和迟帧行为。角色造型由共享渲染测试验证；实机帧率另行测量。

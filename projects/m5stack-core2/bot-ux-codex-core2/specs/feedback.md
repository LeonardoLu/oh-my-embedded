# 灯光、声音与界面反馈

反馈将槽位身份、主机状态和本地操作分开。屏幕状态点保留主机 RGB，卡片与侧灯使用
固定槽位身份色；灯光节奏表达投影状态，不改写输入 `LightingState`。

## Bottom2 灯光

10 颗 SK6812 使用 GPIO25、GRB。六槽对应物理 LED `0,1,2,7,8,9`，中间 `3..6`
作为 ambient。固定工具链使用 IDF4，而所固定 M5Unified 的该分支未实现 LED 总线，
因此 `BottomLeds` 用原生 RMT 提供发送；IDF5 使用 SDK 总线。缓冲仅在 begin 分配。

| 模式 | 行为 |
| --- | --- |
| Off | 全黑 |
| Status | 六槽身份色加状态节奏，ambient 黑；不响应本地交互动画 |
| Alive（默认） | Status 基础上加入选中、交互、通知与新回复反馈，ambient 跟随相关槽身份色 |

更新最多 20 FPS（50 ms 间隔）；关闭或亮度零立即清灯。
全局亮度 0/1/2/3 通过驱动值 0/64/98/135 得到约 0/16/38/71 的通道输出上限，
原因是 SDK 使用平方增益，直接传小线性数值会把弱反馈量化为黑。
每槽还乘主机亮度；设备亮度不能让主机已关闭的槽位重新亮起。

| 状态 | 普通动态的软件等级 | Reduced Motion |
| --- | --- | --- |
| Idle | 恒定 76 | 76 |
| Working | 2.6 秒呼吸，104–220 | 156 |
| NeedsInput | 1.25 秒脉冲，112–255 | 196 |
| NewReply | 0.85 秒脉冲，130–255 | 220 |
| Error | 1.7 秒双拍，82–255 | 242 |
| Unknown/Off | 黑 | 黑 |

Alive 的已接受操作使用 900 ms 行进反馈，通知为 1,800 ms 包络；`notify(0)` 立即取消。
按住响应仅表达真实本地按压，不暗示主机切换成功。
新回复窗口由 [信号模型](agent-signals.md) 的去重事件启动，最多 30 秒；卡片使用静态
高亮，侧灯负责持续运动，避免为了提醒而整屏重画 30 秒。

未取得新鲜就绪槽位时，六个代理位置黑。断连 Alive 可仅在 ambient 显示有限的蓝紫呼吸，
让控制器仍有存在感，同时不冒充正在工作的代理。Reduced Motion 使用静态强调。

## 声音与主题

音频总开关与六档音量独立，静音保留所选音量。软件增益为 `0,16,30,45,96,255`；
非零音量变更先应用新增益，再播放 Select；零档和静音不播放。
当前单声道配置将 Core2 speaker master 设为 128，合成源限制在 ±15,000。
这些增益和缓冲约束说明软件余量，不等价于声压、失真或扬声器实测。

Paper/Warm/Dark 统一使用 `themePalette`，UI 文字与 Bot 眼睛颜色独立，
防止深色眼睛样式降低 UI 可读性。设置预览使用独立 112 px 画布；静止控件缓存，
`Settings::animate()` 只请求预览区域，交互与页面进入才重画完整页面。

## 实现与检查

[BottomLedFrame.h](../include/BottomLedFrame.h)、[BottomLeds.cpp](../src/BottomLeds.cpp)、
[FeedbackLevel.h](../include/FeedbackLevel.h)、[AudioFeedback.cpp](../src/AudioFeedback.cpp)
和 [Settings.cpp](../src/Settings.cpp) 是实现依据。
主机检查覆盖 RGB、槽位映射、亮度、状态节奏、减少动作、基线和通知窗口。
真实 RMT 灯输出、光学效果与声音质量需要相应 Bottom2 装配验证。

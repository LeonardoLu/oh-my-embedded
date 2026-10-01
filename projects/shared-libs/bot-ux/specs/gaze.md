# 视线、Idle 基准与面部镜像

Idle 的 Auto/Neutral 脸型采用右上方基准：眼组偏移 `(+0.26R,-0.38R)`，眼轴比例 `+0.34`，
屏幕右眼略有尺寸强调。显式 UpRight 与单位向量 `gazeAt(0.707107,-0.707107)`
在稳定后得到同一原生像素结果，避免“空闲右上”和“用户选择右上”出现不同角色。

## 方向与优先级

`GazeDirection` 包括 Auto 与 Center、Left、Right、Up、Down、四个对角方向。
`gazeAt(x,y,holdMs)` 用负 x 表示左、负 y 表示上；范围裁切后对单位圆外向量归一化。
Idle 和 LookingAround 接受临时目标，临时覆盖优先于持久方向，到期后平滑恢复。
具体点击如何转换成向量由宿主决定。

左右方向对称，上限保留 Idle 的 `-0.38R`，下限为更靠近中心的 `+0.16R`。
纯侧向和正面保持直立，沿侧弧的俯仰连续改变眼轴斜率，以同时满足镜像和眼形包含关系。
Idle 中显式非 Neutral 表情保留其编排的视线；需要完整接触跟随时宿主选择 Auto。

LookingAround 每 900–2,800 ms 选择单位圆内的新目标并缓动。其自主移动受幅度和
reduced motion 调节，低/零幅度向标准 Idle 收敛；显式方向与临时触摸仍可使用。
数十秒的情绪轮播属于宿主，例如 [StopWatch 陪伴行为](../../../m5stack-stopwatch/bot-ux-watch/specs/companion.md)。

## 为什么镜像独立于位置

`FaceSide::{Auto,Left,Right}` 选择表情左右手性，不直接移动眼组或反射整块画布。
Auto 跟随平滑后的眼组位置（包括 IMU 反向位移），中心死区过渡到对称脸，避免跨中线时
眨眼侧、倾斜和不对称开合突然翻转。固定 Left/Right 可与任何方向组合。

胶囊和笑弧向量在闭合与小画布限制下仍保留水平符号，因此窄眼也是真正镜像。
不使用“旋转后再向固定屏幕 x 方向增加闭合宽度”的方式，否则左右侧会出现不同外形。

## 检查依据

[BotUx.cpp](../src/BotUx.cpp) 中 `_resolveMood`、方向求值与眼部几何定义规则。
原生预览检查 Idle/UpRight/单位触摸像素相等、临时覆盖恢复、表情与眼形的反射对、
40/72/178/286 px 的包含关系、侧弧连续性以及 LookingAround 的范围。
预览图示：[标准 Idle](../docs/idle-up-right-raster.png)、
[九方向](../docs/nine-directions-raster.png)、[镜像](../docs/face-sides-raster.png)。

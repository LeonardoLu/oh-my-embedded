# Corallium 珊瑚精灵

Corallium 取意珊瑚；连成整体的圆润枝条呼应多个随身设备的连接与陪伴。
六张独立候选均经内建 `image_gen` 一次生成，原生 1254 × 1254，未重采样、
筛选、重试或后期修图。工具未暴露具体模型名称，不能据此断言所用型号。
约束采用 `main-prompt constraints`，每个 JSON 保存完整实际提示词与结果信息。

用户选定 **A1**。其原图逐字节复制到
[`AppIcon.icon/Assets/CoralSprite.png`](../../Corallium/AppIcon.icon/Assets/CoralSprite.png)，
原生 Icon Composer 文档支持 macOS/iOS，系统编译器负责平台形状与尺寸。
Apple `ictool` 已成功导出两个平台的默认外观；生成候选本身保持原样。

| 候选 | 构图 | 主体颜色 | 纯色背景 | 原图／完整提示词 |
| --- | --- | --- | --- | --- |
| A1（已选） | 左下 | 珊瑚红 `#F66F64`、深梅紫 `#412647` | 深青 `#28676B` | [PNG](A1.png) · [JSON](A1.json) |
| A2 | 右下 | 暖桃 `#FFAD80`、莓紫 `#823853` | 薰衣草 `#A39CBD` | [PNG](A2.png) · [JSON](A2.json) |
| A3 | 左下 | 朱红 `#EF7054`、海军蓝 `#243B58` | 浅蓝 `#9BBFD0` | [PNG](A3.png) · [JSON](A3.json) |
| A4 | 右下 | 鲑粉 `#F58287`、茄紫 `#58364D` | 鼠尾草绿 `#94B59D` | [PNG](A4.png) · [JSON](A4.json) |
| A5 | 左下 | 杏橙 `#FFA977`、酒红 `#662A45` | 靛蓝 `#656B96` | [PNG](A5.png) · [JSON](A5.json) |
| A6 | 右下 | 珊瑚玫红 `#ED6681`、海蓝 `#254D63` | 沙色 `#D5B995` | [PNG](A6.png) · [JSON](A6.json) |

颜色表记录提示词指定的语义颜色，不是对生成像素逐色的保证。
六张使用同一珊瑚精灵方向，每张的粗枝轮廓变化写在对应 JSON 的 `feature`。

# LVGL 补充字形

`WatchLvglFont.cpp` 为 HSV 和连接信息补充 24 px 中文字形，其他字形复用
shared ux-components。字体为 Noto Sans SC Regular，采用相同固定 SHA-256 的源文件：
`a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da`。

官方源：[google/fonts NotoSansSC](https://github.com/google/fonts/tree/main/ofl/notosanssc)。
许可证见 [OFL.txt](OFL.txt)。从仓库根运行：

```sh
python3 projects/m5stack-stopwatch/bot-ux-watch/tools/generate_lvgl_font.py /path/to/NotoSansSC.ttf
```

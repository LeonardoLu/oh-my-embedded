#pragma once
#include <string.h>
namespace watchstrings {
struct Entry { const char* en; const char* zh; };
static const Entry entries[] = {
    {"CONNECTION","连接"},{"BLUETOOTH STATUS","蓝牙状态"},{"WAITING","等待"},
    {"CANCEL","取消"},{"BLUETOOTH","蓝牙"},{"CONNECTED","已连接"},{"ON (5 MIN)","开启5分钟"},
    {"SECONDS","秒数"},
    {"TIME","时间"},{"DATE","日期"},{"FORMAT","时间格式"},{"BOT","伙伴"},
    {"DISPLAY","显示"},{"SOUND","声音"},{"POWER","省电"},{"LAYOUT","布局"},{"DONE","完成"},{"BACK","返回"},
    {"EXPRESSION","表情"},{"ACTION","动作"},{"APPEARANCE","外观"},{"COLOR","颜色"},
    {"NAME","名字"},{"LANGUAGE","语言"},{"COMBINATIONS","组合预览"},{"GAZE","视线"},
    {"INTENSITY","幅度"},{"SPEED","速度"},{"SETTINGS","设置"},{"BOT PERSONALITY","伙伴个性"},
    {"TIME TOP","时间在上"},{"BOT TOP","伙伴在上"},{"CUSTOM","自定义"},{"THEME","主题"},
    {"24 H","24小时制"},{"12 H","12小时制"},{"SET TIME","设置时间"},{"SET DATE","设置日期"},
    {"HOUR","时"},{"MINUTE","分"},{"MONTH","月"},{"DAY","日"},{"YEAR","年"},
    {"JAN","一月"},{"FEB","二月"},{"MAR","三月"},{"APR","四月"},{"MAY","五月"},{"JUN","六月"},
    {"JUL","七月"},{"AUG","八月"},{"SEP","九月"},{"OCT","十月"},{"NOV","十一月"},{"DEC","十二月"},
    {"TIME FORMAT","时间格式"},{"12 HOUR","12小时制"},{"24 HOUR","24小时制"},
    {"SECONDS  ON","显示秒数"},{"SECONDS  OFF","隐藏秒数"},
    {"Show seconds on the clock","选择是否在时钟上显示秒数"},
    {"SHAPE","形状"},{"EYES","眼睛"},{"Tap arrows; tap the bot to react","轻点箭头切换，轻点伙伴互动"},
    {"MOTION","动态效果"},{"WRIST","手腕感应"},{"ON","开启"},{"OFF","关闭"},
    {"BOT COLOR","伙伴颜色"},{"USE THEME","使用主题"},{"DISPLAY & SOUND","显示与声音"},
    {"TIME SETTINGS","时间"},{"BRIGHTNESS","亮度"},{"INDICATOR","指示灯"},{"BUTTON FX","按下效果"},
    {"STARTUP SOUND","开机声音"},{"BUTTON SOUND","按键声音"},{"ALERT SOUND","提示声音"},
    {"POWER SAVING","省电"},{"POWER SAVE","省电开关"},{"DIM LEVEL","调暗亮度"},
    {"DIM AFTER","调暗延时"},{"AUTO OFF","自动息屏"},{"WAKE METHOD","唤醒方式"},
    {"CHARGE AWAKE","充电时常亮"},{"FORCED OFF","强制息屏"},{"FROM","开始时间"},{"UNTIL","结束时间"},
    {"DIM TIMEOUT","调暗延时"},{"SCREEN OFF","息屏延时"},{"WAKE","唤醒方式"},
    {"TOUCH + KEYS","触摸 + 按键"},{"KEYS ONLY","仅按键"},{"BOT NAME","伙伴名字"},
    {"5 S","5秒"},{"15 S","15秒"},{"1 MIN","1分钟"},{"5 MIN","5分钟"},
    {"10 MIN","10分钟"},{"15 MIN","15分钟"},
    {"English","英语"},{"WATCH LAYOUT","表盘布局"},{"BOT TEXT","伙伴描述"},
    {"SHOW","显示"},{"HIDE","隐藏"},{"TOP","顶部"},{"STATE","状态"},{"FACE","表情"},{"DIRECTION","方向"},
    {"Night","夜色"},{"Dusk","暮色"},{"Mono","单色"},{"Orb","圆球"},{"Bean","豆形"},{"Pebble","卵石"},
    {"Round","圆眼"},{"Oval","椭圆"},{"Square","方眼"},{"Googly","灵动"},
    {"Auto","自动"},{"Neutral","自然"},{"Curious","好奇"},{"Focused","专注"},{"Joy","喜悦"},
    {"Skeptical","疑惑"},{"Bashful","害羞"},{"Wink","眨眼"},{"Dizzy","晕眩"},{"Alarmed","惊讶"},
    {"Calm","平静"},{"Orbit","环绕"},{"Bounce","弹跳"},{"Glitch","抖动"},{"Wave","摇摆"},{"Sparkle","闪耀"},
    {"Working","工作"},{"Idle","待机"},{"Listening","聆听"},{"Thinking","思考"},{"Speaking","说话"},{"Happy","开心"},
    {"Sad","难过"},{"Sleepy","困倦"},{"Surprised","惊喜"},{"Waiting","等待"},{"Done","完成"},{"Blocked","受阻"},{"Error","出错"},
    {"Asleep","熟睡"},{"Looking around","到处看看"},
    {"Front","正前"},{"Center","正前"},{"Left","左"},{"Right","右"},{"Up","上"},{"Down","下"},
    {"Up left","左上"},{"Up right","右上"},{"Down left","左下"},{"Down right","右下"},
    {"Delete","删除"},{"Space","空格"},{"Confirm","确定"}
};
inline bool& chinese() { static bool value=false; return value; }
inline const char* translate(const char* text, bool enabled) {
    if(enabled) for(const auto& e:entries) if(!strcmp(text,e.en)) return e.zh;
    return text;
}
inline const char* translate(const char* text) { return translate(text,chinese()); }
}

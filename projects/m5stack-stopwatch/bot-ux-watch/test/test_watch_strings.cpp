#include "WatchStrings.h"
#include <assert.h>
int main() {
    for(const auto& e:watchstrings::entries) {
        assert(strcmp(watchstrings::translate(e.en,true),e.en));
        assert(!strcmp(watchstrings::translate(e.en,false),e.en));
        assert(!strcmp(watchstrings::translate(e.en,true),e.zh));
    }
    assert(!strcmp(watchstrings::translate("Milo",true),"Milo"));
    assert(!strcmp(watchstrings::translate("Working",true),"工作"));
    assert(!strcmp(watchstrings::translate("Looking around",true),"到处看看"));
    assert(!strcmp(watchstrings::translate("DISPLAY",true),"显示"));
    assert(!strcmp(watchstrings::translate("POWER",true),"省电"));
    assert(!strcmp(watchstrings::translate("STARTUP SOUND",true),"开机声音"));
    assert(!strcmp(watchstrings::translate("BUTTON SOUND",true),"按键声音"));
    assert(!strcmp(watchstrings::translate("ALERT SOUND",true),"提示声音"));
    assert(!strcmp(watchstrings::translate("POWER SAVE",true),"省电开关"));
    assert(!strcmp(watchstrings::translate("CHARGE AWAKE",true),"充电时常亮"));
    assert(!strcmp(watchstrings::translate("FORCED OFF",true),"强制息屏"));
    assert(!strcmp(watchstrings::translate("FROM",true),"开始时间"));
    assert(!strcmp(watchstrings::translate("UNTIL",true),"结束时间"));
    assert(!strcmp(watchstrings::translate("BUTTON FX",true),"按下效果"));
    assert(!strcmp(watchstrings::translate("DIM TIMEOUT",true),"调暗延时"));
    assert(!strcmp(watchstrings::translate("SCREEN OFF",true),"息屏延时"));
    assert(!strcmp(watchstrings::translate("WAKE",true),"唤醒方式"));
    assert(!strcmp(watchstrings::translate("TOUCH + KEYS",true),"触摸 + 按键"));
    assert(!strcmp(watchstrings::translate("KEYS ONLY",true),"仅按键"));
    assert(!strcmp(watchstrings::translate("5 S",true),"5秒"));
    assert(!strcmp(watchstrings::translate("15 MIN",true),"15分钟"));
}

#pragma once
#include <string.h>
#include "UxInput.h"
#include "UxText.h"
namespace ux {
class NameEditor {
public:
    static constexpr unsigned MaxLength=16;
    enum { Backspace=26, Space=27, Case=28, Done=29 };
    void begin(const char* name) { _length=0;_upper=true;_text[0]=0; if(name) for(;*name;++name) append(*name); }
    const char* text() const { return _text; }
    // A true result means Done. Empty/space-only names become Milo on commit.
    bool press(int key) {
        if(key>=0&&key<26) append((char)((_upper?'A':'a')+key));
        else if(key==Backspace && _length) _text[--_length]=0;
        else if(key==Space) append(' ');
        else if(key==Case) _upper=!_upper;
        else if(key==Done) { while(_length&&_text[_length-1]==' ') _text[--_length]=0; if(!_length) begin("Milo"); return true; }
        return false;
    }
    bool upper() const { return _upper; }
private:
    void append(char c) { if(_length>=MaxLength || !((c>='A'&&c<='Z')||(c>='a'&&c<='z')||c==' '||c=='-'||c=='\'')) return; if(!_length&&c==' ') return; _text[_length++]=c;_text[_length]=0; }
    char _text[MaxLength+1]={}; unsigned _length=0; bool _upper=true;
};
// 6 columns x 5 rows; caller chooses a rectangle wholly inside the device's
// usable display. Last four cells are Del, Space, case toggle, Done.
inline Rect nameKeyRect(Rect area,int key) {
    if(key<0||key>=30) return {0,0,0,0};
    int col=key%6,row=key/6;
    int x=area.x+col*area.w/6,y=area.y+row*area.h/5;
    return {x+2,y+2,(col+1)*area.w/6-col*area.w/6-4,(row+1)*area.h/5-row*area.h/5-4};
}
inline int nameKeyAt(Rect area,int x,int y) {
    if(!area.contains(x,y)||area.w<=0||area.h<=0) return -1;
    for(int k=0;k<30;++k) if(nameKeyRect(area,k).contains(x,y)) return k;
    return -1;
}
struct NameKeyboardLabels {
    const char* backspace; const char* space; const char* letterCase; const char* done;
};
template<class Canvas> void drawNameKeyboard(Canvas& c,const NameEditor& editor,Rect area,uint16_t keyColor,uint16_t textColor,const Font& font=Latin14,int pressed=-1,const NameKeyboardLabels* labels=nullptr) {
    for(int k=0;k<30;++k) {
        Rect r=nameKeyRect(area,k); roundRect(c,r.x,r.y,r.w,r.h,6,k==pressed?blend565(keyColor,textColor,65):keyColor);
        char letter[2]={(char)((editor.upper()?'A':'a')+k),0};
        const char* label=k<26?letter:k==26?"Del":k==27?"_":k==28?"Aa":"OK";
        if(labels && k>=26) label=k==26?labels->backspace:k==27?labels->space:k==28?labels->letterCase:labels->done;
        drawText(c,label,r.x+(r.w-textWidth(label,font))/2,r.y+(r.h-lineHeight(font))/2,textColor,font);
    }
}
}

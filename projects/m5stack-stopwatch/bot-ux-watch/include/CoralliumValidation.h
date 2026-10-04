#pragma once
#include <stddef.h>
#include <stdint.h>
namespace coralliumvalidation {
inline bool utf8(const char* text) {
    const auto* p=(const uint8_t*)text;
    while(*p) {
        uint32_t code=*p++; unsigned trailing=0; uint32_t minimum=0;
        if(code<0x80) continue;
        if(code>=0xC2&&code<=0xDF) { code&=0x1F; trailing=1; minimum=0x80; }
        else if(code>=0xE0&&code<=0xEF) { code&=0x0F; trailing=2; minimum=0x800; }
        else if(code>=0xF0&&code<=0xF4) { code&=0x07; trailing=3; minimum=0x10000; }
        else return false;
        while(trailing--) { if((*p&0xC0)!=0x80) return false; code=(code<<6)|(*p++&0x3F); }
        if(code<minimum||code>0x10FFFF||(code>=0xD800&&code<=0xDFFF)) return false;
    }
    return true;
}
struct JsonInput {
    const char* cursor;
    int read() { return *cursor?(unsigned char)*cursor++:-1; }
    size_t readBytes(char* destination,size_t length) {
        size_t n=0; while(n<length&&*cursor) destination[n++]=*cursor++; return n;
    }
    bool atEnd() const {
        const char* p=cursor;
        while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n') ++p;
        return !*p;
    }
};
}

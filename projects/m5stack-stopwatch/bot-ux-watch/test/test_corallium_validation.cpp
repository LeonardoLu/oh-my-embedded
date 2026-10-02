#include "CoralliumValidation.h"
#include <cassert>
#include <ArduinoJson.h>
int main() {
    using namespace coralliumvalidation;
    assert(utf8("Plain ASCII")); assert(utf8("设备")); assert(utf8("\xf0\x9f\x8c\x8a"));
    assert(!utf8("\xc0\xaf")); assert(!utf8("\xed\xa0\x80"));
    assert(!utf8("\xf4\x90\x80\x80")); assert(!utf8("\xe4\xb8"));
    assert(!utf8("\x80")); assert(!utf8("\xe0\x80\x80"));
    for(const char* text:{"{}", "{\"v\":1}  "}) {
        JsonInput reader{text}; StaticJsonDocument<128> doc;
        assert(!deserializeJson(doc,reader)); assert(reader.atEnd());
    }
    for(const char* text:{"{}x", "{}  x", "{}{}"}) {
        JsonInput reader{text}; StaticJsonDocument<128> doc;
        assert(!deserializeJson(doc,reader)); assert(!reader.atEnd());
    }
    JsonInput good{" \r\n\t"}; assert(good.atEnd());
    JsonInput bad{"  tail"}; assert(!bad.atEnd());
}

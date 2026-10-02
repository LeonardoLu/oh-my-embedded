#pragma once
#include <stdint.h>
#include <vector>
#include <stddef.h>
extern uint32_t testMillis;
inline uint32_t millis() { return testMillis; }
namespace lgfx { struct rgb565_t { uint16_t raw; }; struct swap565_t { uint16_t raw; }; }
class M5Canvas {
public:
    std::vector<uint16_t> pixels=std::vector<uint16_t>(466*466,0);
    void pushImage(int x,int y,int w,int h,const lgfx::rgb565_t* source) {
        for(int row=0;row<h;++row) for(int col=0;col<w;++col)
            pixels[(y+row)*466+x+col]=source[row*w+col].raw;
    }
    void* getBuffer() { return pixels.data(); }
};
struct M5Test { M5Canvas Display; };
extern M5Test M5;

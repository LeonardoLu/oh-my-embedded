#pragma once

#include <stddef.h>
#include <stdint.h>

namespace watchsettingsdirect {

// The 16-bit M5Canvas backing store is an RGB565BE byte stream. The generated
// icon decoder already owns one 200-pixel native-rgb565 scanline; writing that
// row into the existing canvas avoids 200 M5GFX pushImage transactions without
// allocating another image or framebuffer.
template<class Canvas>
class CanvasWriter {
public:
    explicit CanvasWriter(Canvas& canvas):_canvas(canvas) {}

    int32_t width() const { return _canvas.width(); }
    int32_t height() const { return _canvas.height(); }

    void pushImage(int32_t x,int32_t y,int32_t width,int32_t height,
                   const lgfx::rgb565_t* source) {
        if(!source||width<=0||height<=0) return;
        uint8_t* row=(uint8_t*)_canvas.getBuffer()
            +((size_t)y*_canvas.width()+x)*2;
        for(int32_t iy=0;iy<height;++iy) {
            uint8_t* target=row+(size_t)iy*_canvas.width()*2;
            const lgfx::rgb565_t* input=source+(size_t)iy*width;
            for(int32_t ix=0;ix<width;++ix) {
                const uint16_t color=input[ix].raw;
                target[ix*2]=(uint8_t)(color>>8);
                target[ix*2+1]=(uint8_t)color;
            }
        }
    }

private:
    Canvas& _canvas;
};

} // namespace watchsettingsdirect

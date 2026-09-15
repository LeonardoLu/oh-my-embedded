#include <assert.h>
#include <new>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace lgfx { inline namespace v1 {
struct rgb565_t {
    uint16_t raw;
    rgb565_t& operator=(uint16_t value) { raw = value; return *this; }
};
} }

#include "WatchSettingsIconAssets.h"

namespace {
constexpr int kIconPixels = watchsettingsiconassets::Width
                          * watchsettingsiconassets::Height;
constexpr int kMaxCanvasPixels = 1200 * 240;
uint16_t gSource[kIconPixels];
uint16_t gCanvasPixels[kMaxCanvasPixels];
size_t gAllocations = 0;

uint32_t fnv1a565(const uint16_t* pixels, size_t count) {
    uint32_t value = 0x811C9DC5u;
    for (size_t i = 0; i < count; ++i) {
        value = (value ^ (pixels[i] & 0xFF)) * 0x01000193u;
        value = (value ^ (pixels[i] >> 8)) * 0x01000193u;
    }
    return value;
}

struct Canvas {
    Canvas(int width, int height): _width(width), _height(height) {
        assert(width * height <= kMaxCanvasPixels);
    }
    int width() const { return _width; }
    int height() const { return _height; }
    void clear(uint16_t color) {
        for (int i = 0; i < _width * _height; ++i) gCanvasPixels[i] = color;
        pushes = pixelsPushed = maxPushWidth = 0;
    }
    void pushImage(int x, int y, int width, int height,
                   const lgfx::rgb565_t* pixels) {
        assert(x >= 0 && y >= 0 && width > 0 && height == 1);
        assert(x + width <= _width && y < _height);
        for (int i = 0; i < width; ++i)
            gCanvasPixels[y * _width + x + i] = pixels[i].raw;
        ++pushes;
        pixelsPushed += width;
        if (width > maxPushWidth) maxPushWidth = width;
    }
    void pushImage(int, int, int, int, const uint16_t*) = delete;

    int pushes = 0;
    int pixelsPushed = 0;
    int maxPushWidth = 0;
private:
    int _width;
    int _height;
};

void decode(const watchsettingsiconassets::Asset& asset) {
    watchsettingsiconassets::Reader reader(asset);
    for (int row = 0; row < watchsettingsiconassets::Height; ++row)
        assert(reader.readRow(gSource + row * watchsettingsiconassets::Width));
    assert(reader.complete());
    assert(reader.rowsRead() == watchsettingsiconassets::Height);
    assert(reader.bytesRead() == asset.size);
}

uint8_t expand5(uint16_t value) { return (uint8_t)((value * 255 + 15) / 31); }
uint8_t expand6(uint16_t value) { return (uint8_t)((value * 255 + 31) / 63); }

void writeRgb(FILE* output, uint16_t pixel) {
    const uint8_t rgb[] = {
        expand5((pixel >> 11) & 31),
        expand6((pixel >> 5) & 63),
        expand5(pixel & 31)
    };
    assert(fwrite(rgb, sizeof(rgb), 1, output) == 1);
}
} // namespace

void* operator new(size_t size) {
    ++gAllocations;
    void* memory = malloc(size);
    if (!memory) throw std::bad_alloc();
    return memory;
}
void* operator new[](size_t size) {
    ++gAllocations;
    void* memory = malloc(size);
    if (!memory) throw std::bad_alloc();
    return memory;
}
void operator delete(void* memory) noexcept { free(memory); }
void operator delete[](void* memory) noexcept { free(memory); }

int main(int argc, char** argv) {
    using namespace watchsettingsiconassets;
    const uint32_t expectedHashes[] = {
        0x7EAB375Du, 0x688F9DB1u, 0xA6FFEE09u, 0x5AD7F0C3u, 0x1482FF4Eu
    };
    static_assert(sizeof(lgfx::rgb565_t) == sizeof(uint16_t),
                  "host must model M5GFX native RGB565 words");
    static_assert(ScanlineBytes == 400, "runtime buffer must remain one scanline");
    static_assert(sizeof(expectedHashes) / sizeof(expectedHashes[0])
                    == (uint8_t)Icon::Count, "every icon needs a source hash");

    size_t packedBytes = 0;
    for (uint8_t index = 0; index < (uint8_t)Icon::Count; ++index) {
        Icon icon = (Icon)index;
        const Asset* selected = asset(icon);
        assert(selected);
        packedBytes += selected->size;
        decode(*selected);
        assert(selected->pixelHash == expectedHashes[index]);
        assert(fnv1a565(gSource, kIconPixels) == expectedHashes[index]);

        Canvas full(Width, Height);
        full.clear(0xA55A);
        size_t allocations = gAllocations;
        assert(draw(full, icon, Width / 2, Height / 2));
        assert(gAllocations == allocations);
        assert(full.pushes == Height);
        assert(full.pixelsPushed == kIconPixels);
        assert(full.maxPushWidth == Width);
        assert(!memcmp(gCanvasPixels, gSource, sizeof(gSource)));

        // A carousel neighbor can place the 200 px icon beyond the upper-left
        // canvas boundary. Drawing clips to source x/y 80 without a bad read.
        Canvas clipped(120, 90);
        clipped.clear(0xA55A);
        allocations = gAllocations;
        assert(draw(clipped, icon, 20, 20));
        assert(gAllocations == allocations);
        assert(clipped.pushes == 90);
        assert(clipped.pixelsPushed == 120 * 90);
        assert(clipped.maxPushWidth == 120);
        for (int y = 0; y < 90; ++y)
            for (int x = 0; x < 120; ++x)
                assert(gCanvasPixels[y * 120 + x]
                    == gSource[(y + 80) * Width + x + 80]);

        // Exercise the opposite boundary as well: only source 0..19 remains.
        clipped.clear(0xA55A);
        assert(draw(clipped, icon, 200, 170));
        assert(clipped.pushes == 20);
        assert(clipped.pixelsPushed == 20 * 20);
        for (int y = 0; y < 90; ++y) {
            for (int x = 0; x < 120; ++x) {
                uint16_t expected = x >= 100 && y >= 70
                    ? gSource[(y - 70) * Width + x - 100] : 0xA55A;
                assert(gCanvasPixels[y * 120 + x] == expected);
            }
        }
    }
    assert(!asset(Icon::Count));
    assert(packedBytes == 172488);
    assert(packedBytes < (size_t)Width * Height * 2 * (uint8_t)Icon::Count / 2);

    if (argc == 2) {
        Canvas sheet(1200, 240);
        sheet.clear(0);
        for (uint8_t index = 0; index < (uint8_t)Icon::Count; ++index)
            assert(draw(sheet, (Icon)index, 120 + index * 240, 120));
        FILE* output = fopen(argv[1], "wb");
        assert(output);
        fprintf(output, "P6\n1200 240\n255\n");
        for (int i = 0; i < 1200 * 240; ++i) writeRgb(output, gCanvasPixels[i]);
        assert(fclose(output) == 0);
        printf("%s\n", argv[1]);
    }
    printf("settings icons: %zu packed bytes, five exact RGB565 hashes, 400-byte scanline\n",
           packedBytes);
}

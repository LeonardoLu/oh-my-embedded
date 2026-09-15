#pragma once

#include <stdint.h>
#include <math.h>
#include <sstream>
#include <string>
#include <vector>

enum textdatum_t { top_left, top_right, middle_left, middle_center, middle_right };

class M5Canvas {
public:
    M5Canvas(int16_t w, int16_t h) : _w(w), _h(h), _pixels(w * h, 0) {}

    int32_t width() const { return _w; }
    int32_t height() const { return _h; }
    void clear() { _out.str(""); _out.clear(); _outOfBounds = false; }
    void setRecording(bool on) { _recording = on; }
    std::string svgBody() const { return _out.str(); }
    bool outOfBounds() const { return _outOfBounds; }

    uint16_t readPixel(int32_t x, int32_t y) const {
        return pointOutside(x, y) ? 0 : _pixels[y * _w + x];
    }
    void drawPixel(int32_t x, int32_t y, uint32_t c) { rect(x, y, 1, 1, 0, c, true); }
    void fillSprite(uint32_t c) { rect(0, 0, _w, _h, 0, c, true); }
    void fillCircle(int32_t x, int32_t y, int32_t r, uint32_t c) {
        if (x - r < 0 || y - r < 0 || x + r >= _w || y + r >= _h) _outOfBounds = true;
        for (int yy = y-r; yy <= y+r; ++yy) for (int xx = x-r; xx <= x+r; ++xx)
            if ((xx-x)*(xx-x)+(yy-y)*(yy-y) <= r*r && !pointOutside(xx,yy)) _pixels[yy*_w+xx] = c;
        _out << "<circle cx='" << x << "' cy='" << y << "' r='" << r
             << "' fill='" << color(c) << "'/>\n";
    }
    void fillEllipse(int32_t x, int32_t y, int32_t rx, int32_t ry, uint32_t c) {
        if (x - rx < 0 || y - ry < 0 || x + rx >= _w || y + ry >= _h) _outOfBounds = true;
        _out << "<ellipse cx='" << x << "' cy='" << y << "' rx='" << rx
             << "' ry='" << ry << "' fill='" << color(c) << "'/>\n";
    }
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) {
        rect(x, y, w, h, 0, c, true);
    }
    void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t c) {
        rect(x, y, w, h, r, c, true);
    }
    void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t c) {
        rect(x, y, w, h, r, c, false);
    }
    void fillTriangle(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                      int32_t x3, int32_t y3, uint32_t c) {
        if (pointOutside(x1, y1) || pointOutside(x2, y2) || pointOutside(x3, y3))
            _outOfBounds = true;
        int32_t minX = x1 < x2 ? x1 : x2;
        if (x3 < minX) minX = x3;
        int32_t maxX = x1 > x2 ? x1 : x2;
        if (x3 > maxX) maxX = x3;
        int32_t minY = y1 < y2 ? y1 : y2;
        if (y3 < minY) minY = y3;
        int32_t maxY = y1 > y2 ? y1 : y2;
        if (y3 > maxY) maxY = y3;
        for (int32_t y = minY; y <= maxY; ++y) for (int32_t x = minX; x <= maxX; ++x) {
            int32_t a = (x - x2) * (y1 - y2) - (x1 - x2) * (y - y2);
            int32_t b = (x - x3) * (y2 - y3) - (x2 - x3) * (y - y3);
            int32_t d = (x - x1) * (y3 - y1) - (x3 - x1) * (y - y1);
            if (((a >= 0 && b >= 0 && d >= 0) ||
                 (a <= 0 && b <= 0 && d <= 0)) && !pointOutside(x, y))
                _pixels[y * _w + x] = c;
        }
        _out << "<polygon points='" << x1 << "," << y1 << " " << x2 << "," << y2
             << " " << x3 << "," << y3 << "' fill='" << color(c) << "'/>\n";
    }
    void fillArc(int32_t x, int32_t y, int32_t inner, int32_t outer,
                 float start, float end, uint32_t c) {
        if (x - outer < 0 || y - outer < 0 || x + outer >= _w || y + outer >= _h)
            _outOfBounds = true;
        float a0 = start * 3.14159265358979f / 180.0f;
        float a1 = end * 3.14159265358979f / 180.0f;
        float ox0 = x + cosf(a0) * outer, oy0 = y + sinf(a0) * outer;
        float ox1 = x + cosf(a1) * outer, oy1 = y + sinf(a1) * outer;
        float ix1 = x + cosf(a1) * inner, iy1 = y + sinf(a1) * inner;
        float ix0 = x + cosf(a0) * inner, iy0 = y + sinf(a0) * inner;
        int large = ((end - start) > 180.0f) ? 1 : 0;
        _out << "<path d='M " << ox0 << " " << oy0 << " A " << outer << " " << outer
             << " 0 " << large << " 1 " << ox1 << " " << oy1 << " L " << ix1 << " " << iy1
             << " A " << inner << " " << inner << " 0 " << large << " 0 " << ix0 << " " << iy0
             << " Z' fill='" << color(c) << "'/>\n";
    }

    void setTextDatum(textdatum_t d) { _datum = d; }
    void setTextColor(uint32_t c) { _textColor = c; }
    void setTextSize(float s) { _textSize = s; }
    void drawString(const char* text, int32_t x, int32_t y) {
        const char* anchor = (_datum == top_right || _datum == middle_right) ? "end"
                           : (_datum == top_left || _datum == middle_left) ? "start" : "middle";
        _out << "<text x='" << x << "' y='" << y << "' text-anchor='" << anchor
             << "' dominant-baseline='middle' font-family='sans-serif' font-size='"
             << 8.0f * _textSize << "' fill='" << color(_textColor) << "'>";
        for (const char* p = text; p && *p; ++p) {
            if (*p == '&') _out << "&amp;";
            else if (*p == '<') _out << "&lt;";
            else _out << *p;
        }
        _out << "</text>\n";
    }

private:
    bool pointOutside(int32_t x, int32_t y) const {
        return x < 0 || y < 0 || x >= _w || y >= _h;
    }
    static std::string color(uint32_t c) {
        uint8_t r = (uint8_t)(((c >> 11) & 31u) * 255u / 31u);
        uint8_t g = (uint8_t)(((c >> 5) & 63u) * 255u / 63u);
        uint8_t b = (uint8_t)((c & 31u) * 255u / 31u);
        const char* hex = "0123456789ABCDEF";
        std::string s = "#000000";
        s[1] = hex[r >> 4]; s[2] = hex[r & 15];
        s[3] = hex[g >> 4]; s[4] = hex[g & 15];
        s[5] = hex[b >> 4]; s[6] = hex[b & 15];
        return s;
    }
    void rect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r,
              uint32_t c, bool fill) {
        if (x < 0 || y < 0 || x + w > _w || y + h > _h) _outOfBounds = true;
        if (fill) for (int yy = y; yy < y + h; ++yy)
            for (int xx = x; xx < x + w; ++xx)
                if (!pointOutside(xx, yy)) _pixels[yy * _w + xx] = c;
        if (!_recording) return;
        _out << "<rect shape-rendering='crispEdges' x='" << x << "' y='" << y << "' width='" << w << "' height='" << h
             << "' rx='" << r << "' " << (fill ? "fill='" : "fill='none' stroke='")
             << color(c) << "'/>\n";
    }

    int16_t _w, _h;
    textdatum_t _datum = top_left;
    uint32_t _textColor = 0;
    float _textSize = 1.0f;
    bool _outOfBounds = false;
    bool _recording = true;
    std::vector<uint16_t> _pixels;
    std::ostringstream _out;
};

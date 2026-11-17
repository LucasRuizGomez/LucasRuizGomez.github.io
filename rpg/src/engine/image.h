// CPU-side RGBA images and the handful of pixel operations sprite preparation needs.
#pragma once
#include "core/types.h"

#include <string>
#include <vector>

namespace hc {

struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> px;   // RGBA, 4 bytes per pixel, row by row

    Image() = default;
    Image(int w_, int h_) : w(w_), h(h_), px(static_cast<size_t>(w_) * h_ * 4, 0) {}
    bool empty() const { return w <= 0 || h <= 0; }
    uint8_t* at(int x, int y) { return &px[(static_cast<size_t>(y) * w + x) * 4]; }
    const uint8_t* at(int x, int y) const { return &px[(static_cast<size_t>(y) * w + x) * 4]; }

    // Loads a PNG. Returns an empty image (and logs a warning) if the file is missing or broken.
    static Image load(const std::string& path);
    bool savePng(const std::string& path) const;

    Image crop(int x, int y, int cw, int ch) const;   // out-of-range areas come back transparent
    Image flipX() const;
    Image rot90(int quarterTurnsClockwise) const;
    Image trim() const;                               // cut away fully transparent borders
    Image tint(Color c) const;                        // flat colour, original alpha
    Image shade(Color c) const;                       // multiply colour, original alpha
    Image frost(Color c) const;                       // brightened grey scale times colour (snow-covered ground)
    Image outline(Color fill, Color edge) const;      // tinted shape with a 1 px outline (2 px larger)
    void blit(const Image& src, int dx, int dy);      // alpha-over
    void blitPart(const Image& src, int sx, int sy, int sw, int sh, int dx, int dy);
    void fill(int x, int y, int fw, int fh, Color c);
};

}   // namespace hc

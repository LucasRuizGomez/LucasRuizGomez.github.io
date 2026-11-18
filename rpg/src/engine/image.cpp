#include "engine/image.h"

#include "core/files.h"

#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace hc {

Image::Grade Image::grade;

Image Image::graded(const Grade& g) const {
    Image out = *this;
    for (size_t i = 0; i + 3 < out.px.size(); i += 4) {
        uint8_t* p = &out.px[i];
        const double lum = 0.3 * p[0] + 0.59 * p[1] + 0.11 * p[2];
        const uint8_t tint[3] = {g.tint.r, g.tint.g, g.tint.b};
        for (int k = 0; k < 3; k++) {
            double c = lum + (p[k] - lum) * g.saturation;
            c = (c - 128) * g.contrast + 128;
            c *= g.brightness * tint[k] / 255.0;
            p[k] = static_cast<uint8_t>(std::clamp(c, 0.0, 255.0));
        }
    }
    return out;
}

Image Image::load(const std::string& path) {
    int w = 0, h = 0, n = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!data) {
        logWarn("Missing image: %s", path.c_str());
        return {};
    }
    Image im(w, h);
    std::copy(data, data + static_cast<size_t>(w) * h * 4, im.px.begin());
    stbi_image_free(data);
    for (const auto& s : grade.skip)
        if (path.find(s) != std::string::npos) return im;
    const Grade& g = grade;
    if (g.saturation == 1 && g.brightness == 1 && g.contrast == 1 && g.tint.r == 255 && g.tint.g == 255 && g.tint.b == 255) return im;
    return im.graded(g);
}

bool Image::savePng(const std::string& path) const { return !empty() && stbi_write_png(path.c_str(), w, h, 4, px.data(), w * 4) != 0; }

Image Image::crop(int x, int y, int cw, int ch) const {
    Image out(std::max(1, cw), std::max(1, ch));
    for (int j = 0; j < ch; j++)
        for (int i = 0; i < cw; i++) {
            const int sx = x + i, sy = y + j;
            if (sx < 0 || sy < 0 || sx >= w || sy >= h) continue;
            std::copy(at(sx, sy), at(sx, sy) + 4, out.at(i, j));
        }
    return out;
}

Image Image::flipX() const {
    Image out(w, h);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) std::copy(at(w - 1 - i, j), at(w - 1 - i, j) + 4, out.at(i, j));
    return out;
}

Image Image::rot90(int q) const {
    q = ((q % 4) + 4) % 4;
    if (q == 0 || empty()) return *this;
    Image out(q % 2 ? h : w, q % 2 ? w : h);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            int dx, dy;
            if (q == 1) {
                dx = h - 1 - j;
                dy = i;
            } else if (q == 2) {
                dx = w - 1 - i;
                dy = h - 1 - j;
            } else {
                dx = j;
                dy = w - 1 - i;
            }
            std::copy(at(i, j), at(i, j) + 4, out.at(dx, dy));
        }
    return out;
}

Image Image::trim() const {
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            if (at(i, j)[3]) {
                x0 = std::min(x0, i);
                x1 = std::max(x1, i);
                y0 = std::min(y0, j);
                y1 = std::max(y1, j);
            }
    return x1 < 0 ? *this : crop(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
}

Image Image::tint(Color c) const {
    Image out = *this;
    for (size_t i = 0; i < out.px.size(); i += 4) {
        out.px[i] = c.r;
        out.px[i + 1] = c.g;
        out.px[i + 2] = c.b;
    }
    return out;
}

Image Image::shade(Color c) const {
    Image out = *this;
    for (size_t i = 0; i < out.px.size(); i += 4) {
        out.px[i] = static_cast<uint8_t>(out.px[i] * c.r / 255);
        out.px[i + 1] = static_cast<uint8_t>(out.px[i + 1] * c.g / 255);
        out.px[i + 2] = static_cast<uint8_t>(out.px[i + 2] * c.b / 255);
    }
    return out;
}

Image Image::frost(Color c) const {
    Image out = *this;
    for (size_t i = 0; i < out.px.size(); i += 4) {
        const double lum = std::min(255.0, (0.3 * out.px[i] + 0.59 * out.px[i + 1] + 0.11 * out.px[i + 2]) * 1.55 + 18);
        out.px[i] = static_cast<uint8_t>(lum * c.r / 255);
        out.px[i + 1] = static_cast<uint8_t>(lum * c.g / 255);
        out.px[i + 2] = static_cast<uint8_t>(lum * c.b / 255);
    }
    return out;
}

Image Image::outline(Color fillCol, Color edge) const {
    Image out(w + 2, h + 2);
    const Image o = tint(edge), f = tint(fillCol);
    static const int D[8][2] = {{0, 1}, {2, 1}, {1, 0}, {1, 2}, {0, 0}, {2, 2}, {0, 2}, {2, 0}};
    for (const auto& d : D) out.blit(o, d[0], d[1]);
    out.blit(f, 1, 1);
    return out;
}

void Image::blit(const Image& src, int dx, int dy) { blitPart(src, 0, 0, src.w, src.h, dx, dy); }

void Image::blitPart(const Image& src, int sx, int sy, int sw, int sh, int dx, int dy) {
    for (int j = 0; j < sh; j++)
        for (int i = 0; i < sw; i++) {
            const int x = dx + i, y = dy + j, u = sx + i, v = sy + j;
            if (x < 0 || y < 0 || x >= w || y >= h || u < 0 || v < 0 || u >= src.w || v >= src.h) continue;
            const uint8_t* s = src.at(u, v);
            if (!s[3]) continue;
            uint8_t* d = at(x, y);
            const int a = s[3], ia = 255 - a;
            const int outA = a + d[3] * ia / 255;
            if (outA == 0) continue;
            for (int k = 0; k < 3; k++) d[k] = static_cast<uint8_t>((s[k] * a + d[k] * d[3] * ia / 255) / outA);
            d[3] = static_cast<uint8_t>(outA);
        }
}

void Image::fill(int x, int y, int fw, int fh, Color c) {
    for (int j = y; j < y + fh; j++)
        for (int i = x; i < x + fw; i++) {
            if (i < 0 || j < 0 || i >= w || j >= h) continue;
            uint8_t* d = at(i, j);
            d[0] = c.r;
            d[1] = c.g;
            d[2] = c.b;
            d[3] = c.a;
        }
}

}   // namespace hc

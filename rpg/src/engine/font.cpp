#include "engine/font.h"

#include "core/files.h"

#include <SDL.h>
#include <cmath>
#include <unordered_map>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace hc {

namespace {

// Characters beyond ASCII that the game's text uses.
const int EXTRA[] = {0x00B7 /* · */, 0x00D7 /* × */, 0x2026 /* … */, 0x2013 /* – */, 0x2019 /* ’ */, 0x00BD /* ½ */, 0x00E9 /* é */, 0x2014 /* — */};
const int EXTRA_COUNT = static_cast<int>(sizeof EXTRA / sizeof EXTRA[0]);
const double SIZE_BOOST = 1.12;   // Almendra runs small next to the system fonts the layout was tuned for

// Minimal UTF-8 decoder: returns the next code point and advances i.
int nextCodepoint(const std::string& s, size_t& i) {
    const unsigned char c = static_cast<unsigned char>(s[i++]);
    if (c < 0x80) return c;
    int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
    int cp = c & (0x3F >> extra);
    while (extra-- > 0 && i < s.size()) cp = (cp << 6) | (static_cast<unsigned char>(s[i++]) & 0x3F);
    return cp;
}

}   // namespace

struct Fonts::Face {
    std::string data;
    stbtt_fontinfo info{};
};

struct Fonts::Atlas {
    Sprite tex;
    double ascent = 0, descent = 0, space = 0;
    stbtt_packedchar ascii[95]{};
    stbtt_packedchar extra[EXTRA_COUNT]{};
    const stbtt_packedchar* glyph(int cp) const {
        if (cp >= 32 && cp < 127) return &ascii[cp - 32];
        for (int i = 0; i < EXTRA_COUNT; i++)
            if (EXTRA[i] == cp) return &extra[i];
        return &ascii['?' - 32];
    }
    // Advance of a character; the typeface's own word space is too tight at interface sizes
    double advance(int cp) const { return cp == ' ' ? space : glyph(cp)->xadvance; }
};

Fonts::Fonts() = default;
Fonts::~Fonts() = default;

bool Fonts::load(int weight, const std::string& path) {
    auto face = std::make_unique<Face>();
    try {
        face->data = readFile(path);
    } catch (const std::exception& e) {
        logWarn("%s", e.what());
        return false;
    }
    if (!stbtt_InitFont(&face->info, reinterpret_cast<const unsigned char*>(face->data.data()), 0)) {
        logWarn("Not a usable font: %s", path.c_str());
        return false;
    }
    faces_[std::clamp(weight, 0, FACES - 1)] = std::move(face);
    return true;
}

void Fonts::releaseAtlases(Gfx& g) {
    for (auto& [k, a] : atlases_) g.destroy(a->tex);
    atlases_.clear();
}

Fonts::Atlas* Fonts::atlas(Gfx& g, int weight, int px) {
    weight = std::clamp(weight, 0, FACES - 1);
    while (weight > 0 && !faces_[weight]) weight--;
    if (!faces_[weight]) return nullptr;
    px = std::clamp(px, 6, 160);
    const int key = weight * 1000 + px;
    if (auto it = atlases_.find(key); it != atlases_.end()) return it->second.get();

    const Face& face = *faces_[weight];
    auto a = std::make_unique<Atlas>();
    int dim = px <= 20 ? 256 : px <= 44 ? 512 : px <= 90 ? 1024 : 2048;
    std::vector<unsigned char> bitmap;
    for (;; dim *= 2) {
        bitmap.assign(static_cast<size_t>(dim) * dim, 0);
        stbtt_pack_context pc;
        stbtt_PackBegin(&pc, bitmap.data(), dim, dim, 0, 1, nullptr);
        stbtt_pack_range ranges[2] = {};
        ranges[0].font_size = static_cast<float>(px);
        ranges[0].first_unicode_codepoint_in_range = 32;
        ranges[0].num_chars = 95;
        ranges[0].chardata_for_range = a->ascii;
        ranges[1].font_size = static_cast<float>(px);
        ranges[1].array_of_unicode_codepoints = const_cast<int*>(EXTRA);
        ranges[1].num_chars = EXTRA_COUNT;
        ranges[1].chardata_for_range = a->extra;
        const int ok = stbtt_PackFontRanges(&pc, reinterpret_cast<const unsigned char*>(face.data.data()), 0, ranges, 2);
        stbtt_PackEnd(&pc);
        if (ok || dim >= 4096) break;
    }
    Image im(dim, dim);
    for (size_t i = 0; i < bitmap.size(); i++) {
        im.px[i * 4] = im.px[i * 4 + 1] = im.px[i * 4 + 2] = 255;
        im.px[i * 4 + 3] = bitmap[i];
    }
    a->tex = g.makeSprite(im);
    // Glyph textures are drawn 1:1, so linear filtering only matters for subpixel positions
    int asc, desc, gap;
    stbtt_GetFontVMetrics(&face.info, &asc, &desc, &gap);
    const float s = stbtt_ScaleForPixelHeight(&face.info, static_cast<float>(px));
    a->ascent = asc * s;
    a->descent = desc * s;
    a->space = std::max<double>(a->ascii[0].xadvance, px * 0.27);
    Atlas* raw = a.get();
    atlases_[key] = std::move(a);
    return raw;
}

double Fonts::measure(Gfx& g, const std::string& text, double size, int weight) {
    const double scale = g.scale();
    Atlas* a = atlas(g, weight, static_cast<int>(std::lround(size * SIZE_BOOST * scale)));
    if (!a) return 0;
    double w = 0;
    for (size_t i = 0; i < text.size();) w += a->advance(nextCodepoint(text, i));
    return w / scale;
}

void Fonts::draw(Gfx& g, const std::string& text, double x, double y, Color c, double size, int weight, Align align, Baseline base) {
    if (text.empty()) return;
    const double scale = g.scale();
    Atlas* a = atlas(g, weight, static_cast<int>(std::lround(size * SIZE_BOOST * scale)));
    if (!a || !a->tex) return;
    double px = x * scale, py = y * scale;
    if (align != Align::Left) {
        double w = 0;
        for (size_t i = 0; i < text.size();) w += a->advance(nextCodepoint(text, i));
        px -= align == Align::Center ? w / 2 : w;
    }
    if (base == Baseline::Middle) py += (a->ascent + a->descent) / 2 - 1;
    px = std::round(px);
    py = std::round(py);
    SDL_Renderer* ren = g.renderer();
    SDL_SetTextureColorMod(a->tex.tex, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(a->tex.tex, static_cast<Uint8>(c.a * g.alpha()));
    // Honour the global alpha that Gfx applies to sprites
    for (size_t i = 0; i < text.size();) {
        const int cp = nextCodepoint(text, i);
        if (cp == ' ') {
            px += a->space;
            continue;
        }
        const stbtt_packedchar* ch = a->glyph(cp);
        const SDL_Rect src{ch->x0, ch->y0, ch->x1 - ch->x0, ch->y1 - ch->y0};
        const SDL_Rect dst{static_cast<int>(std::lround(px + ch->xoff)), static_cast<int>(std::lround(py + ch->yoff)), src.w, src.h};
        if (src.w > 0 && src.h > 0) SDL_RenderCopy(ren, a->tex.tex, &src, &dst);
        px += ch->xadvance;
    }
}

std::vector<std::string> Fonts::wrap(Gfx& g, const std::string& text, double maxWidth, double size, int weight) {
    std::vector<std::string> lines;
    std::string line, word;
    auto flushWord = [&] {
        if (word.empty()) return;
        const std::string trial = line.empty() ? word : line + " " + word;
        if (!line.empty() && measure(g, trial, size, weight) > maxWidth) {
            lines.push_back(line);
            line = word;
        } else line = trial;
        word.clear();
    };
    for (char ch : text) {
        if (ch == ' ') flushWord();
        else if (ch == '\n') {
            flushWord();
            lines.push_back(line);
            line.clear();
        } else word += ch;
    }
    flushWord();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

}   // namespace hc

#include "engine/gfx.h"

#include "core/files.h"

#include <SDL.h>
#include <cmath>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

namespace hc {

namespace {

// Rounds both edges so neighbouring rectangles never leave a gap at fractional scales.
SDL_Rect dstRect(double x, double y, double w, double h, double scale, double ox, double oy) {
    const int x0 = static_cast<int>(std::lround(x * scale - ox)), y0 = static_cast<int>(std::lround(y * scale - oy));
    const int x1 = static_cast<int>(std::lround((x + w) * scale - ox)), y1 = static_cast<int>(std::lround((y + h) * scale - oy));
    return {x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
}

}   // namespace

bool Gfx::init(const std::string& title, int w, int h, bool headless) {
    headless_ = headless;
    if (headless) SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
#ifdef __EMSCRIPTEN__
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        logWarn("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    win_ = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_RESIZABLE | (headless ? SDL_WINDOW_HIDDEN : 0));
    if (!win_) {
        logWarn("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    ren_ = SDL_CreateRenderer(win_, -1, headless ? SDL_RENDERER_SOFTWARE : (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE));
    if (!ren_) ren_ = SDL_CreateRenderer(win_, -1, SDL_RENDERER_SOFTWARE);
    if (!ren_) {
        logWarn("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    dw_ = w;
    dh_ = h;
    syncSize();
    return true;
}

void Gfx::shutdown() {
    if (ren_) SDL_DestroyRenderer(ren_);
    if (win_) SDL_DestroyWindow(win_);
    ren_ = nullptr;
    win_ = nullptr;
    SDL_Quit();
}

bool Gfx::syncSize() {
    int w = dw_, h = dh_;
#ifdef __EMSCRIPTEN__
    // Back the canvas with real device pixels (capped) so pixel art and text stay sharp on high-DPI screens
    double cw = 0, ch = 0;
    emscripten_get_element_css_size("#canvas", &cw, &ch);
    const double dpr = std::min(emscripten_get_device_pixel_ratio(), 2.0);
    w = std::max(320, static_cast<int>(cw * dpr));
    h = std::max(240, static_cast<int>(ch * dpr));
    if (w != dw_ || h != dh_) {
        emscripten_set_canvas_element_size("#canvas", w, h);
        SDL_SetWindowSize(win_, w, h);
    }
#else
    if (!headless_) SDL_GetRendererOutputSize(ren_, &w, &h);
    w = std::max(320, w);
    h = std::max(240, h);
#endif
    const bool changed = w != dw_ || h != dh_;
    dw_ = w;
    dh_ = h;
    return changed;
}

void Gfx::setFullscreen(bool on) {
#ifndef __EMSCRIPTEN__
    if (win_ && !headless_) SDL_SetWindowFullscreen(win_, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
#else
    (void)on;
#endif
}

Sprite Gfx::makeSprite(const Image& im) {
    Sprite s;
    if (im.empty()) return s;
    s.tex = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, im.w, im.h);
    if (!s.tex) return s;
    SDL_UpdateTexture(s.tex, nullptr, im.px.data(), im.w * 4);
    SDL_SetTextureBlendMode(s.tex, SDL_BLENDMODE_BLEND);
    s.w = im.w;
    s.h = im.h;
    return s;
}

Sprite Gfx::makeTarget(int w, int h) {
    Sprite s;
    s.tex = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, w, h);
    if (!s.tex) {
        logWarn("Cannot create render target %dx%d: %s", w, h, SDL_GetError());
        return s;
    }
    SDL_SetTextureBlendMode(s.tex, SDL_BLENDMODE_BLEND);
    s.w = w;
    s.h = h;
    return s;
}

Sprite Gfx::makeStreaming(int w, int h, bool smooth) {
    Sprite s;
    s.tex = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!s.tex) return s;
    SDL_SetTextureBlendMode(s.tex, SDL_BLENDMODE_BLEND);
    if (smooth) SDL_SetTextureScaleMode(s.tex, SDL_ScaleModeLinear);
    s.w = w;
    s.h = h;
    return s;
}

void Gfx::update(const Sprite& s, const uint8_t* rgba) {
    if (s.tex) SDL_UpdateTexture(s.tex, nullptr, rgba, s.w * 4);
}

void Gfx::destroy(Sprite& s) {
    if (s.tex) SDL_DestroyTexture(s.tex);
    s = Sprite();
}

void Gfx::setTarget(const Sprite* target) { SDL_SetRenderTarget(ren_, target ? target->tex : nullptr); }

void Gfx::setTransform(double scale, double ox, double oy) {
    scale_ = scale;
    ox_ = ox;
    oy_ = oy;
}

void Gfx::clear(Color c) {
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren_, c.r, c.g, c.b, c.a);
    SDL_RenderClear(ren_);
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
}

void Gfx::fillRect(double x, double y, double w, double h, Color c) {
    const SDL_Rect r = dstRect(x, y, w, h, scale_, ox_, oy_);
    SDL_SetRenderDrawColor(ren_, c.r, c.g, c.b, static_cast<Uint8>(c.a * alpha_));
    SDL_RenderFillRect(ren_, &r);
}

void Gfx::draw(const Sprite& s, double x, double y, bool flip) { draw(s, x, y, s.w, s.h, flip); }

void Gfx::draw(const Sprite& s, double x, double y, double w, double h, bool flip) {
    if (!s.tex) return;
    const SDL_Rect r = dstRect(x, y, w, h, scale_, ox_, oy_);
    SDL_SetTextureAlphaMod(s.tex, static_cast<Uint8>(255 * alpha_));
    SDL_SetTextureColorMod(s.tex, 255, 255, 255);
    SDL_RenderCopyEx(ren_, s.tex, nullptr, &r, 0, nullptr, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void Gfx::drawPart(const Sprite& s, IRect src, double x, double y, double w, double h) {
    if (!s.tex) return;
    const SDL_Rect r = dstRect(x, y, w, h, scale_, ox_, oy_);
    const SDL_Rect sr{src.x, src.y, src.w, src.h};
    SDL_SetTextureAlphaMod(s.tex, static_cast<Uint8>(255 * alpha_));
    SDL_SetTextureColorMod(s.tex, 255, 255, 255);
    SDL_RenderCopy(ren_, s.tex, &sr, &r);
}

void Gfx::drawRotated(const Sprite& s, double x, double y, double radians, double pivotX, double pivotY) {
    if (!s.tex) return;
    const SDL_Rect r = dstRect(x, y, s.w, s.h, scale_, ox_, oy_);
    const SDL_Point p{static_cast<int>(std::lround(pivotX * scale_)), static_cast<int>(std::lround(pivotY * scale_))};
    SDL_SetTextureAlphaMod(s.tex, static_cast<Uint8>(255 * alpha_));
    SDL_SetTextureColorMod(s.tex, 255, 255, 255);
    SDL_RenderCopyEx(ren_, s.tex, nullptr, &r, radians * 180.0 / PI, &p, SDL_FLIP_NONE);
}

void Gfx::drawTinted(const Sprite& s, double x, double y, double w, double h, Color tint, bool additive, bool flip) {
    if (!s.tex) return;
    const SDL_Rect r = dstRect(x, y, w, h, scale_, ox_, oy_);
    SDL_SetTextureAlphaMod(s.tex, static_cast<Uint8>(tint.a * alpha_));
    SDL_SetTextureColorMod(s.tex, tint.r, tint.g, tint.b);
    if (additive) SDL_SetTextureBlendMode(s.tex, SDL_BLENDMODE_ADD);
    SDL_RenderCopyEx(ren_, s.tex, nullptr, &r, 0, nullptr, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    if (additive) SDL_SetTextureBlendMode(s.tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(s.tex, 255, 255, 255);
}

void Gfx::present() { SDL_RenderPresent(ren_); }

bool Gfx::screenshot(const std::string& path) {
    Image im(dw_, dh_);
    if (SDL_RenderReadPixels(ren_, nullptr, SDL_PIXELFORMAT_RGBA32, im.px.data(), dw_ * 4) != 0) {
        logWarn("Screenshot failed: %s", SDL_GetError());
        return false;
    }
    for (size_t i = 3; i < im.px.size(); i += 4) im.px[i] = 255;
    return im.savePng(path);
}

}   // namespace hc

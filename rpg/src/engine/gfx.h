// Thin 2D drawing layer over SDL_Renderer: sprites (one texture each), render targets,
// and a canvas-style transform (scale + offset) so game code can draw in world or interface units.
#pragma once
#include "core/types.h"
#include "engine/image.h"

#include <string>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace hc {

struct Sprite {
    SDL_Texture* tex = nullptr;
    int w = 0, h = 0;
    explicit operator bool() const { return tex != nullptr; }
};

struct IRect {
    int x = 0, y = 0, w = 0, h = 0;
};

class Gfx {
  public:
    // headless: no visible window (used for automated screenshots).
    bool init(const std::string& title, int w, int h, bool headless);
    void shutdown();

    int width() const { return dw_; }
    int height() const { return dh_; }
    // Adopts the current window / canvas size. Returns true when it changed.
    bool syncSize();
    void setFullscreen(bool on);

    Sprite makeSprite(const Image& im);
    Sprite makeTarget(int w, int h);
    Sprite makeStreaming(int w, int h, bool smooth);
    void update(const Sprite& s, const uint8_t* rgba);   // streaming sprites: w*h*4 bytes
    void destroy(Sprite& s);
    void setTarget(const Sprite* target);                // nullptr = the base target (normally the screen)
    void setBaseTarget(const Sprite* target);            // where setTarget(nullptr) draws: a full-screen texture, or the screen
    void setSmooth(const Sprite& s, bool on);            // linear filtering when the sprite is scaled

    // screen = value * scale - offset
    void setTransform(double scale, double ox = 0, double oy = 0);
    double scale() const { return scale_; }
    void setAlpha(double a) { alpha_ = std::clamp(a, 0.0, 1.0); }
    double alpha() const { return alpha_; }

    void clear(Color c);
    void fillRect(double x, double y, double w, double h, Color c);
    void draw(const Sprite& s, double x, double y, bool flip = false);
    void draw(const Sprite& s, double x, double y, double w, double h, bool flip = false);
    void drawPart(const Sprite& s, IRect src, double x, double y, double w, double h);
    void drawRotated(const Sprite& s, double x, double y, double radians, double pivotX, double pivotY, double scale = 1);
    void drawTinted(const Sprite& s, double x, double y, double w, double h, Color tint, bool additive = false, bool flip = false);
    void present();
    bool screenshot(const std::string& path);

    SDL_Renderer* renderer() const { return ren_; }
    SDL_Texture* base_ = nullptr;

  private:
    SDL_Window* win_ = nullptr;
    SDL_Renderer* ren_ = nullptr;
    int dw_ = 0, dh_ = 0;
    double scale_ = 1, ox_ = 0, oy_ = 0, alpha_ = 1;
    bool headless_ = false;
};

}   // namespace hc

// Interface building blocks: text, parchment panels (9-slice), buttons, slots, key caps, click regions.
// Everything is laid out in a 1280x720-ish virtual space scaled by ui_, and rasterised at device resolution.
#include "app/app.h"
#include "app/palette.h"

#include <cmath>

namespace hc {

void App::hit(double x, double y, double w, double h, std::function<void()> click, std::function<void()> hover) { hits_.push_back({x, y, w, h, std::move(click), std::move(hover)}); }

const App::Hit* App::hitAt(double ux, double uy) const {
    for (size_t i = hits_.size(); i-- > 0;) {
        const Hit& h = hits_[i];
        if (ux >= h.x && ux < h.x + h.w && uy >= h.y && uy < h.y + h.h) return &h;
    }
    return nullptr;
}

void App::text(const std::string& s, double x, double y, Color col, double size, int weight, Align align, Baseline base) { fonts_.draw(g_, s, x, y, col, size, weight, align, base); }

void App::shadowText(const std::string& s, double x, double y, Color col, double size, int weight, Align align) {
    const double d = std::max(1.0, size / 16);
    text(s, x + d, y + d, pal::ink, size, weight, align);
    text(s, x, y, col, size, weight, align);
}

double App::textW(const std::string& s, double size, int weight) { return fonts_.measure(g_, s, size, weight); }
std::vector<std::string> App::wrap(const std::string& s, double maxW, double size, int weight) { return fonts_.wrap(g_, s, maxW, size, weight); }

// 9-slice of a UI-pack sprite: crisp corners (`corner` art pixels), stretched edges and centre, 2 UI px per art pixel
void App::nine(const Sprite& img, double x, double y, double w, double h, int c) {
    if (!img) return;
    const int k = std::max(1, static_cast<int>(std::lround(ui_ * 2))), ck = c * k;
    const int X = static_cast<int>(std::lround(x * ui_)), Y = static_cast<int>(std::lround(y * ui_));
    const int W = std::max(2 * ck, static_cast<int>(std::lround(w * ui_))), H = std::max(2 * ck, static_cast<int>(std::lround(h * ui_)));
    const int sx[3] = {0, c, img.w - c}, sw[3] = {c, img.w - 2 * c, c}, dx[3] = {X, X + ck, X + W - ck}, dw[3] = {ck, W - 2 * ck, ck};
    const int sy[3] = {0, c, img.h - c}, sh[3] = {c, img.h - 2 * c, c}, dy[3] = {Y, Y + ck, Y + H - ck}, dh[3] = {ck, H - 2 * ck, ck};
    g_.setTransform(1);
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++)
            if (dw[i] > 0 && dh[j] > 0) g_.drawPart(img, {sx[i], sy[j], sw[i], sh[j]}, dx[i], dy[j], dw[i], dh[j]);
    g_.setTransform(ui_);
}

void App::panel(double x, double y, double w, double h) { nine(spr_.ui.panel, x, y, w, h, 6); }
void App::slotBox(double x, double y, double w, double h, bool dark) { nine(dark ? spr_.ui.slotDark : spr_.ui.slot, x, y, w, h, 3); }

void App::button(double x, double y, double w, double h, const std::string& label, bool sel, double size) {
    nine(sel ? spr_.ui.btnSel : spr_.ui.btn, x, y, w, h, 4);
    if (!label.empty()) text(label, x + w / 2, y + h / 2 + 1, pal::btnInk, size, 1, Align::Center, Baseline::Middle);
}

void App::row(double x, double y, double w, double h, bool sel) { box(x, y, w, h, sel ? pal::slotSel : pal::slot, sel ? pal::sel : pal::line, sel ? 2 : 1); }

void App::box(double x, double y, double w, double h, Color fill, Color border, double bw) {
    if (fill.a) g_.fillRect(x, y, w, h, fill);
    if (border.a) {
        g_.fillRect(x, y, w, bw, border);
        g_.fillRect(x, y + h - bw, w, bw, border);
        g_.fillRect(x, y, bw, h, border);
        g_.fillRect(x + w - bw, y, bw, h, border);
    }
}

void App::bar(double x, double y, double w, double h, double frac, Color fill, Color back) {
    g_.fillRect(x - 1, y - 1, w + 2, h + 2, pal::ink);
    g_.fillRect(x, y, w, h, back);
    g_.fillRect(x, y, std::round(w * std::clamp(frac, 0.0, 1.0)), h, fill);
}

// Draws a sprite at a whole multiple of its size so pixels stay square
void App::drawSpr(const Sprite& img, double x, double y, double size) {
    if (!img) return;
    const int k = std::max(1, static_cast<int>(std::floor(size * ui_ / std::max(img.w, img.h))));
    const int w = img.w * k, h = img.h * k;
    g_.setTransform(1);
    g_.draw(img, std::round(x * ui_ + (size * ui_ - w) / 2), std::round(y * ui_ + (size * ui_ - h) / 2), w, h);
    g_.setTransform(ui_);
}

double App::keycap(const std::string& label, double x, double y, double h) {
    const double w = std::max(h, textW(label, h * 0.62, 1) + h * 0.6);
    box(x, y, w, h, Color(107, 70, 38), Color(46, 28, 14));
    text(label, x + w / 2, y + h / 2 + 1, pal::ltText, h * 0.62, 1, Align::Center, Baseline::Middle);
    return w;
}

void App::dim(double alpha) { g_.fillRect(0, 0, uiW() + 1, uiH() + 1, Color(8, 7, 12, static_cast<uint8_t>(255 * alpha))); }

void App::closeButton(double x, double y) {
    drawSpr(spr_.ui.close, x + 3, y + 3, 20);
    hit(x, y, 26, 26, [this] { setMode(Mode::Play); });
}

Color App::rarityColor(const Item& it) const {
    return it.rarity > 0 && it.rarity < static_cast<int>(db_.rarities.size()) ? db_.rarities[it.rarity].color : pal::gold;
}

void App::centerCard(const std::string& title, const std::string& sub, const std::vector<std::pair<std::string, std::string>>& lines, const std::string& foot) {
    dim(0.72);
    const double W = std::min(520.0, uiW() - 40), H = 150 + lines.size() * 26, X = std::round((uiW() - W) / 2), Y = std::round((uiH() - H) / 2);
    panel(X, Y, W, H);
    text(title, X + W / 2, Y + 54, pal::text, 28, 2, Align::Center);
    if (!sub.empty()) text(sub, X + W / 2, Y + 84, pal::muted, 15, 0, Align::Center);
    for (size_t i = 0; i < lines.size(); i++) {
        text(lines[i].first, X + 40, Y + 120 + i * 26, pal::muted, 14);
        text(lines[i].second, X + W - 40, Y + 120 + i * 26, pal::text, 14, 1, Align::Right);
    }
    text(foot, X + W / 2, Y + H - 22, pal::gold, 14, 1, Align::Center);
}

}   // namespace hc

// Cutscenes: a few painted shots with narration (config/cutscenes.json), played when the game starts, when a boss
// falls and when the hero first sets foot in another land.
//
// Each shot is a small stage, 320 x 180 art pixels, drawn from scratch every frame: a backdrop painted from layered
// silhouettes (sky, hills, peaks, sea, crypt walls), the game's own sprites standing on it as actors, and drifting
// particles (embers, snow, mist...). The camera creeps in over the shot, the narration is typed out beneath it, and
// shots cross through black. Space or a click moves on, Escape skips the lot.
#include "app/app.h"
#include "app/palette.h"
#include "core/rng.h"

#include <algorithm>
#include <cmath>

namespace hc {

namespace {

constexpr double SW = 320, SH = 180;   // the stage, in art pixels
constexpr double FADE = 0.6;           // seconds of black between shots
constexpr double TYPE_CPS = 40;        // narration speed, characters per second

Color mix(Color a, Color b, double f) {
    f = std::clamp(f, 0.0, 1.0);
    auto l = [&](uint8_t x, uint8_t y) { return static_cast<uint8_t>(std::lround(x + (y - x) * f)); };
    return Color(l(a.r, b.r), l(a.g, b.g), l(a.b, b.b), l(a.a, b.a));
}
Color withA(Color c, double a) { return Color(c.r, c.g, c.b, static_cast<uint8_t>(std::clamp(a, 0.0, 1.0) * 255)); }
double ease(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3 - 2 * t);
}
// Smooth, repeatable 1D noise for skylines: a few sines at unrelated rates
double ridge(double x, double seed) {
    return std::sin(x * 0.031 + seed) * 0.5 + std::sin(x * 0.073 + seed * 2.3) * 0.3 + std::sin(x * 0.19 + seed * 4.1) * 0.12 + std::sin(x * 0.47 + seed * 7.7) * 0.05;
}

struct Palette {
    Color sky0, sky1, land, haze;
};

Palette paletteFor(const ShotDef& s) {
    Palette p;
    if (s.scene == "dusk" || s.scene == "village") p = {Color(46, 26, 48), Color(196, 104, 66), Color(26, 18, 24), Color(150, 80, 70)};
    else if (s.scene == "dawn") p = {Color(64, 84, 124), Color(226, 176, 136), Color(36, 38, 46), Color(170, 150, 150)};
    else if (s.scene == "fire") p = {Color(60, 14, 10), Color(206, 86, 30), Color(18, 10, 10), Color(150, 50, 24)};
    else if (s.scene == "crypt" || s.scene == "throne") p = {Color(18, 16, 24), Color(40, 34, 46), Color(14, 12, 18), Color(60, 50, 70)};
    else if (s.scene == "void") p = {Color(6, 5, 10), Color(14, 10, 22), Color(4, 4, 8), Color(40, 30, 60)};
    else if (s.scene == "peaks") p = {Color(22, 30, 54), Color(110, 130, 170), Color(30, 34, 46), Color(150, 160, 190)};
    else if (s.scene == "fields") p = {Color(40, 44, 30), Color(150, 140, 90), Color(28, 26, 18), Color(120, 110, 70)};
    else p = {Color(10, 12, 28), Color(46, 44, 76), Color(14, 14, 20), Color(70, 70, 100)};   // night, sea
    if (s.tinted) {
        p.sky0 = s.sky0;
        p.sky1 = s.sky1;
        p.land = s.land;
        p.haze = mix(s.sky1, s.land, 0.5);
    }
    return p;
}

}   // namespace

// ───────────────────────────── playback ─────────────────────────────

void App::beginCutscene(const std::string& id) {
    const CutsceneDef* cs = db_.cutscene(id);
    if (!cs) return;
    cutId_ = id;
    cutShot_ = 0;
    cutT_ = 0;
    cutOut_ = -1;
    cutTypedAt_ = -1;
    motes_.clear();
    sim_->mode = Mode::Cutscene;
}

void App::endCutscene() {
    cutId_.clear();
    motes_.clear();
    if (sim_->mode == Mode::Cutscene) sim_->mode = Mode::Play;
}

void App::updateCutscene(double dt) {
    const CutsceneDef* cs = db_.cutscene(cutId_);
    if (!cs) {
        endCutscene();
        return;
    }
    if (cutOut_ >= 0) {   // leaving: fade to black, then back to the game
        cutOut_ += dt;
        if (cutOut_ >= FADE * 1.5) endCutscene();
        return;
    }
    cutT_ += dt;
    const ShotDef& s = cs->shots[cutShot_];
    const double typed = s.text.size() / TYPE_CPS;
    // A shot lasts its length, and at least long enough to read what it says
    const double length = std::max(s.secs, typed + 2.6 + FADE);
    if (cutT_ >= length) nextShot();
}

void App::nextShot() {
    const CutsceneDef* cs = db_.cutscene(cutId_);
    if (!cs) return;
    if (cutShot_ + 1 >= static_cast<int>(cs->shots.size())) {
        cutOut_ = 0;
        return;
    }
    cutShot_++;
    cutT_ = 0;
    cutTypedAt_ = -1;
    motes_.clear();
}

void App::cutsceneKey(const std::string& code) {
    if (cutOut_ >= 0) return;
    if (code == "Escape") {   // skip the whole thing
        cutOut_ = 0;
        return;
    }
    if (!(code == "Enter" || code == "NumpadEnter" || code == "Space" || code == "KeyE" || code == "Click")) return;
    const CutsceneDef* cs = db_.cutscene(cutId_);
    if (!cs) return;
    const ShotDef& s = cs->shots[cutShot_];
    const double shown = (cutT_ - FADE * 0.5) * TYPE_CPS;
    if (cutTypedAt_ < 0 && shown < s.text.size()) cutTypedAt_ = cutT_;   // first press: finish the sentence
    else nextShot();
}

// ───────────────────────────── drawing ─────────────────────────────

void App::drawCutscene() {
    const CutsceneDef* cs = db_.cutscene(cutId_);
    g_.setTransform(1);
    g_.setAlpha(1);
    const int DW = g_.width(), DH = g_.height();
    g_.fillRect(0, 0, DW, DH, Color(4, 3, 6));
    if (!cs) return;
    const ShotDef& s = cs->shots[cutShot_];
    const double t = cutT_;
    const double length = std::max(s.secs, s.text.size() / TYPE_CPS + 2.6 + FADE);
    const double k = ease(t / length);

    // The picture: 16:9, as large as leaves a band of black beneath it for the words
    const double fit = std::min(DW * 0.94 / SW, DH * 0.74 / SH);
    const double picW = SW * fit, picH = SH * fit;
    const double px = (DW - picW) / 2, py = std::max(DH * 0.04, (DH * 0.8 - picH) / 2);
    double zoom = s.zoomFrom + (s.zoomTo - s.zoomFrom) * k;
    double cx = SW / 2 + s.panX * SW * k, cy = SH / 2 + s.panY * SH * k;
    if (std::find(s.fx.begin(), s.fx.end(), "quake") != s.fx.end()) {
        const double q = 1.6 * (0.6 + 0.4 * std::sin(t * 1.3));
        cx += std::sin(t * 41) * q;
        cy += std::cos(t * 37) * q * 0.7;
    }
    // Clip to the picture: draw the stage, then mask everything outside it with black
    const double scale = fit * zoom;
    g_.setTransform(scale, cx * scale - (px + picW / 2), cy * scale - (py + picH / 2));
    drawBackdrop(s, t);
    for (const auto& a : s.actors) drawActor(a, t, length);
    drawMotes(s, t, 1.0 / 60);
    g_.setTransform(1);
    const Color black(4, 3, 6);
    g_.fillRect(0, 0, DW, py, black);
    g_.fillRect(0, py + picH, DW, DH - py - picH, black);
    g_.fillRect(0, 0, px, DH, black);
    g_.fillRect(px + picW, 0, DW - px - picW, DH, black);
    // A soft vignette at the picture's edges
    for (int i = 0; i < 10; i++) {
        const double w = picW * 0.012, a = 0.5 * (1 - i / 10.0);
        g_.fillRect(px + i * w, py, w, picH, withA(black, a * 0.6));
        g_.fillRect(px + picW - (i + 1) * w, py, w, picH, withA(black, a * 0.6));
        g_.fillRect(px, py + i * w, picW, w, withA(black, a * 0.7));
        g_.fillRect(px, py + picH - (i + 1) * w, picW, w, withA(black, a * 0.9));
    }

    // Words, in interface space
    g_.setTransform(ui_);
    const double W = uiW(), H = uiH();
    const double picBottom = (py + picH) / ui_;
    if (!s.title.empty()) {
        const double a = std::clamp((t - 0.4) / 1.4, 0.0, 1.0) * std::clamp((length - FADE - t) / 0.8, 0.0, 1.0);
        const double midY = (py + picH * 0.46) / ui_;
        g_.setAlpha(a);
        shadowText(s.title, W / 2, midY, pal::ltGold, std::min(84.0, W / 11), 3, Align::Center);
        g_.setAlpha(1);
    }
    if (!s.text.empty()) {
        const double size = std::clamp(W / 52, 16.0, 22.0);
        const auto lines = wrap(s.text, std::min(W * 0.72, 860.0), size);
        const double into = cutTypedAt_ >= 0 ? 1e9 : (t - FADE * 0.5) * TYPE_CPS;   // characters shown so far
        double y = std::min(picBottom + size * 2.0, H - size * 1.45 * lines.size() - 30);
        size_t used = 0;
        for (const auto& ln : lines) {
            const double left = into - static_cast<double>(used);
            used += ln.size() + 1;
            if (left <= 0) break;
            std::string part = ln.substr(0, static_cast<size_t>(std::min<double>(ln.size(), left)));
            // never cut a UTF-8 letter in half
            while (!part.empty() && (static_cast<unsigned char>(part.back()) & 0xC0) == 0x80) part.pop_back();
            if (!part.empty() && (static_cast<unsigned char>(part.back()) & 0xC0) == 0xC0) part.pop_back();
            const double lx = W / 2 - textW(ln, size) / 2;
            shadowText(part, lx, y, pal::ltText, size);
            y += size * 1.45;
        }
    }
    // How to move on, and how far along we are
    const std::string hint = "Space  next     Esc  skip";
    text(hint, W - 24, H - 18, withA(pal::ltMuted, 0.55), 13, 0, Align::Right);
    for (size_t i = 0; i < cs->shots.size(); i++)
        box(24 + i * 14, H - 26, 8, 8, static_cast<int>(i) <= cutShot_ ? withA(pal::ltGold, 0.8) : withA(pal::ltMuted, 0.25));

    // Through black between shots, and out at the end
    g_.setTransform(1);
    double black0 = 0;
    if (t < FADE) black0 = 1 - t / FADE;
    if (t > length - FADE) black0 = std::max(black0, (t - (length - FADE)) / FADE);
    if (cutOut_ >= 0) black0 = std::max(black0, cutOut_ / FADE);
    if (std::find(s.fx.begin(), s.fx.end(), "flash") != s.fx.end() && t < 0.9) g_.fillRect(0, 0, DW, DH, Color(255, 246, 226, static_cast<uint8_t>(230 * (1 - t / 0.9))));
    if (black0 > 0) g_.fillRect(0, 0, DW, DH, withA(black, black0));
    g_.setTransform(ui_);
}

// The backdrop: a sky graded top to bottom, then whatever the scene calls for in front of it, back to front.
void App::drawBackdrop(const ShotDef& s, double t) {
    const Palette p = paletteFor(s);
    const double M = 40;   // painted margin beyond the stage, so camera moves never show an edge
    for (int i = 0; i < 48; i++) {
        const double y = -M + (SH + 2 * M) * i / 48.0;
        g_.fillRect(-M, y, SW + 2 * M, (SH + 2 * M) / 48.0 + 1, mix(p.sky0, p.sky1, static_cast<double>(i) / 47));
    }
    auto has = [&](const char* f) { return std::find(s.fx.begin(), s.fx.end(), f) != s.fx.end(); };
    const bool indoors = s.scene == "crypt" || s.scene == "throne";
    // stars and a moon on clear nights
    if (s.scene == "night" || s.scene == "sea" || s.scene == "peaks" || has("stars")) {
        for (int i = 0; i < 90; i++) {
            const double sx = hash2(i, 3, 11) * (SW + 2 * M) - M, sy = hash2(i, 5, 13) * SH * 0.62 - M * 0.5;
            const double tw = 0.5 + 0.5 * std::sin(t * (1.5 + hash2(i, 7, 17) * 3) + i);
            g_.fillRect(sx, sy, 1, 1, Color(230, 230, 255, static_cast<uint8_t>(60 + 150 * tw * hash2(i, 9, 19))));
        }
        if (s.scene != "peaks") {
            const double mx = SW * 0.76, my = SH * 0.2;
            g_.drawTinted(spr_.glow, mx - 34, my - 34, 68, 68, Color(200, 210, 255, 70), true);
            for (int dy = -9; dy <= 9; dy++) {
                const double w = std::sqrt(81.0 - dy * dy);
                g_.fillRect(mx - w, my + dy, w * 2, 1, Color(226, 228, 236));
            }
            g_.fillRect(mx + 2, my - 4, 3, 2, Color(196, 198, 210));
            g_.fillRect(mx - 5, my + 3, 4, 2, Color(196, 198, 210));
        }
    }
    if (s.scene == "dusk" || s.scene == "dawn" || s.scene == "village" || s.scene == "fire") {   // a low, swollen sun
        const double sx = SW * 0.3, sy = SH * 0.56;
        g_.drawTinted(spr_.glow, sx - 70, sy - 50, 140, 100, withA(mix(p.sky1, Color(255, 230, 180), 0.5), 0.6), true);
        for (int dy = -13; dy <= 13; dy++) {
            const double w = std::sqrt(169.0 - dy * dy);
            g_.fillRect(sx - w, sy + dy, w * 2, 1, mix(Color(255, 226, 170), p.sky1, 0.25));
        }
    }
    if (indoors) {
        // dungeon walls: courses of stone fading into the dark, with pillars and a floor
        const Color stone = mix(p.sky1, Color(70, 62, 76), 0.4), joint = mix(p.sky0, Color(0, 0, 0), 0.4);
        for (int r = 0; r < 12; r++) {
            const double y = -M + r * 14;
            if (y > SH * 0.72) break;
            g_.fillRect(-M, y, SW + 2 * M, 14, mix(stone, p.sky0, 0.15 + 0.06 * (r % 3)));
            g_.fillRect(-M, y + 13, SW + 2 * M, 1, joint);
            for (double x = -M + (r % 2) * 13; x < SW + M; x += 26) g_.fillRect(x, y, 1, 13, joint);
        }
        for (int i = 0; i < 4; i++) {   // pillars
            const double x = SW * (0.08 + 0.28 * i) - 10;
            g_.fillRect(x, -M, 20, SH * 0.72 + M, mix(stone, Color(0, 0, 0), 0.35));
            g_.fillRect(x + 2, -M, 3, SH * 0.72 + M, mix(stone, Color(255, 255, 255), 0.08));
            g_.fillRect(x - 3, SH * 0.72 - 8, 26, 8, mix(stone, Color(0, 0, 0), 0.25));
        }
        g_.fillRect(-M, SH * 0.72, SW + 2 * M, SH * 0.28 + M, p.land);
        for (int i = 0; i < 6; i++) g_.fillRect(-M, SH * 0.72 + i * 9, SW + 2 * M, 1, mix(p.land, Color(0, 0, 0), 0.4));
        if (s.scene == "throne") {   // a throne of bones on a dais, candles either side
            const double tx = SW / 2;
            g_.fillRect(tx - 50, SH * 0.66, 100, 8, mix(stone, Color(0, 0, 0), 0.2));
            g_.fillRect(tx - 40, SH * 0.62, 80, 6, mix(stone, Color(0, 0, 0), 0.1));
            const Color bone(178, 168, 146), boneD(110, 100, 88);
            g_.fillRect(tx - 22, SH * 0.3, 44, SH * 0.32, boneD);
            for (int i = 0; i < 7; i++) g_.fillRect(tx - 22 + i * 7, SH * 0.26 - (i % 2) * 6, 4, 14, bone);
            g_.fillRect(tx - 26, SH * 0.48, 8, SH * 0.14, bone);
            g_.fillRect(tx + 18, SH * 0.48, 8, SH * 0.14, bone);
            for (int side = -1; side <= 1; side += 2) {
                const double fx = tx + side * 70, fy = SH * 0.6;
                g_.fillRect(fx - 2, fy, 4, 12, Color(220, 210, 180));
                const double fl = 1 + 0.25 * std::sin(t * 9 + side);
                g_.drawTinted(spr_.glow, fx - 22 * fl, fy - 26 * fl, 44 * fl, 44 * fl, Color(255, 170, 80, 110), true);
                g_.fillRect(fx - 1, fy - 4 - fl * 2, 2, 4 + fl * 2, Color(255, 214, 120));
            }
        }
        return;
    }
    if (s.scene == "void") return;
    // Far ranges first, paler with distance (aerial perspective), then nearer and darker
    const bool peaks = s.scene == "peaks";
    const int layers = 3;
    for (int L = 0; L < layers; L++) {
        const double depth = static_cast<double>(L) / (layers - 1);   // 0 = far, 1 = near
        const Color c = mix(p.haze, p.land, 0.35 + 0.65 * depth);
        const double base = SH * (0.5 + 0.1 * L), amp = (peaks ? 40 : 18) * (1.2 - depth * 0.5);
        for (double x = -M; x < SW + M; x += 1) {
            double h = ridge(x * (peaks ? 0.8 : 1) + L * 97, 1.7 + L * 3.1);
            if (peaks) h = 0.9 - std::abs(h) * 1.5 + 0.25 * ridge(x * 0.35 + L * 41, 5.3);   // crests, broken by broad shoulders
            const double y = base - (h + 0.5) * amp;
            g_.fillRect(x, y, 1, SH + M - y, c);
            if (peaks && L < 2 && h > 0.5) g_.fillRect(x, y, 1, 2 + (h - 0.5) * 16, mix(Color(222, 228, 242), c, 0.25 + depth * 0.4));   // snow on the tops
        }
    }
    if (s.scene == "sea") {   // the sea, with the moon's road across it
        const double y0 = SH * 0.64;
        g_.fillRect(-M, y0, SW + 2 * M, SH - y0 + M, Color(14, 22, 40));
        for (int i = 0; i < 40; i++) {
            const double y = y0 + 2 + i * 1.6, ph = t * 0.8 + i * 1.7;
            const double w = 14 + 30 * hash2(i, 1, 4);
            const double x = std::fmod(hash2(i, 2, 5) * (SW + 2 * M) + ph * 6, SW + 2 * M) - M;
            g_.fillRect(x, y, w, 1, Color(70, 90, 130, 90));
            g_.fillRect(SW * 0.76 - 6 - (i % 3) * 2 + std::sin(ph) * 3, y, 12 + (i % 4) * 2, 1, Color(210, 214, 230, static_cast<uint8_t>(150 - i * 3)));
        }
    }
    if (s.scene == "village") {   // the houses of Greenhollow on the near hill, windows lit
        static const char* HOUSES[] = {"house_a", "hut", "house_b", "hut", "house_a"};
        for (int i = 0; i < 5; i++) {
            const Sprite& h = spr_.get(HOUSES[i]);
            if (!h) continue;
            const double sc = 0.55, hx = SW * (0.12 + 0.19 * i), hy = SH * 0.78 - h.h * sc;
            g_.drawTinted(h, hx, hy, h.w * sc, h.h * sc, mix(Color(255, 255, 255), p.land, 0.72));
            const double fl = 0.75 + 0.25 * std::sin(t * 3 + i * 2);
            g_.fillRect(hx + h.w * sc * 0.42, hy + h.h * sc * 0.62, 3, 3, Color(255, 200, 110, static_cast<uint8_t>(230 * fl)));
            g_.drawTinted(spr_.glow, hx + h.w * sc * 0.42 - 8, hy + h.h * sc * 0.62 - 8, 19, 19, Color(255, 180, 90, static_cast<uint8_t>(80 * fl)), true);
        }
    }
    if (s.scene == "fields" || s.scene == "fire") {   // dead trees along the near rise
        static const char* TREES[] = {"dead_tree_1", "broken_tree_1", "dead_tree_2", "dead_tree_3"};
        for (int i = 0; i < 4; i++) {
            const Sprite& tr = spr_.undead(TREES[i]);
            if (!tr) continue;
            const double sc = 0.5, tx = SW * (0.06 + 0.27 * i) + (i % 2) * 18, ty = SH * 0.8 - tr.h * sc;
            g_.drawTinted(tr, tx, ty, tr.w * sc, tr.h * sc, mix(Color(255, 255, 255), p.land, 0.85));
        }
    }
    // the ground the actors stand on
    g_.fillRect(-M, SH * 0.8, SW + 2 * M, SH * 0.2 + M, mix(p.land, Color(0, 0, 0), 0.25));
}

// An actor: one of the game's sprites set on the stage, with a simple way of moving.
void App::drawActor(const ShotActor& a, double t, double length) {
    const double tt = t - a.delay;
    if (tt < 0) return;
    const double span = std::max(0.1, length - a.delay - FADE);
    const double k = ease(tt / span);
    double x = a.x * SW, y = a.y * SH;
    if (a.toX >= 0) {
        x += (a.toX * SW - x) * (a.anim == "walk" ? std::clamp(tt / span, 0.0, 1.0) : k);
        y += (a.toY * SH - y) * (a.anim == "walk" ? std::clamp(tt / span, 0.0, 1.0) : k);
    }
    double alpha = 1, rot = 0, sink = 0, sx = 1, sy = 1;
    if (a.anim == "float") y += std::sin(tt * 1.6) * 3 - 2;
    if (a.anim == "fade_in") alpha = std::clamp(tt / 1.8, 0.0, 1.0);
    if (a.anim == "fade_out") alpha = 1 - std::clamp((tt - 1.0) / 2.5, 0.0, 1.0);
    if (a.anim == "sway") rot = std::sin(tt * 1.1) * 0.03;
    if (a.anim == "rise") sink = 1 - ease(tt / 2.6);   // climbs up out of the ground
    if (a.anim == "fall") {                            // stands, then topples and fades
        const double f = std::clamp((tt - 1.2) / 0.9, 0.0, 1.0);
        rot = -1.45 * f * f;
        alpha = 1 - std::clamp((tt - 2.6) / 2.0, 0.0, 1.0);
    }
    if (a.anim == "idle" || a.anim.empty()) {   // breathing: the body swells a hair and settles
        sy = 1 + 0.012 * std::sin(tt * 2.2);
        sx = 1 - 0.006 * std::sin(tt * 2.2);
    }
    const Color tint = mix(Color(255, 255, 255), Color(12, 10, 16), a.dark);

    // Which picture
    const Sprite* img = nullptr;
    if (a.sprite == "hero") {
        const AnimalSet& K = spr_.knight(world_->heroLayers(*sim_));
        const int dir = a.flip ? 2 : 3;   // rows: down, up, left, right
        std::string anim = a.anim == "walk" ? "walk" : a.anim == "slash" ? "slash" : a.anim == "kneel" ? "hurt" : "idle";
        const Frames& f = K.get(anim)[dir];
        if (!f.empty()) {
            size_t fi = 0;
            if (anim == "walk") fi = 1 + static_cast<size_t>(tt * 9) % (f.size() - 1);
            else if (anim == "slash") fi = static_cast<size_t>(tt * 12) % f.size();
            else if (anim == "hurt") fi = std::min(f.size() - 1, static_cast<size_t>(tt * 8));
            else fi = static_cast<size_t>(tt * 3) % f.size();
            img = &f[fi];
        }
    } else if (a.sprite.rfind("enemy:", 0) == 0) {
        if (const EnemyDef* d = db_.findEnemy(a.sprite.substr(6))) {
            const Sprite& m = spr_.monster(d->sprite, d->tint);
            if (m) img = &m;
            else {
                const Frames& f = spr_.character(d->sprite, d->tint).get("idle");
                if (!f.empty()) img = &f[static_cast<size_t>(tt * 6) % f.size()];
            }
        }
    } else if (a.sprite.rfind("undead:", 0) == 0) img = &spr_.undead(a.sprite.substr(7));
    else img = &spr_.get(a.sprite);
    if (!img || !*img) return;

    const double w = img->w * a.scale, h = img->h * a.scale;
    // its shadow on the ground
    if (alpha > 0.05 && a.anim != "float") g_.drawTinted(spr_.glow, x - w * 0.45, y - 3, w * 0.9, 6, Color(0, 0, 0, static_cast<uint8_t>(110 * alpha * (1 - sink))));
    g_.setAlpha(alpha);
    if (rot != 0) {
        g_.drawRotated(*img, x - w / 2, y - h, rot, img->w / 2.0, img->h, a.scale);
    } else if (sink > 0) {   // only the part above ground shows
        const int showH = static_cast<int>(img->h * (1 - sink));
        if (showH > 0) g_.drawPart(*img, {0, 0, img->w, showH}, x - w / 2, y - showH * a.scale, w, showH * a.scale, a.flip && a.sprite != "hero", tint);
        // earth thrown up where it breaks the ground
        for (int i = 0; i < 6; i++) g_.fillRect(x - w * 0.4 + hash2(i, static_cast<int>(tt * 10), 3) * w * 0.8, y - hash2(i, static_cast<int>(tt * 10), 4) * 6, 2, 2, Color(60, 48, 40));
    } else {
        g_.drawTinted(*img, x - w * sx / 2, y - h * sy, w * sx, h * sy, tint, false, a.flip && a.sprite != "hero");
    }
    g_.setAlpha(1);
}

// Particles drifting over the stage, kept between frames and spawned as each effect asks.
void App::drawMotes(const ShotDef& s, double t, double dt) {
    auto has = [&](const char* f) { return std::find(s.fx.begin(), s.fx.end(), f) != s.fx.end(); };
    auto spawn = [&](double x, double y, double vx, double vy, double life, double size, Color c, int kind) {
        if (motes_.size() < 400) motes_.push_back({x, y, vx, vy, life, life, size, c, kind});
    };
    const int salt = static_cast<int>(frameNo_ % 997);
    auto r01 = [&](int k) { return hash2(salt, k, static_cast<int>(motes_.size()) * 3 + 1); };
    if (has("embers") && r01(0) < 0.55)
        for (int i = 0; i < 1; i++) spawn(r01(1) * SW, SH + 4, (r01(2) - 0.5) * 8, -14 - r01(3) * 18, 3 + r01(4) * 3, 1, Color(255, 150 + static_cast<int>(r01(5) * 80), 60), 0);
    if (has("snow"))
        for (int i = 0; i < 2; i++) spawn(r01(6) * (SW + 60) - 30, -6, 4 + r01(7) * 6, 10 + r01(8) * 10, 14, r01(9) < 0.3 ? 2 : 1, Color(236, 240, 250), 1);
    if (has("ash") && r01(10) < 0.7) spawn(r01(11) * (SW + 60) - 30, -6, 6 + r01(12) * 8, 6 + r01(13) * 6, 18, 1, Color(150, 146, 140), 1);
    if (has("rain"))
        for (int i = 0; i < 5; i++) spawn(r01(14 + i) * (SW + 80) - 40, -10, -40, 190, 1.2, 1, Color(150, 170, 200, 150), 2);
    if (has("fireflies") && r01(20) < 0.08) spawn(r01(21) * SW, SH * (0.5 + 0.35 * r01(22)), 0, 0, 5, 1, Color(210, 240, 120), 3);
    if (has("spores") && r01(23) < 0.3) spawn(r01(24) * SW, SH * (0.6 + 0.4 * r01(25)), (r01(26) - 0.5) * 6, -4 - r01(27) * 5, 7, 1, Color(170, 200, 110), 3);
    if (has("bubbles") && r01(28) < 0.4) spawn(r01(29) * SW, SH + 2, 0, -16 - r01(30) * 10, 8, 1, Color(170, 210, 230, 160), 4);
    if (has("mist") && r01(31) < 0.04) spawn(-60, SH * (0.6 + 0.3 * r01(32)), 6 + r01(33) * 6, 0, 30, 50 + r01(34) * 40, Color(190, 196, 210), 5);

    for (auto& m : motes_) {
        m.life -= dt;
        m.x += m.vx * dt;
        m.y += m.vy * dt;
        if (m.kind == 1) m.x += std::sin(m.life * 1.7 + m.y * 0.05) * 0.15;   // flakes swing as they fall
        if (m.kind == 3) {                                                  // fireflies and spores wander
            m.x += std::sin(m.life * 2.3 + m.max) * 0.25;
            m.y += std::cos(m.life * 1.9 + m.max) * 0.2;
        }
        const double f = std::clamp(m.life / m.max, 0.0, 1.0), fadeIn = std::clamp((m.max - m.life) / 0.6, 0.0, 1.0);
        const double a = std::min(f * 2, 1.0) * fadeIn;
        if (m.kind == 2) g_.fillRect(m.x, m.y, 1, 4, withA(m.col, a * m.col.a / 255.0));
        else if (m.kind == 3) {
            const double pulse = 0.5 + 0.5 * std::sin(m.life * 5 + m.max * 3);
            g_.drawTinted(spr_.glow, m.x - 4, m.y - 4, 9, 9, withA(m.col, 0.4 * a * pulse), true);
            g_.fillRect(m.x, m.y, 1, 1, withA(m.col, a * (0.4 + 0.6 * pulse)));
        } else if (m.kind == 5) g_.drawTinted(spr_.glow, m.x - m.size, m.y - m.size * 0.3, m.size * 2, m.size * 0.6, withA(m.col, 0.09 * a), false);
        else if (m.kind == 0) {
            g_.fillRect(m.x, m.y, m.size, m.size, withA(m.col, a));
            g_.drawTinted(spr_.glow, m.x - 3, m.y - 3, 7, 7, withA(m.col, 0.35 * a), true);
        } else g_.fillRect(m.x, m.y, m.size, m.size, withA(m.col, a * m.col.a / 255.0));
    }
    std::erase_if(motes_, [](const Mote& m) { return m.life <= 0 || m.y > SH + 40 || m.y < -60 || m.x > SW + 120; });

    // Effects drawn fresh each frame
    if (has("rays"))   // light falling through a high window
        for (int i = 0; i < 5; i++) {
            const double x = SW * (0.3 + 0.1 * i) + std::sin(t * 0.3 + i) * 4, w = 10 + 6 * (i % 3);
            for (int sgm = 0; sgm < 18; sgm++) {
                const double y = -20 + sgm * 10;
                g_.fillRect(x + sgm * 3, y, w, 10, Color(255, 236, 190, static_cast<uint8_t>(14 * (1 - sgm / 18.0) * (0.7 + 0.3 * std::sin(t + i)))));
            }
        }
    if (has("bell")) {   // the Highwatch bell: rings of sound spreading over the land
        const double period = 2.4;
        for (int ring = 0; ring < 3; ring++) {
            const double age = std::fmod(t + ring * period / 3, period) / period;
            const double r = 8 + age * 150, a = (1 - age) * 0.5;
            const int n = static_cast<int>(r * 1.6);
            for (int i = 0; i < n; i++) {
                const double ang = PI * i / n;
                g_.fillRect(SW * 0.5 + std::cos(ang) * r, SH * 0.32 - std::sin(ang) * r * 0.45, 1, 1, Color(255, 226, 160, static_cast<uint8_t>(255 * a)));
            }
        }
    }
    if (has("rift")) {   // a tear in the air, its edges burning violet
        const double cx = SW * 0.5, cy = SH * 0.52, h = 70 + 6 * std::sin(t * 2);
        g_.drawTinted(spr_.glow, cx - 50, cy - h * 0.8, 100, h * 1.6, Color(150, 90, 230, 120), true);
        for (int i = 0; i < static_cast<int>(h); i++) {
            const double f = static_cast<double>(i) / h, w = std::sin(f * PI) * (6 + 2 * std::sin(t * 5 + i * 0.3));
            g_.fillRect(cx - w, cy - h / 2 + i, w * 2, 1, Color(30, 10, 50));
            g_.fillRect(cx - w - 1, cy - h / 2 + i, 1, 1, Color(210, 160, 255));
            g_.fillRect(cx + w, cy - h / 2 + i, 1, 1, Color(210, 160, 255));
        }
    }
}

}   // namespace hc

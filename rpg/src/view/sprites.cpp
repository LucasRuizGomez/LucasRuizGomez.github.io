#include "view/sprites.h"

#include "core/files.h"

#include <algorithm>
#include <cmath>

namespace hc {

namespace {
const Sprite EMPTY_SPRITE;
const Frames EMPTY_FRAMES{Sprite()};
const std::array<Frames, 4> EMPTY_ROWS{Frames{Sprite()}, Frames{Sprite()}, Frames{Sprite()}, Frames{Sprite()}};
bool isWhite(const std::string& tint) { return tint.empty() || tint == "#ffffff"; }
}   // namespace

const Frames& CharSet::get(const std::string& name) const {
    auto it = anims.find(name);
    if (it != anims.end() && !it->second.empty()) return it->second;
    it = anims.find("idle");
    return it != anims.end() && !it->second.empty() ? it->second : EMPTY_FRAMES;
}

const std::array<Frames, 4>& AnimalSet::get(const std::string& name) const {
    auto it = anims.find(name);
    if (it != anims.end()) return it->second;
    it = anims.find("idle");
    return it != anims.end() ? it->second : EMPTY_ROWS;
}

Frames Sprites::strip(const Image& im, int fw, int fh, int row) const {
    Frames out;
    if (im.empty()) {
        out.push_back(Sprite());
        return out;
    }
    const int n = std::max(1, im.w / fw);
    for (int i = 0; i < n; i++) out.push_back(g_->makeSprite(im.crop(i * fw, row * fh, fw, fh)));
    return out;
}

const Sprite& Sprites::get(const std::string& name) const {
    auto it = named_.find(name);
    return it == named_.end() ? EMPTY_SPRITE : it->second;
}
const Frames& Sprites::frames(const std::string& name) const {
    auto it = strips_.find(name);
    return it == strips_.end() ? EMPTY_FRAMES : it->second;
}
const Sprite& Sprites::icon(const std::string& id) const {
    auto it = icons_.find(id);
    if (it != icons_.end()) return it->second;
    it = icons_.find("wood");
    return it == icons_.end() ? EMPTY_SPRITE : it->second;
}

void Sprites::build(Gfx& g, const Database& db, const std::string& root) {
    g_ = &g;
    root_ = root;
    const json& cfg = db.sprites;
    if (cfg.contains("grade")) {   // the overall colour grade of the art
        const json& gr = cfg.at("grade");
        Image::grade.saturation = gr.value("saturation", 1.0);
        Image::grade.brightness = gr.value("brightness", 1.0);
        Image::grade.contrast = gr.value("contrast", 1.0);
        Image::grade.tint = hex(gr.value("tint", std::string("#ffffff")));
        Image::grade.skip = gr.value("skip", std::vector<std::string>{});
    }
    std::map<std::string, std::string> paths;
    for (auto it = cfg.at("images").begin(); it != cfg.at("images").end(); ++it) paths[it.key()] = root + it.value().get<std::string>();
    const std::string fort = paths["fort"], hunt = paths["hunt"], rock = paths["rock"], uiDir = paths["ui"], gen = paths["gen"];

    // ── simple sprites from sprites.json ──
    std::map<std::string, Image> sheets;
    auto sheet = [&](const std::string& alias) -> const Image& {
        auto it = sheets.find(alias);
        if (it == sheets.end()) it = sheets.emplace(alias, Image::load(paths[alias])).first;
        return it->second;
    };
    for (auto it = cfg.at("sprites").begin(); it != cfg.at("sprites").end(); ++it) {
        const json& s = it.value();
        const json& at = s.at("at");
        const Image& src = sheet(at.at(0).get<std::string>());
        const int x = at.at(1), y = at.at(2), w = at.at(3), h = at.at(4);
        const bool raw = s.value("raw", false);
        const int n = s.value("frames", 1), step = s.value("step", w);
        auto cut = [&](int i) {
            Image im = src.crop(x + i * step, y, w, h);
            if (!raw) im = im.trim();
            if (s.contains("shade")) im = im.shade(hex(s.at("shade").get<std::string>()));
            return im;
        };
        const std::string name = it.key();
        if (n > 1) {
            Frames fr;
            for (int i = 0; i < n; i++) fr.push_back(g.makeSprite(cut(i)));
            named_[name] = fr.front();
            strips_[name] = std::move(fr);
        } else if (name.rfind("icon.", 0) == 0) icons_[name.substr(5)] = g.makeSprite(cut(0));
        else named_[name] = g.makeSprite(cut(0));
    }
    {   // The second house model has no door in the art: give it the arched door of the first
        const Image& ow = sheet("ow");
        Image hb = ow.crop(176, 0, 80, 80);
        hb.fill(30, 52, 20, 26, Color(0, 0, 0, 0));
        hb.blitPart(ow, 96 + 30, 52, 20, 26, 30, 52);
        named_["house_b_door"] = g.makeSprite(hb.trim());
    }
    water = strips_["water"];
    for (const auto& f : strips_["waterb"]) water.push_back(f);
    waves = strips_["wave"];
    plaza = named_["plaza"];
    cobble = named_["cobble"];

    // ── fortress kit: tiles, objects, characters ──
    tileSheet_ = Image::load(fort + "Tileset/Tiled_Map.png");
    auto t = [&](int x, int y, int w, int h) { return tileSheet_.crop(x, y, w, h); };
    const Image flowerIm[4] = {t(212, 164, 9, 9), t(227, 164, 9, 7), t(245, 166, 5, 4), t(257, 163, 13, 10)};
    for (const auto& im : flowerIm) flowers.push_back(g.makeSprite(im));
    for (const Image& im : {t(257, 6, 15, 8), t(242, 8, 11, 6), t(228, 38, 8, 5), t(210, 36, 12, 10)}) decals.push_back(g.makeSprite(im));
    const Image logIm = t(224, 112, 16, 16);
    named_["log"] = g.makeSprite(logIm);
    named_["fence"] = g.makeSprite(t(160, 113, 21, 31).trim());
    named_["arch"] = g.makeSprite(t(96, 16, 16, 32));

    chest = strip(Image::load(fort + "Tileset/Chest.png"), 32, 32);
    const Image meatSheet = Image::load(fort + "Tileset/Meat.png");
    campMeat = strip(meatSheet, 32, 32);
    const Image torchSheet = Image::load(fort + "Tileset/Torch.png");
    torch = strip(torchSheet, 16, 16);
    gate = strip(Image::load(fort + "Tileset/Gate.png"), 80, 80);
    const Image knifeSheet = Image::load(fort + "Characters/Knife.png");
    knife = strip(knifeSheet, 16, 16);
    Image bolt = Image::load(fort + "Characters/Charge_1.png");
    if (bolt.empty()) bolt = Image(15, 16);
    Image diag = Image::load(fort + "Characters/Charge_2.png");   // points down-left
    if (diag.empty()) diag = bolt;
    // index = 45 degree step clockwise from "right"
    const Image b8[8] = {bolt.flipX(), diag.rot90(3), bolt.rot90(3), diag, bolt, diag.rot90(1), bolt.rot90(1), diag.rot90(2)};
    for (int i = 0; i < 8; i++) bolt8[i] = g.makeSprite(b8[i]);

    static const struct {
        const char *file, *key;
    } CHAR_ANIMS[] = {{"Idle", "idle"},     {"Walk", "walk"},       {"Run", "run"},          {"Attack_1", "atk"}, {"Attack", "atk"}, {"Attack_2", "thrust"},
                      {"Dead", "dead"},     {"Shot", "shot"},       {"Recharge", "reload"},  {"Run+Attack", "dash"}, {"Cast", "cast"}};
    for (int n = 1; n <= 9; n++)
        for (const auto& a : CHAR_ANIMS) {
            const std::string path = fort + "Characters/Character_" + std::to_string(n) + "/" + a.file + ".png";
            if (FILE* f = std::fopen(path.c_str(), "rb")) {
                std::fclose(f);
                charSheets_["ch" + std::to_string(n) + "." + a.key] = Image::load(path);
            }
        }

    // ── vampires (CraftPix), cut to 32x32 frames by tools/import_dark_art.py; rows down, up, left, right ──
    for (int v = 1; v <= 3; v++)
        for (const char* a : {"idle", "walk", "run", "attack", "dead", "hurt"}) {
            const std::string path = gen + "vampires/vampire" + std::to_string(v) + "." + a + ".png";
            if (FILE* f = std::fopen(path.c_str(), "rb")) {
                std::fclose(f);
                animalSheets_["vampire" + std::to_string(v) + "." + (std::string(a) == "attack" ? "atk" : a)] = Image::load(path);
            }
        }

    // ── hunt pack animals ──
    static const struct {
        const char *key, *dir;
        const char* anims[5][2];
    } ANIMALS[] = {{"boar", "Boar", {{"idle", "Idle"}, {"walk", "Walk"}, {"run", "Run"}, {"atk", "Attack"}, {"dead", "Death"}}},
                   {"fox", "Fox", {{"idle", "Idle"}, {"walk", "walk"}, {"run", "Run"}, {"dead", "Death"}, {nullptr, nullptr}}},
                   {"hare", "Hare", {{"idle", "Idle"}, {"walk", "Walk"}, {"run", "Run"}, {"dead", "Death"}, {nullptr, nullptr}}},
                   {"deer", "Deer", {{"idle", "Idle"}, {"walk", "Walk"}, {"run", "Run"}, {"dead", "Death"}, {nullptr, nullptr}}},
                   {"grouse", "Black_grouse", {{"idle", "Flight"}, {"walk", "Flight"}, {"dead", "Death"}, {nullptr, nullptr}, {nullptr, nullptr}}}};
    for (const auto& a : ANIMALS)
        for (const auto& an : a.anims)
            if (an[0]) animalSheets_[std::string(a.key) + "." + an[0]] = Image::load(hunt + a.dir + "/" + a.dir + "_" + an[1] + "_with_shadow.png");
    treantSheet_ = Image::load(paths["treant"]);

    // ── rocks ──
    auto rk = [&](int i, int v) { return Image::load(rock + "Rock" + std::to_string(i) + "_" + std::to_string(v) + ".png").trim(); };
    auto rocks = [&](std::initializer_list<std::pair<int, int>> list) {
        Frames out;
        for (const auto& [i, v] : list) out.push_back(g.makeSprite(rk(i, v)));
        return out;
    };
    boulders["snow"] = rocks({{3, 3}, {3, 4}, {5, 3}});
    boulders["grey"] = rocks({{5, 3}, {5, 4}});
    boulders["sand"] = rocks({{1, 3}, {1, 4}});
    bigBoulders["snow"] = rocks({{3, 2}});
    bigBoulders["grey"] = rocks({{5, 2}});
    bigBoulders["sand"] = rocks({{1, 2}});
    nodes["stone"] = rocks({{4, 3}, {4, 4}, {4, 5}});
    nodes["iron"] = rocks({{2, 3}, {2, 4}, {2, 5}});
    nodes["coal"] = rocks({{8, 4}, {8, 3}, {8, 5}});
    nodes["herb"] = {flowers[3], flowers[0]};
    for (const char* f : {"Rock7_water_shadow_dark3.png", "Rock7_water_shadow_dark4.png"}) {
        const Image im = Image::load(rock + f);
        if (!im.empty()) waterRocks.push_back(g.makeSprite(im));
    }

    // ── fortress UI glyphs: hearts, alert mark, bandage ──
    const Image fortUi = Image::load(fort + "UI/UI.png");
    auto U = [&](int i, int j) { return fortUi.crop(16 + i * 16, 16 + j * 16, 16, 16).trim(); };
    const Color ink(22, 18, 31);
    const Image gHeart = U(5, 4);
    const Image heartEmpty = gHeart.outline(Color(58, 47, 69), ink), heartFull = gHeart.outline(Color(224, 80, 60), ink);
    Image heartHalf = heartEmpty;
    heartHalf.blitPart(heartFull, 0, 0, (heartFull.w + 1) / 2, heartFull.h, 0, 0);
    heart[0] = g.makeSprite(heartEmpty);
    heart[1] = g.makeSprite(heartHalf);
    heart[2] = g.makeSprite(heartFull);
    alert = g.makeSprite(U(4, 2).outline(Color(255, 90, 74), ink));

    // ── item icons that are composed rather than simply cut out ──
    auto setIcon = [&](const std::string& id, const Image& im) {
        if (!icons_.count(id) && !im.empty()) icons_[id] = g.makeSprite(im);
    };
    setIcon("stone", rk(4, 5));
    setIcon("iron", rk(2, 5));
    setIcon("coal", rk(8, 5));
    setIcon("wood", logIm);
    setIcon("herb", flowerIm[3]);
    const Image meat0 = meatSheet.crop(0, 0, 32, 32);
    setIcon("meat", meat0.crop(9, 8, 15, 10).shade(Color(255, 176, 160)));
    setIcon("roast", meat0.crop(2, 8, 30, 12).trim());
    setIcon("torch", torchSheet.crop(0, 0, 16, 16).trim());
    setIcon("bandage", U(0, 0).outline(Color(224, 80, 60), Color(236, 230, 216)));
    setIcon("arrows", bolt.trim());
    setIcon("gold", U(6, 0).outline(Color(242, 210, 122), ink));
    setIcon("crown", U(0, 3).outline(Color(242, 210, 122), ink));
    if (auto it = charSheets_.find("ch5.idle"); it != charSheets_.end()) {   // spearhead and shaft from the spearman
        Image spear(7, 20);
        spear.blitPart(it->second, 7, 5, 7, 8, 0, 0);
        spear.blitPart(it->second, 9, 13, 3, 12, 2, 8);
        setIcon("steel_spear", spear);
    }
    if (auto it = animalSheets_.find("boar.idle"); it != animalSheets_.end()) setIcon("mount_boar", it->second.crop(0, 96, 32, 32).trim());
    if (auto it = animalSheets_.find("deer.idle"); it != animalSheets_.end()) setIcon("mount_stag", it->second.crop(0, 96, 32, 32).trim().shade(Color(216, 228, 240)));
    if (auto it = animalSheets_.find("grouse.idle"); it != animalSheets_.end()) setIcon("mount_roc", it->second.crop(0, 0, 32, 26).trim().shade(Color(240, 200, 120)));
    icons_["levelup"] = named_["mark_new"];
    icons_["shrine"] = named_["statue"];
    icons_["quest"] = named_["mark_done"];

    // Weapon sprites for the swing, rotated per facing (up, right, down, left)
    for (const auto& d : db.items) {
        if (d.type != ItemType::Weapon) continue;
        Image base;
        if (d.id == "steel_spear") {
            if (auto it = charSheets_.find("ch5.idle"); it != charSheets_.end()) {
                base = Image(7, 20);
                base.blitPart(it->second, 7, 5, 7, 8, 0, 0);
                base.blitPart(it->second, 9, 13, 3, 12, 2, 8);
            }
        } else {
            const json& s = cfg.at("sprites");
            const std::string key = "icon." + d.id;
            if (s.contains(key)) {
                const json& at = s.at(key).at("at");
                base = sheet(at.at(0).get<std::string>()).crop(at.at(1), at.at(2), at.at(3), at.at(4)).trim();
                if (s.at(key).contains("shade")) base = base.shade(hex(s.at(key).at("shade").get<std::string>()));
                if (s.at(key).value("diag", false)) {   // turn a quarter of a right angle anticlockwise, blade up
                    const int n = static_cast<int>(std::ceil(std::hypot(base.w, base.h)));
                    Image up(n, n);
                    const double c = std::cos(PI / 4), sn = std::sin(PI / 4), cx = base.w / 2.0, cy = base.h / 2.0;
                    for (int y = 0; y < n; y++)
                        for (int x = 0; x < n; x++) {
                            const double dx = x + 0.5 - n / 2.0, dy = y + 0.5 - n / 2.0;
                            const int sx = static_cast<int>(std::floor(c * dx - sn * dy + cx)), sy = static_cast<int>(std::floor(sn * dx + c * dy + cy));
                            if (sx < 0 || sy < 0 || sx >= base.w || sy >= base.h) continue;
                            std::copy(base.at(sx, sy), base.at(sx, sy) + 4, up.at(x, y));
                        }
                    base = up.trim();
                }
            }
        }
        if (base.empty()) continue;
        for (int q = 0; q < 4; q++) weaponRot[d.id][q] = g.makeSprite(base.rot90(q));
    }

    // ── interface pieces from the RPG UI pack ──
    const Image uiTiles = Image::load(uiDir + "Main_tiles.png"), uiInv = Image::load(uiDir + "Inventory.png"), uiButtons = Image::load(uiDir + "Buttons.png");
    // Dark iron-bound leather, drawn here: panels with brass rims and rivets, sunken slots, oxblood buttons
    (void)uiTiles;
    (void)uiButtons;
    auto frame = [](int w, int h, Color fill, Color rim, Color hi, Color lo, bool rivets) {
        Image im(w, h);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                Color c = fill;
                const int e = std::min({x, y, w - 1 - x, h - 1 - y});
                if (e == 0) c = Color(10, 8, 7);
                else if (e == 1) c = (x == 1 || y == 1) ? hi : rim;
                else if (e == 2) c = (x == w - 3 || y == h - 3) ? hi : lo;
                else if (e == 3) c = Color(12, 10, 9);
                im.fill(x, y, 1, 1, c);
            }
        if (rivets)
            for (auto [rx, ry] : {std::pair{2, 2}, {w - 4, 2}, {2, h - 4}, {w - 4, h - 4}}) {
                im.fill(rx, ry, 2, 2, hi);
                im.fill(rx + 1, ry + 1, 1, 1, lo);
            }
        return im;
    };
    ui.panel = g.makeSprite(frame(48, 40, Color(36, 29, 25), Color(112, 88, 52), Color(176, 140, 78), Color(70, 54, 34), true));
    auto well = [](int w, int h, Color fill, Color edge, Color hi) {
        Image im(w, h);
        im.fill(0, 0, w, h, edge);
        im.fill(1, 1, w - 2, h - 2, Color(8, 7, 6));
        im.fill(2, 2, w - 3, h - 3, fill);
        im.fill(2, h - 2, w - 3, 1, hi);
        im.fill(w - 2, 2, 1, h - 3, hi);
        return im;
    };
    ui.slot = g.makeSprite(well(14, 14, Color(26, 21, 18), Color(96, 76, 50), Color(58, 46, 32)));
    ui.slotDark = g.makeSprite(well(14, 14, Color(15, 12, 11), Color(60, 48, 34), Color(34, 28, 22)));
    ui.btn = g.makeSprite(frame(42, 15, Color(74, 34, 28), Color(110, 84, 50), Color(160, 124, 70), Color(52, 26, 22), false));
    ui.btnSel = g.makeSprite(frame(42, 15, Color(120, 50, 32), Color(176, 138, 70), Color(236, 196, 110), Color(84, 36, 24), false));
    ui.close = g.makeSprite(uiInv.crop(206, 3, 7, 7));

    // Globes: a dark hollow, the liquid (drawn cut to how full it is), a glass highlight and a brass ring
    {
        const int N = 48;
        const double c = (N - 1) / 2.0, R = 20.5;
        auto disc = [&](auto shadeAt) {
            Image im(N, N);
            for (int y = 0; y < N; y++)
                for (int x = 0; x < N; x++) {
                    const double d = std::hypot(x - c, y - c);
                    if (d > R) continue;
                    const Color col = shadeAt(x, y, d / R);
                    std::copy(&col.r, &col.r + 4, im.at(x, y));
                }
            return im;
        };
        auto liquid = [&](Color base) {
            return disc([&](int x, int y, double t) {
                // brighter in the middle and towards the top, swirls of darker colour
                const double sw = 0.5 + 0.5 * std::sin(x * 0.45 + std::sin(y * 0.3) * 2.2);
                const double k = std::clamp((1.0 - t * 0.55) * (0.75 + 0.35 * (1 - y / static_cast<double>(N))) * (0.88 + 0.12 * sw), 0.0, 1.4);
                return Color(static_cast<uint8_t>(std::clamp(base.r * k, 0.0, 255.0)), static_cast<uint8_t>(std::clamp(base.g * k, 0.0, 255.0)),
                             static_cast<uint8_t>(std::clamp(base.b * k, 0.0, 255.0)), 255);
            });
        };
        ui.orbBack = g.makeSprite(disc([](int, int, double t) { return Color(static_cast<uint8_t>(14 + 8 * (1 - t)), 10, 10, 255); }));
        ui.orbRed = g.makeSprite(liquid(Color(196, 26, 22)));
        ui.orbViolet = g.makeSprite(liquid(Color(110, 60, 190)));
        Image glass(N, N), ring(N, N);
        for (int y = 0; y < N; y++)
            for (int x = 0; x < N; x++) {
                const double d = std::hypot(x - c, y - c);
                // a crescent of light high on the left
                const double hd = std::hypot((x - (c - 6)) / 1.25, y - (c - 9));
                if (d < R - 1 && hd < 9) {
                    const double a = std::clamp((9 - hd) / 9, 0.0, 1.0) * 0.55;
                    uint8_t* p = glass.at(x, y);
                    p[0] = p[1] = p[2] = 255;
                    p[3] = static_cast<uint8_t>(a * 255 * (d > R - 5 ? 0.4 : 1));
                }
                if (d > R - 2.5 && d < R + 3.5) {   // the brass ring, lit from the top left
                    const double lit = 0.5 + 0.5 * ((c - x) + (c - y)) / (std::max(1.0, d) * 1.414);
                    const bool edge = d > R + 2.5 || d < R - 1.5;
                    const Color b = edge ? Color(12, 9, 7) : Color(static_cast<uint8_t>(90 + 120 * lit), static_cast<uint8_t>(68 + 92 * lit), static_cast<uint8_t>(36 + 46 * lit));
                    std::copy(&b.r, &b.r + 3, ring.at(x, y));
                    ring.at(x, y)[3] = 255;
                }
            }
        for (int i = 0; i < 4; i++) {   // four rivets around the rim
            const double a = PI / 4 + i * PI / 2;
            const int rx = static_cast<int>(std::lround(c + std::cos(a) * (R + 0.5))), ry = static_cast<int>(std::lround(c + std::sin(a) * (R + 0.5)));
            ring.fill(rx - 1, ry - 1, 2, 2, Color(240, 210, 140));
        }
        ui.orbGlass = g.makeSprite(glass);
        ui.orbFrame = g.makeSprite(ring);
    }

    // ── water effects ──
    const Image ripple = Image::load(gen + "water_ripple.png"), splash = Image::load(gen + "water_splash.png"), whirl = Image::load(gen + "water_whirlpool.png");
    wRipple = strip(ripple, 30, std::max(1, ripple.h));
    wSplash = strip(splash, 26, std::max(1, splash.h));
    wWhirl = strip(whirl, 40, std::max(1, whirl.h));

    // Soft round glow (elites, loot, shrines)
    Image glowIm(64, 64);
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            const double d = std::hypot(x - 31.5, y - 31.5) / 32.0;
            uint8_t* p = glowIm.at(x, y);
            p[0] = p[1] = p[2] = 255;
            p[3] = static_cast<uint8_t>(255 * std::clamp(1.0 - d, 0.0, 1.0) * std::clamp(1.0 - d, 0.0, 1.0));
        }
    glow = g.makeSprite(glowIm);
}

const CharSet& Sprites::character(const std::string& sprite, const std::string& tint) {
    const std::string key = sprite + "|" + tint;
    if (auto it = chars_.find(key); it != chars_.end()) return it->second;
    CharSet set;
    const std::string prefix = sprite + ".";
    for (const auto& [name, im] : charSheets_) {
        if (name.rfind(prefix, 0) != 0) continue;
        set.anims[name.substr(prefix.size())] = strip(isWhite(tint) ? im : im.shade(hex(tint)), 32, 32);
    }
    if (!set.anims.count("walk") && set.anims.count("run")) set.anims["walk"] = set.anims["run"];
    return chars_[key] = std::move(set);
}

const CharSet& Sprites::hero(const std::vector<std::string>& layers) {
    std::string key = "doll";
    for (const auto& l : layers) key += "|" + l;
    if (auto it = chars_.find(key); it != chars_.end()) return it->second;
    Image doll(32, 32);
    for (const auto& l : layers) {
        const Image part = Image::load(root_ + "assets/dcss/doll/" + l + ".png");
        if (!part.empty()) doll.blit(part, 0, 0);
    }
    // The doll faces left; the sets face right and are flipped for the left
    doll = doll.flipX();
    auto shifted = [&](int dy) {
        Image im(32, 32);
        im.blit(doll, 0, dy);
        return g_->makeSprite(im);
    };
    CharSet set;
    set.anims["idle"] = {shifted(0), shifted(0), shifted(1), shifted(0)};   // breathing
    set.anims["walk"] = {shifted(0), shifted(-1), shifted(0), shifted(-1)};  // a step with a bob
    set.anims["atk"] = {shifted(0), shifted(1), shifted(1), shifted(0)};
    set.anims["thrust"] = {shifted(0), shifted(1)};
    Frames dead;
    for (int i = 0; i < 4; i++) {   // topples over
        Image im(32, 32);
        im.blit(i < 2 ? doll : doll.rot90(1), 0, i < 2 ? i * 2 : 6);
        dead.push_back(g_->makeSprite(im));
    }
    set.anims["dead"] = dead;
    return chars_[key] = std::move(set);
}

const AnimalSet& Sprites::knight(const std::vector<std::string>& layers) {
    std::string key = "knight";
    for (const auto& l : layers) key += "|" + l;
    if (auto it = animals_.find(key); it != animals_.end()) return it->second;
    AnimalSet set;
    for (const char* anim : {"walk", "slash", "hurt", "idle", "shoot"}) {
        Image sheet;
        for (const auto& l : layers) {
            const Image part = Image::load(root_ + "assets/lpc/" + l + "." + anim + ".png");
            if (part.empty()) continue;
            if (sheet.empty()) sheet = Image(part.w, part.h);
            if (part.w == sheet.w && part.h == sheet.h) sheet.blit(part, 0, 0);
        }
        if (sheet.empty()) continue;
        std::array<Frames, 4> rows;
        for (int r = 0; r < 4; r++) rows[r] = strip(sheet, 40, 40, r);
        set.anims[anim] = std::move(rows);
    }
    return animals_[key] = std::move(set);
}

const Sprite& Sprites::monster(const std::string& name, const std::string& tint) {
    const std::string key = "m|" + name + "|" + tint;
    if (auto it = loose_.find(key); it != loose_.end()) return it->second;
    Image im = Image::load(root_ + "assets/dcss/" + name + ".png");
    if (!isWhite(tint) && !im.empty()) im = im.shade(hex(tint));
    return loose_[key] = im.empty() ? Sprite() : g_->makeSprite(im);
}

const Sprite& Sprites::undead(const std::string& name) {
    const std::string key = "u|" + name;
    if (auto it = loose_.find(key); it != loose_.end()) return it->second;
    const Image im = Image::load(root_ + "assets/undead/" + name + ".png");
    return loose_[key] = im.empty() ? Sprite() : g_->makeSprite(im.trim());
}

const AnimalSet& Sprites::animal(const std::string& sprite, const std::string& tint) {
    const std::string key = sprite + "|" + tint;
    if (auto it = animals_.find(key); it != animals_.end()) return it->second;
    AnimalSet set;
    const std::string prefix = sprite + ".";
    for (const auto& [name, im] : animalSheets_) {
        if (name.rfind(prefix, 0) != 0) continue;
        const Image src = isWhite(tint) ? im : im.shade(hex(tint));
        std::array<Frames, 4> rows;
        for (int r = 0; r < 4; r++) rows[r] = strip(src, 32, 32, r);
        // Fox_Run.png stores right before left (every other sheet is left, right)
        if (name == "fox.run") std::swap(rows[2], rows[3]);
        set.anims[name.substr(prefix.size())] = std::move(rows);
    }
    return animals_[key] = std::move(set);
}

const std::array<Frames, 4>& Sprites::treant(const std::string& tint) {
    if (auto it = treants_.find(tint); it != treants_.end()) return it->second;
    const Image src = isWhite(tint) ? treantSheet_ : treantSheet_.shade(hex(tint));
    std::array<Frames, 4> rows;
    for (int r = 0; r < 4; r++)
        for (int i = 0; i < 4; i++) rows[r].push_back(g_->makeSprite(src.crop(i * 32, r * 32, 32, 32)));
    return treants_[tint] = std::move(rows);
}

const TileSet& Sprites::tiles(const std::string& tint) {
    if (auto it = tileSets_.find(tint); it != tileSets_.end()) return it->second;
    // "snow:#rrggbb" bleaches the ground instead of just tinting it
    const bool snow = tint.rfind("snow:", 0) == 0;
    const Image src = snow ? tileSheet_.frost(hex(tint.substr(5))) : isWhite(tint) ? tileSheet_ : tileSheet_.shade(hex(tint));
    auto t = [&](int x, int y, int w = 16, int h = 16) { return src.crop(x, y, w, h); };
    const Color dusk(90, 86, 112);
    TileSet ts;
    ts.sheet = g_->makeSprite(src);
    for (int i = 0; i < 4; i++) ts.dirt[i] = g_->makeSprite(t(239 + (i & 1) * 16, 191 + (i >> 1) * 16));
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++) ts.slab[j * 3 + i] = g_->makeSprite(t(96 + i * 16, 64 + j * 16));
    static const int BX[4] = {20, 32, 44, 56}, TX[4] = {24, 32, 40, 56};
    for (int i = 0; i < 4; i++) {
        ts.brickHi[i] = g_->makeSprite(t(BX[i], 112));
        ts.brickLo[i] = g_->makeSprite(t(BX[i], 128));
        ts.wallTop[i] = g_->makeSprite(t(TX[i], 80).shade(dusk));
    }
    const Image stairs = t(176, 192), deck = t(104, 136), ladder = t(128, 120);
    ts.stairsUp = g_->makeSprite(stairs);
    ts.stairsDown = g_->makeSprite(stairs.rot90(2).shade(dusk));
    ts.slit = g_->makeSprite(t(128, 32));
    ts.deck = g_->makeSprite(deck);
    ts.deckH = g_->makeSprite(deck.rot90(1));
    ts.bridgeV = g_->makeSprite(ladder);
    ts.bridgeH = g_->makeSprite(ladder.rot90(1));
    return tileSets_[tint] = ts;
}

const TreeSet& Sprites::trees(const std::string& tint) {
    if (auto it = treeSets_.find(tint); it != treeSets_.end()) return it->second;
    auto t = [&](int x, int y, int w, int h) {
        const Image im = tileSheet_.crop(x, y, w, h).trim();
        return g_->makeSprite(isWhite(tint) ? im : im.shade(hex(tint)));
    };
    TreeSet ts;
    ts.oak = t(160, 49, 32, 45);
    ts.oakM = t(193, 65, 30, 31);
    ts.pine = t(241, 48, 31, 48);
    ts.pineS = t(272, 64, 16, 32);
    ts.small = t(224, 72, 16, 23);
    return treeSets_[tint] = ts;
}

}   // namespace hc

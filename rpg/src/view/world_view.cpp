#include "view/world_view.h"

#include "core/rng.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace hc {

std::string toHex(Color c) {
    char buf[8];
    std::snprintf(buf, sizeof buf, "#%02x%02x%02x", c.r, c.g, c.b);
    return buf;
}

namespace {

const Sprite& anim(const Frames& f, int t, int speed) { return f[static_cast<size_t>((std::max(0, t) / speed) % static_cast<int>(f.size()))]; }
// Frame for an action that plays once over `total` ticks with `left` remaining
const Sprite& progress(const Frames& f, int total, int left) {
    const int i = static_cast<int>(static_cast<double>(total - left) / total * f.size());
    return f[static_cast<size_t>(std::clamp(i, 0, static_cast<int>(f.size()) - 1))];
}
int animalRow(Dir d) { return d == Dir::Down ? 0 : d == Dir::Up ? 1 : d == Dir::Left ? 2 : 3; }
int treantRow(Dir d) { return d == Dir::Down ? 0 : d == Dir::Right ? 1 : d == Dir::Up ? 2 : 3; }
bool grassy(uint8_t t) { return t == GRASS || t == FLOWERS || t == TREE; }
// A cliff tile is a face (rock hanging below high ground) when higher ground is one or two tiles above it;
// otherwise it is the lip of the high ground, which is grass with rocks along the drop.
bool cliffFace(const Map& m, int c, int r) {
    if (m.get(c, r) != CLIFF && m.get(c, r) != RAMP) return false;
    const int h = m.heightAt(c, r);
    return m.heightAt(c, r - 1) > h || m.heightAt(c, r - 2) > h;
}
bool grassyAt(const Map& m, int c, int r) {
    const uint8_t t = m.get(c, r);
    return grassy(t) || (t == CLIFF && !cliffFace(m, c, r));
}
double frand() { return std::rand() / (RAND_MAX + 1.0); }
// How far the wind bends a plant at (x, y), from -1 to 1: gusts roll across the land from the west, and every
// plant also stirs a little on its own. Indoors there is no wind, only a slow stir.
double wind(int tick, double x, double y, bool outdoor) {
    if (!outdoor) return 0.6 * std::sin(tick * 0.02 + x * 0.37 + y * 0.71);
    return 0.75 * std::sin(tick * 0.03 - x * 0.015 - y * 0.006) + 0.25 * std::sin(tick * 0.07 + x * 0.37 + y * 0.71);
}
bool swaysInWind(const char* name) {
    for (const char* p : {"plant_", "thorn_plant_", "dead_arm_", "broken_tree_"})
        if (std::string(name).rfind(p, 0) == 0) return true;
    return false;
}

Color miniColor(uint8_t t) {
    switch (t) {
        case GRASS:
        case FLOWERS: return {163, 177, 58};
        case WATER: return {60, 127, 166};
        case TREE: return {91, 116, 40};
        case PATH:
        case SAND: return {196, 124, 76};
        case ROCK: return {170, 166, 184};
        case BRIDGE: return {122, 74, 44};
        case FLOOR:
        case PLANKS: return {142, 138, 166};
        case WALL: return {52, 49, 63};
        case DOOR: return {154, 106, 60};
        case PLAZA:
        case COBBLE: return {214, 210, 200};
        case CLIFF: return {72, 64, 58};
        case RAMP: return {150, 140, 120};
        default: return {242, 210, 122};
    }
}

}   // namespace

void WorldView::forget() {
    for (auto& [id, v] : views_) {
        for (auto& c : v.chunks) g_.destroy(c);
        g_.destroy(v.mini);
        g_.destroy(v.miniFull);
    }
    views_.clear();
}

WorldView::MapView& WorldView::view(Sim& sim, Map& m) {
    auto it = views_.find(m.id);
    if (it != views_.end()) return it->second;
    MapView& v = views_[m.id];
    v.cw = (m.w + CHUNK - 1) / CHUNK;
    v.ch = (m.h + CHUNK - 1) / CHUNK;
    v.chunks.resize(static_cast<size_t>(v.cw) * v.ch);
    v.baked.assign(v.chunks.size(), 0);
    if (m.outdoor)
        for (const auto& z : db_.zones) {
            v.zoneTiles.push_back(&spr_.tiles((z.snow ? "snow:" : "") + toHex(z.ground)));
            v.zoneTrees.push_back(&spr_.trees(toHex(z.foliage)));
        }
    else {
        v.zoneTiles.push_back(&spr_.tiles(m.dungeon ? m.dungeon->tint : ""));
        v.zoneTrees.push_back(&spr_.trees(""));
    }
    v.props.assign(m.h, {});
    for (int r = 0; r < m.h; r++)
        for (int c = 0; c < m.w; c++) {
            Prop p;
            if (propFor(m, v, c, r, p)) v.props[r].push_back(p);
        }
    m.dirty.clear();
    m.exploredDirty = true;
    (void)sim;
    return v;
}

const TileSet& WorldView::tilesAt(Map& m, MapView& v, int c, int r) {
    if (!m.outdoor || m.zone.empty()) return *v.zoneTiles.front();
    return *v.zoneTiles[m.zone[m.idx(std::clamp(c, 0, m.w - 1), std::clamp(r, 0, m.h - 1))]];
}

// Applies tile changes made by the simulation: re-bake the chunks they touch, rebuild nearby props.
void WorldView::sync(Sim& sim, Map& m, MapView& v) {
    if (m.dirty.empty()) {
        if (m.falling.empty()) v.fallingImg.clear();
        return;
    }
    for (const auto& d : m.dirty) {
        for (const auto& f : m.falling)
            if (f.c == d.c && f.r == d.r && !v.fallingImg.count(m.idx(d.c, d.r)))
                for (const auto& p : v.props[d.r])
                    if (p.c == d.c) v.fallingImg[m.idx(d.c, d.r)] = p.img;
        refreshProps(m, v, d.c, d.r);
        for (int dr = -2; dr <= 2; dr++)
            for (int dc = -1; dc <= 1; dc++) {
                const int c = d.c + dc, r = d.r + dr;
                if (m.inside(c, r)) v.baked[static_cast<size_t>(r / CHUNK) * v.cw + c / CHUNK] = 0;
            }
    }
    m.dirty.clear();
    m.exploredDirty = true;
    v.fullDirty = true;
    (void)sim;
}

void WorldView::refreshProps(Map& m, MapView& v, int c, int r) {
    static const int D[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& d : D) {
        const int nc = c + d[0], nr = r + d[1];
        if (!m.inside(nc, nr)) continue;
        std::erase_if(v.props[nr], [&](const Prop& p) { return p.c == nc; });
        Prop p;
        if (propFor(m, v, nc, nr, p)) v.props[nr].push_back(p);
    }
}

// Trees and boulders overlap the tiles above them, so they are drawn with the actors, sorted by their base.
bool WorldView::propFor(Map& m, MapView& v, int c, int r, Prop& out) {
    const uint8_t t = m.get(c, r);
    if (t == GRASS && m.outdoor) return decorFor(m, c, r, out);
    if (t != TREE && !(t == ROCK && m.outdoor)) return false;
    auto allSame = [&](uint8_t want) { return m.get(c + 1, r) == want && m.get(c - 1, r) == want && m.get(c, r + 1) == want && m.get(c, r - 1) == want; };
    const double hv = hash2(c, r, 21);
    // Inside a thick wood or rock field only every other tile gets a sprite (staggered);
    // the canopies and boulders from the row below still cover the skipped tiles.
    const bool stagger = (c + r) & 1;
    const int jx = static_cast<int>(hash2(c, r, 24) * 5) - 2, jy = static_cast<int>(hash2(c, r, 25) * 3);
    const ZoneDef* z = m.outdoor ? &zoneAt(db_, m, c, r) : nullptr;
    out.c = c;
    out.base = (r + 1) * TILE - 2;
    // Old giants from the Undead pack: two to four times the hero's height, spaced so their crowns do not pile up.
    // Dead and broken ones in the marsh and the north, dark-leaved ones elsewhere.
    if (m.outdoor && z && ((c * 7 + r * 13) % 5 == 0 || (t == ROCK && (c * 5 + r * 11) % 6 == 0))) {
        const double roll = hash2(c, r, 27);
        const bool dead = z->id == "marsh" || z->id == "frost" || z->tree == "dead";
        static const char* LEAFY[] = {"tree_1", "tree_1", "tree_2", "tree_2", "tree_3", "dead_tree_2"};
        static const char* DEAD[] = {"dead_tree_1", "dead_tree_2", "dead_tree_3", "broken_tree_1", "broken_tree_4", "dead_tree_1"};
        static const char* ROCKS[] = {"rock_1", "rock_2", "rock_3", "rock_2"};
        const char* name = nullptr;
        if (t == TREE && roll < (z->id == "deepwood" ? 0.95 : 0.75)) name = (dead ? DEAD : LEAFY)[static_cast<int>(hash2(c, r, 28) * 6)];
        else if (t == ROCK && !z->snow && roll < 0.6) name = ROCKS[static_cast<int>(hash2(c, r, 28) * 4)];
        if (name) {
            const Sprite& big = spr_.undead(name);
            if (big) {
                out.img = &big;
                out.tree = t == TREE;
                out.sway = out.tree ? std::clamp(big.h / 24, 1, 3) : 0;
                out.x = c * TILE + 8 - big.w / 2 + static_cast<int>(hash2(c, r, 29) * 5) - 2;
                out.y = (r + 1) * TILE + 3 - big.h;
                return true;
            }
        }
    }
    if (t == TREE) {
        const bool deep = allSame(TREE);
        if (deep && stagger) return false;
        const TreeSet& T = *v.zoneTrees[z ? z->index : 0];
        const std::string kind = z ? z->tree : "oak";
        const bool pines = kind == "pine" || (kind == "mixed" && fbm(c / 16.0, r / 16.0, 77) > 0.5) || (kind == "oak" && fbm(c / 16.0, r / 16.0, 77) > 0.62);
        const Sprite* pool[4];
        int n = 0;
        if (kind == "dead") {
            pool[n++] = &T.oakM;
            pool[n++] = &T.small;
            pool[n++] = &T.pineS;
            if (deep) pool[n++] = &T.oak;
        } else if (pines) {
            pool[n++] = &T.pine;
            if (!deep) {
                pool[n++] = &T.pine;
                pool[n++] = &T.pineS;
            }
        } else if (deep) {
            pool[n++] = &T.oak;
            pool[n++] = &T.oak;
            pool[n++] = &T.oakM;
        } else {
            pool[n++] = &T.oak;
            pool[n++] = &T.oakM;
            pool[n++] = &T.oakM;
            pool[n++] = &T.small;
        }
        out.img = pool[static_cast<int>(hv * n)];
        out.tree = true;
        out.sway = std::clamp(out.img->h / 22, 1, 3);
        out.x = c * TILE + 8 - out.img->w / 2 + (deep ? jx : 0);
        out.y = (r + 1) * TILE + 1 + (deep ? jy : 0) - out.img->h;
        return true;
    }
    const bool deep = allSame(ROCK);
    if (deep && stagger && m.get(c, r + 2) == ROCK) return false;
    // Coherent veins: snow in the mountains, patches of sandstone among the grey rock
    const char* zone = z && z->snow ? "snow" : fbm(c / 7.0, r / 7.0, 91) > 0.55 ? "sand" : "grey";
    const bool big = deep && hv < 0.18 && m.get(c, r - 1) == ROCK;
    const Frames& pool = big ? spr_.bigBoulders[zone] : spr_.boulders[zone];
    if (pool.empty()) return false;
    out.img = &pool[static_cast<size_t>(hash2(c, r, 23) * pool.size())];
    out.tree = false;
    out.sway = 0;
    out.x = c * TILE + 8 - out.img->w / 2 + (deep ? jx : 0);
    out.y = (r + 1) * TILE + (big ? 4 : 2) - out.img->h;
    return true;
}

// The ways to other maps, each its own thing: a black carriage, a gate of bones, a moored boat with its ferryman,
// a ritual circle ringed with candles, an old mine lift on its chains.
void WorldView::drawTravel(Sim& sim, const Object& o) {
    const double x = o.c * TILE, y = o.r * TILE;
    const int tick = sim.tickCount;
    const bool open = o.res.empty() || sim.bossesDefeated.count(o.res);
    const Color ink(14, 11, 10), wood(70, 52, 38), woodHi(104, 80, 56), iron(80, 80, 88), ironHi(130, 130, 140);
    if (o.sprite == "carriage") {
        g_.drawTinted(spr_.glow, x - 18, y + 8, 52, 14, Color(0, 0, 0, 120));
        g_.fillRect(x - 12, y - 14, 34, 22, ink);                    // the coach
        g_.fillRect(x - 11, y - 13, 32, 20, Color(32, 28, 30));
        g_.fillRect(x - 11, y - 13, 32, 2, Color(60, 52, 54));
        g_.fillRect(x - 6, y - 9, 8, 7, Color(open ? 120 : 40, open ? 96 : 34, 50));   // a lamplit window
        g_.fillRect(x + 6, y - 9, 8, 7, Color(open ? 120 : 40, open ? 96 : 34, 50));
        g_.fillRect(x - 14, y - 16, 38, 3, Color(48, 40, 40));       // the roof
        for (int w = 0; w < 2; w++) {                                // wheels
            const double wx = x - 9 + w * 24, wy = y + 6;
            g_.fillRect(wx - 4, wy - 4, 9, 9, ink);
            g_.fillRect(wx - 3, wy - 3, 7, 7, wood);
            g_.fillRect(wx, wy - 3, 1, 7, ink);
            g_.fillRect(wx - 3, wy, 7, 1, ink);
        }
        g_.fillRect(x + 23, y - 10, 2, 6, ironHi);                   // the lantern
        g_.drawTinted(spr_.glow, x + 16, y - 16, 16, 16, Color(255, 180, 90, static_cast<uint8_t>(110 + 40 * std::sin(tick * 0.1))), true);
        g_.fillRect(x - 22, y + 2, 10, 3, wood);                     // the shafts
    } else if (o.sprite == "portal") {
        const Sprite& pt = spr_.monster("portal", "");
        if (open) g_.drawTinted(spr_.glow, x - 14, y - 22, 44, 44, Color(200, 120, 255, static_cast<uint8_t>(90 + 60 * std::sin(tick * 0.08))), true);
        if (pt) g_.draw(pt, x - 16, y - 26, 48, 48);
        else g_.fillRect(x - 8, y - 20, 32, 34, Color(60, 20, 80));
    } else if (o.sprite == "ferry") {
        g_.fillRect(x - 14, y + 4, 44, 8, Color(20, 30, 36, 160));      // dark water by the jetty
        const double by = y + std::round(std::sin(tick * 0.05 + o.c) * 1.2);   // the boat rides the swell, the post stays
        g_.fillRect(x - 12, by - 2, 40, 2, ink);                       // the boat
        g_.fillRect(x - 10, by, 36, 5, wood);
        g_.fillRect(x - 8, by + 5, 32, 2, Color(46, 34, 26));
        g_.fillRect(x - 10, by, 36, 1, woodHi);
        g_.fillRect(x + 16, by - 16, 2, 18, wood);                     // the pole
        g_.fillRect(x + 4, by - 14, 6, 12, Color(30, 28, 32));        // the ferryman, hooded
        g_.fillRect(x + 5, by - 17, 4, 4, Color(24, 22, 26));
        g_.fillRect(x - 16, y - 6, 3, 14, wood);                  // a mooring post
        g_.drawTinted(spr_.glow, x - 22, y - 16, 14, 14, Color(255, 200, 120, 90), true);
    } else if (o.sprite == "ritual") {
        const double cx = x + 8, cy = y + 4;
        for (int i = 0; i < 48; i++) {                                // the circle
            const double a = i / 48.0 * PI * 2;
            g_.fillRect(std::round(cx + std::cos(a) * 18), std::round(cy + std::sin(a) * 9), 1, 1, Color(open ? 200 : 110, open ? 60 : 40, 50));
        }
        for (int i = 0; i < 6; i++) {                                 // candles
            const double a = i / 6.0 * PI * 2 + 0.3;
            const double kx = std::round(cx + std::cos(a) * 18), ky = std::round(cy + std::sin(a) * 9);
            g_.fillRect(kx, ky - 4, 2, 4, Color(210, 200, 180));
            if (open) g_.drawTinted(spr_.glow, kx - 4, ky - 10, 10, 10, Color(255, 170, 80, static_cast<uint8_t>(120 + 60 * std::sin(tick * 0.2 + i))), true);
        }
        g_.fillRect(cx - 3, cy - 2, 6, 3, Color(90, 30, 30));          // the effigy at its heart
        g_.fillRect(cx - 1, cy - 10, 2, 9, Color(60, 46, 36));
        g_.fillRect(cx - 4, cy - 8, 8, 2, Color(60, 46, 36));
    } else {   // lift
        g_.fillRect(x - 10, y - 30, 3, 40, wood);                     // the frame
        g_.fillRect(x + 23, y - 30, 3, 40, wood);
        g_.fillRect(x - 12, y - 32, 40, 4, woodHi);
        for (int i = 0; i < 9; i++) g_.fillRect(x + 1 + (i % 2), y - 28 + i * 3, 1, 2, ironHi);   // the chains
        for (int i = 0; i < 9; i++) g_.fillRect(x + 15 + (i % 2), y - 28 + i * 3, 1, 2, ironHi);
        const double cx = x + std::round(std::sin(tick * 0.04 + o.c) * 1.2);   // the cage swings on its chains
        g_.fillRect(cx - 6, y - 2, 28, 10, ink);                      // the cage
        g_.fillRect(cx - 5, y - 1, 26, 8, iron);
        for (int i = 0; i < 6; i++) g_.fillRect(cx - 3 + i * 5, y - 1, 1, 8, ironHi);
        g_.drawTinted(spr_.glow, x + 20, y - 26, 14, 14, Color(255, 190, 110, 90), true);
    }
    if (!open) {   // a padlock of shadow until the way is opened
        g_.fillRect(x + 4, y - 30, 8, 7, Color(20, 16, 14, 200));
        g_.fillRect(x + 6, y - 33, 4, 3, Color(20, 16, 14, 200));
    }
}

// Small things standing on open ground, region by region: grave markers in old burial grounds, bones, thorns,
// weeds, stumps, the arms of the drowned in the marsh, ice crystals in the north. Walked through, never blocking.
bool WorldView::decorFor(Map& m, int c, int r, Prop& out) {
    if (m.objectAt(c, r) || m.block[m.idx(c, r)] || m.heightAt(c, r) != m.heightAt(c, r + 1)) return false;
    for (const auto& t : db_.towns)
        if (t.realm == m.id && std::abs(t.at.c - c) < t.radius + 3 && std::abs(t.at.r - r) < t.radius + 3) return false;
    const std::string& z = zoneAt(db_, m, c, r).id;
    const double roll = hash2(c, r, 140);
    // burial grounds: patches where the dead were laid, thick with markers
    const bool graves = (z == "heartlands" || z == "marsh" || z == "vale" || z == "coast") && fbm(c / 9.0, r / 9.0, 141) > 0.7;
    const char* name = nullptr;
    if (m.heightAt(c, r) > 0 && roll > 0.93) {   // high ground keeps the ruins of old watchposts
        static const char* R[] = {"ruin_3", "ruin_4", "ruin_5", "ruin_5", "pile_sculls", "ruin_4"};
        name = R[static_cast<int>(hash2(c, r, 145) * 6)];
    } else if (graves && roll < 0.22) {
        static const char* G[] = {"grave_1", "grave_2", "grave_3", "grave_4", "grave_10", "grave_11", "grave_12", "bones_1"};
        name = G[static_cast<int>(hash2(c, r, 142) * 8)];
    } else if (roll < 0.035) {
        static const char* VALE[] = {"plant_3", "plant_4", "plant_5", "broken_tree_2", "broken_tree_3", "bones_1"};
        static const char* WOOD[] = {"thorn_plant_3", "thorn_plant_6", "plant_2", "plant_3", "broken_tree_2", "bones_3"};
        static const char* COAST[] = {"bones_3", "bones_5", "rock_4", "rock_5", "plant_5", "bones_4"};
        static const char* MARSH[] = {"dead_arm_3", "dead_arm_4", "thorn_plant_5", "bones_6", "bones_7", "plant_4"};
        static const char* FROST[] = {"crystal_3", "rock_4", "rock_5", "bones_15", "bones_16", "broken_tree_3"};
        const char* const* set = z == "deepwood" ? WOOD : z == "coast" ? COAST : z == "marsh" ? MARSH : z == "frost" ? FROST : VALE;
        name = set[static_cast<int>(hash2(c, r, 143) * 6)];
    } else if (roll > 0.9993) name = "pile_sculls";
    if (!name) return false;
    const Sprite& img = spr_.undead(name);
    if (!img) return false;
    out.img = &img;
    out.tree = false;
    out.sway = swaysInWind(name) ? std::clamp(img.h / 14, 1, 2) : 0;
    out.c = c;
    out.base = r * TILE + 8;
    out.x = c * TILE + 8 - img.w / 2 + static_cast<int>(hash2(c, r, 144) * 7) - 3;
    out.y = r * TILE + 13 - img.h;
    return true;
}

// Inside a building: wooden boards, tiles or carpet underfoot, a two-tile wall face along the back,
// dark wall tops elsewhere, and a doormat at the way out.
void WorldView::drawInteriorTile(Map& m, const TileSet& T, int c, int r) {
    const uint8_t t = m.get(c, r), below = m.get(c, r + 1), below2 = m.get(c, r + 2);
    const double x = c * TILE, y = r * TILE, hv = hash2(c, r, 5);
    auto floor = [&] {
        const std::string& f = m.floorStyle;
        if (f == "tile") g_.draw(spr_.get("in_floor_tile"), x, y);
        else if (f == "dark") g_.drawPart(spr_.get("in_floor_dark"), {(c & 1) * 16, (r & 1) * 16, 16, 16}, x, y, 16, 16);
        else if (f == "carpet") g_.drawPart(spr_.get("in_carpet"), {(c & 1) * 16, (r & 1) * 16, 16, 16}, x, y, 16, 16);
        else if (f == "stone") g_.draw(T.slab[static_cast<int>(hv * 9)], x, y);
        else {   // boards in rows of four, joints staggered
            g_.fillRect(x, y, 16, 16, Color(146, 100, 62));
            for (int k = 0; k < 4; k++) {
                const double py = y + k * 4;
                g_.fillRect(x, py + 3, 16, 1, Color(108, 70, 42));
                const int joint = static_cast<int>(hash2(c, r * 4 + k, 77) * 16);
                g_.fillRect(x + joint, py, 1, 3, Color(116, 78, 48));
                if (hash2(c, r * 4 + k, 78) < 0.35) g_.fillRect(x + (joint + 7) % 14, py + 1, 2, 1, Color(166, 118, 76));
            }
        }
    };
    if (t == WALL) {
        const bool lower = below != WALL && r + 1 < m.h, upper = below == WALL && below2 != WALL && r + 2 < m.h;
        if ((lower || upper) && c > 0 && c < m.w - 1) {
            const std::string& w = m.wallStyle;
            if (w == "wood") g_.drawPart(spr_.get("in_wall_wood"), {(c % 3) * 16, upper ? 0 : 16, 16, 16}, x, y, 16, 16);
            else if (w == "stone") g_.draw(upper ? T.brickHi[static_cast<int>(hv * 4)] : T.brickLo[static_cast<int>(hv * 4)], x, y);
            else {
                g_.draw(spr_.get(w == "yellow" ? "in_wall_yellow" : "in_wall_brick"), x, y);
                if (lower) g_.fillRect(x, y + 13, 16, 3, Color(92, 62, 40));
            }
            if (upper) g_.fillRect(x, y, 16, 1, Color(30, 20, 16));
            return;
        }
        g_.fillRect(x, y, 16, 16, Color(48, 34, 28));
        const Color trim(96, 68, 48);
        if (r + 1 < m.h && below != WALL) g_.fillRect(x, y + 13, 16, 3, trim);
        if (c + 1 < m.w && m.get(c + 1, r) != WALL && r >= 3) g_.fillRect(x + 13, y, 3, 16, trim);
        if (c > 0 && m.get(c - 1, r) != WALL && r >= 3) g_.fillRect(x, y, 3, 16, trim);
        if (r > 0 && m.get(c, r - 1) != WALL) g_.fillRect(x, y, 16, 3, trim);
        return;
    }
    floor();
    if (t == STAIRS_DOWN) {   // the way out
        g_.fillRect(x + 2, y + 2, 12, 12, Color(98, 36, 30));
        g_.fillRect(x + 3, y + 3, 10, 10, Color(150, 58, 44));
        g_.fillRect(x + 3, y + 7, 10, 1, Color(190, 120, 70));
    }
}

void WorldView::bakeChunk(Map& m, MapView& v, int cx, int cy) {
    const size_t i = static_cast<size_t>(cy) * v.cw + cx;
    // Keep texture memory bounded on the big overworld: drop chunks far from this one
    int live = 0;
    for (const auto& c : v.chunks) live += c ? 1 : 0;
    if (live >= 20)
        for (int y = 0; y < v.ch; y++)
            for (int x = 0; x < v.cw; x++)
                if (std::max(std::abs(x - cx), std::abs(y - cy)) > 2) {
                    const size_t j = static_cast<size_t>(y) * v.cw + x;
                    if (v.chunks[j]) g_.destroy(v.chunks[j]);
                    v.baked[j] = 0;
                }
    if (!v.chunks[i]) v.chunks[i] = g_.makeTarget(CHUNK * TILE, CHUNK * TILE);
    g_.setTarget(&v.chunks[i]);
    g_.setTransform(1, cx * CHUNK * TILE, cy * CHUNK * TILE);
    g_.setAlpha(1);
    g_.clear(Color(0, 0, 0, 0));
    for (int r = cy * CHUNK; r < std::min(m.h, (cy + 1) * CHUNK); r++)
        for (int c = cx * CHUNK; c < std::min(m.w, (cx + 1) * CHUNK); c++) drawTile(m, v, c, r);
    g_.setTarget(nullptr);
    v.baked[i] = 1;
}

void WorldView::drawTile(Map& m, MapView& v, int c, int r) {
    const uint8_t t = m.get(c, r);
    const double x = c * TILE, y = r * TILE, hv = hash2(c, r, 5);
    const TileSet& T = tilesAt(m, v, c, r);
    const uint8_t below = m.get(c, r + 1), below2 = m.get(c, r + 2);
    auto solidWall = [](uint8_t w) { return w == WALL || w == DOOR; };
    if (m.interior) {
        drawInteriorTile(m, T, c, r);
        return;
    }
    switch (t) {
        case CLIFF:
            if (cliffFace(m, c, r)) {
                drawCliff(m, T, c, r);
                break;
            }
            [[fallthrough]];
        case GRASS:
        case FLOWERS:
        case TREE: {
            g_.draw(T.dirt[static_cast<int>(hv * 4)], x, y);
            // Grass over dirt: each 8x8 quarter picks an edge, corner or inner-corner piece from the
            // grass patch (x160 y224) and the grass-with-hole block (x240 y240) of the tileset.
            for (int qy = 0; qy < 2; qy++)
                for (int qx = 0; qx < 2; qx++) {
                    const int sx = qx ? 1 : -1, sy = qy ? 1 : -1;
                    const bool V = grassyAt(m, c, r + sy), H = grassyAt(m, c + sx, r), D = grassyAt(m, c + sx, r + sy);
                    int tx, ty;
                    if (V && H && !D) {
                        tx = 240 + (qx ? 0 : 16);
                        ty = 240 + (qy ? 0 : 16);
                    } else {
                        tx = 160 + 16 * (H ? (qx ? 2 : 1) : (qx ? 3 : 0));
                        ty = 224 + 16 * (V ? (qy ? 2 : 1) : (qy ? 3 : 0));
                    }
                    g_.drawPart(T.sheet, {tx + qx * 8, ty + qy * 8, 8, 8}, x + qx * 8, y + qy * 8, 8, 8);
                }
            if (t == FLOWERS) {
                // the flowers themselves are drawn every frame in render(), so they can nod in the wind
            } else if (t == CLIFF) {
                drawCliff(m, T, c, r);   // the lip: rocks along the drop
            } else if (t == GRASS && hash2(c, r, 6) < 0.035) {
                const Sprite& f = spr_.decals[static_cast<size_t>(hash2(c, r, 7) * spr_.decals.size())];
                g_.draw(f, x + (16 - f.w) / 2, y + (16 - f.h) / 2);
            }
            break;
        }
        case PATH:
        case SAND:
        case ROCK: g_.draw(T.dirt[static_cast<int>(hv * 4)], x, y); break;
        case COBBLE:
            g_.draw(T.dirt[static_cast<int>(hv * 4)], x, y);
            g_.drawPart(spr_.cobble, {(c & 1) * 16, (r & 1) * 16, 16, 16}, x, y, 16, 16);
            break;
        case PLAZA: g_.drawPart(spr_.plaza, {16, 16, 16, 16}, x, y, 16, 16); break;
        case STAIRS_DOWN: g_.draw(m.dark ? T.stairsDown : T.dirt[static_cast<int>(hv * 4)], x, y); break;
        case PLANKS:
        case FLOOR: g_.draw(T.slab[static_cast<int>(hv * 9)], x, y); break;
        case STAIRS_UP: g_.draw(T.stairsUp, x, y); break;
        case DOOR: g_.draw(T.slab[4], x, y); break;
        case WALL:
            if (!solidWall(below) && r + 1 < m.h) g_.draw(T.brickLo[static_cast<int>(hv * 4)], x, y);
            else if (below == WALL && !solidWall(below2) && r + 2 < m.h) g_.draw(T.brickHi[static_cast<int>(hv * 4)], x, y);
            else g_.draw(T.wallTop[static_cast<int>(hv * 4)], x, y);
            break;
        case WATER:
        case BRIDGE: {
            if (!spr_.water.empty()) g_.draw(spr_.water[static_cast<size_t>(hash2(c, r, 40) * spr_.water.size())], x, y);
            else g_.fillRect(x, y, 16, 16, Color(60, 127, 166));
            auto land = [&](int dc, int dr) {
                const uint8_t n = m.get(c + dc, r + dr);
                return n != WATER && n != BRIDGE;
            };
            // dark bank shadow under land, light foam along it
            const Color foam(200, 236, 250), bank(22, 70, 120);
            if (land(0, -1)) {
                g_.fillRect(x, y, 16, 3, bank);
                for (int i = 0; i < 16; i++)
                    if (hash2(c * 16 + i, r, 3) > 0.3) g_.fillRect(x + i, y + 3, 1, 1, foam);
            }
            for (int i = 0; i < 16; i++) {
                if (hash2(c * 16 + i, r, 4) < 0.3) continue;
                if (land(0, 1)) g_.fillRect(x + i, y + 15, 1, 1, foam);
                if (land(-1, 0)) g_.fillRect(x, y + i, 1, 1, foam);
                if (land(1, 0)) g_.fillRect(x + 15, y + i, 1, 1, foam);
            }
            const bool nearWhirl = m.hasWhirl && hypot2(c - m.whirl.c, r - m.whirl.r) < 3;
            const bool open = !land(0, -1) && !land(0, 1) && !land(-1, 0) && !land(1, 0);
            if (t == WATER && open && !nearWhirl) {
                const double roll = hash2(c, r, 41);
                if (roll < 0.012 && !spr_.waterRocks.empty()) {
                    const Sprite& im = spr_.waterRocks[static_cast<size_t>(hash2(c, r, 42) * spr_.waterRocks.size())];
                    g_.draw(im, x + 8 - im.w / 2, y + 8 - im.h / 2);
                } else if (roll > 0.985 && m.outdoor && zoneAt(db_, m, c, r).id == "marsh") g_.draw(spr_.get("lily"), x, y);
            }
            if (t == BRIDGE) {
                const bool vertical = m.get(c - 1, r) == WATER && m.get(c + 1, r) == WATER;
                if (vertical) {
                    g_.drawPart(T.deck, {3, 0, 10, 16}, x + 3, y, 10, 16);
                    g_.draw(T.bridgeV, x, y);
                } else {
                    g_.drawPart(T.deckH, {0, 3, 16, 10}, x, y + 3, 16, 10);
                    g_.draw(T.bridgeH, x, y);
                }
            }
            break;
        }
        case RAMP: drawCliff(m, T, c, r); break;
        default: break;
    }
    if (m.outdoor && !m.height.empty()) drawElevation(m, c, r);
}

// High ground. A face of layered rock, two tiles tall, hangs below each south edge; the other edges are a lip
// of grass with rocks along the drop; ramps are stone steps cut into the face.
void WorldView::drawCliff(Map& m, const TileSet& T, int c, int r) {
    const double x = c * TILE, y = r * TILE;
    const int here = m.heightAt(c, r);
    const Color base(84, 76, 68), dark(50, 44, 40), lite(126, 116, 102), line(28, 24, 22);
    const bool upper = m.heightAt(c, r - 1) > here;   // the top tile of a face (the bottom one has face above it)
    if (m.get(c, r) == RAMP) {   // stone steps between two cheeks of rock
        g_.fillRect(x, y, 16, 16, base);
        g_.fillRect(x + 3, y, 10, 16, Color(98, 92, 84));
        for (int i = 0; i < 4; i++) {
            g_.fillRect(x + 3, y + i * 4, 10, 1, Color(150, 142, 128));
            g_.fillRect(x + 3, y + i * 4 + 3, 10, 1, Color(56, 50, 46));
        }
        g_.fillRect(x + 2, y, 1, 16, line);
        g_.fillRect(x + 13, y, 1, 16, line);
        g_.fillRect(x, y, 2, 16, dark);
        g_.fillRect(x + 14, y, 2, 16, dark);
        return;
    }
    if (cliffFace(m, c, r)) {
        // strata: bands of rock with cracks; turf hanging over the top; dark and damp at the foot
        g_.fillRect(x, y, 16, 16, base);
        for (int band = 0; band < 3; band++) {
            const double by = y + 2 + band * 5 + (upper ? 1 : 0);
            g_.fillRect(x, by, 16, 1, lite);
            g_.fillRect(x, by + 4, 16, 1, dark);
            for (int i = 0; i < 16; i++)
                if (hash2(c * 16 + i, r * 4 + band, 13) < 0.14) g_.fillRect(x + i, by + 1, 1, 3, line);
        }
        if (upper) {
            for (int i = 0; i < 16; i++) {   // the turf of the high ground spilling over the edge
                const int len = 1 + static_cast<int>(hash2(c * 16 + i, r, 14) * 4);
                g_.fillRect(x + i, y, 1, len, i % 3 ? Color(76, 94, 50) : Color(96, 116, 60));
            }
            g_.fillRect(x, y, 16, 1, Color(118, 138, 74));
        } else {
            for (int i = 0; i < 6; i++) g_.fillRect(x, y + 10 + i, 16, 1, Color(0, 0, 0, static_cast<uint8_t>(20 + i * 18)));
            g_.fillRect(x, y + 15, 16, 1, line);
            for (int i = 0; i < 3; i++) {   // stones fallen at the foot
                const double sx = x + hash2(c, r, 50 + i) * 13;
                if (hash2(c, r, 60 + i) < 0.5) g_.fillRect(sx, y + 13, 3, 2, lite);
            }
        }
        // where the face ends, its corner is shaded
        const int up = upper ? 1 : 2;
        if (m.heightAt(c - 1, r - up) <= here) g_.fillRect(x, y, 3, 16, Color(0, 0, 0, 60));
        if (m.heightAt(c + 1, r - up) <= here) g_.fillRect(x + 13, y, 3, 16, Color(0, 0, 0, 100));
        return;
    }
    // a lip (drawn over grass): rocks along each side that drops away
    auto lower = [&](int dc, int dr) { return m.heightAt(c + dc, r + dr) < here; };
    auto rocks = [&](double rx, double ry, double rw, double rh, int seed) {
        for (int i = 0; i < 4; i++) {
            const double bs = 3 + hash2(c, r, seed + i) * 3;
            const double bx = rx + hash2(c, r, seed + 10 + i) * std::max(0.0, rw - bs), by = ry + hash2(c, r, seed + 20 + i) * std::max(0.0, rh - bs);
            g_.fillRect(bx, by, bs, bs, base);
            g_.fillRect(bx, by, bs, 1, lite);
            g_.fillRect(bx, by + bs - 1, bs, 1, dark);
        }
    };
    if (lower(-1, 0)) {
        g_.fillRect(x, y, 2, 16, dark);
        rocks(x, y, 6, 16, 100);
    }
    if (lower(1, 0)) {
        g_.fillRect(x + 14, y, 2, 16, dark);
        rocks(x + 10, y, 6, 16, 140);
    }
    if (lower(0, -1)) {
        g_.fillRect(x, y, 16, 2, lite);
        g_.fillRect(x, y + 2, 16, 1, dark);
        rocks(x, y, 16, 6, 180);
    }
    if (!lower(-1, 0) && !lower(1, 0) && !lower(0, -1)) rocks(x, y, 16, 16, 220);   // a corner
}

// The height of the land: high ground catches more light, and every cliff casts a shadow on the ground below
// and to its right (the light comes from the upper left).
void WorldView::drawElevation(Map& m, int c, int r) {
    const double x = c * TILE, y = r * TILE;
    const int here = m.heightAt(c, r);
    if (cliffFace(m, c, r)) return;
    const uint8_t t = m.get(c, r);
    const bool ground = t == GRASS || t == FLOWERS || t == SAND || t == PATH || t == TREE;
    if (ground) {
        // Rolling ground: a gentle height field, lit from the upper left, shades every 4x4 patch. Small rises
        // and hollows read as relief without any cliff.
        auto H = [](double px, double py) { return fbm(px / 150.0, py / 150.0, 913) * 1.0 + fbm(px / 45.0, py / 45.0, 917) * 0.25; };
        for (int by = 0; by < 4; by++)
            for (int bx = 0; bx < 4; bx++) {
                const double px = x + bx * 4 + 2, py = y + by * 4 + 2;
                const double gx = H(px + 3, py) - H(px - 3, py), gy = H(px, py + 3) - H(px, py - 3);
                const double lit = std::clamp((gx + gy) * -55.0, -1.0, 1.0);   // facing the light: brighter
                if (std::abs(lit) < 0.08) continue;
                g_.fillRect(x + bx * 4, y + by * 4, 4, 4, lit > 0 ? Color(255, 240, 210, static_cast<uint8_t>(lit * 26)) : Color(10, 8, 14, static_cast<uint8_t>(-lit * 46)));
            }
        drawDetail(m, c, r);
    }
    if (here > 0) g_.fillRect(x, y, 16, 16, Color(255, 236, 196, static_cast<uint8_t>(14 * here)));
    if (cliffFace(m, c, r - 1))
        for (int i = 0; i < 7; i++) g_.fillRect(x, y + i, 16, 1, Color(0, 0, 0, static_cast<uint8_t>(120 - i * 17)));
    if (m.heightAt(c - 1, r) > here || cliffFace(m, c - 1, r))
        for (int i = 0; i < 5; i++) g_.fillRect(x + i, y, 1, 16, Color(0, 0, 0, static_cast<uint8_t>(80 - i * 16)));
}

// Small things on the ground that keep the land from repeating: stones, twigs, leaf litter, mud, bones,
// puddles and reeds in the marsh, cracked ice in the north. Which ones, and how many, depends on the region.
void WorldView::drawDetail(Map& m, int c, int r) {
    const double x = c * TILE, y = r * TILE;
    const uint8_t t = m.get(c, r);
    if (t == TREE || (t == GRASS && m.objectAt(c, r))) return;
    const std::string& z = zoneAt(db_, m, c, r).id;
    const double roll = hash2(c, r, 71), rx = hash2(c, r, 72) * 11, ry = hash2(c, r, 73) * 11;
    const Color stone(112, 106, 98), stoneDk(62, 58, 54), twig(78, 58, 40), leaf(118, 84, 44), leaf2(92, 70, 40), mud(52, 44, 36);
    auto pebbles = [&](int n) {
        for (int i = 0; i < n; i++) {
            const double px = x + 1 + hash2(c, r, 80 + i) * 13, py = y + 1 + hash2(c, r, 90 + i) * 13;
            g_.fillRect(px, py, 2, 1, stone);
            g_.fillRect(px, py + 1, 2, 1, stoneDk);
        }
    };
    if (t == PATH || t == SAND) {   // ruts and stones on roads
        if (roll < 0.35) pebbles(1 + static_cast<int>(hash2(c, r, 74) * 3));
        if (t == PATH && roll > 0.85) g_.fillRect(x + rx, y + ry, 4, 2, Color(60, 48, 38, 120));
        return;
    }
    const bool forest = z == "deepwood", marsh = z == "marsh", frost = z == "frost";
    if (roll < (forest ? 0.30 : 0.10)) {   // leaf litter
        for (int i = 0; i < 4; i++)
            g_.fillRect(x + hash2(c, r, 100 + i) * 14, y + hash2(c, r, 110 + i) * 14, 2, 1, i % 2 ? leaf : leaf2);
    } else if (roll < (forest ? 0.38 : 0.16)) {   // a fallen twig
        const int len = 4 + static_cast<int>(hash2(c, r, 75) * 4);
        for (int i = 0; i < len; i++) g_.fillRect(x + rx + i, y + ry + (i * 2) / len, 1, 1, twig);
        g_.fillRect(x + rx + len / 2, y + ry - 1, 1, 1, twig);
    } else if (roll < 0.26) {
        pebbles(1 + static_cast<int>(hash2(c, r, 76) * 3));
    } else if (roll < 0.34) {   // a tuft of dark grass
        for (int i = 0; i < 4; i++) g_.fillRect(x + rx + i, y + ry + (i % 2), 1, 3 - (i % 2), Color(46, 60, 34));
    } else if (marsh && roll < 0.48) {   // puddles and reeds
        g_.fillRect(x + rx - 1, y + ry + 1, 7, 3, Color(28, 36, 40));
        g_.fillRect(x + rx, y + ry, 5, 1, Color(28, 36, 40));
        g_.fillRect(x + rx + 1, y + ry + 1, 2, 1, Color(110, 130, 140, 140));
        if (hash2(c, r, 77) < 0.5)
            for (int i = 0; i < 3; i++) g_.fillRect(x + rx + 6 + i * 2, y + ry - 4 + i, 1, 5 - i, Color(70, 84, 48));
    } else if ((marsh || z == "heartlands" || z == "coast") && roll < 0.42) {   // mud
        g_.fillRect(x + rx - 1, y + ry, 7, 3, mud);
        g_.fillRect(x + rx, y + ry - 1, 5, 1, mud);
    } else if (frost && roll < 0.45) {   // cracks in the frozen ground
        for (int i = 0; i < 6; i++) g_.fillRect(x + rx + i, y + ry + static_cast<int>(hash2(c, r, 120 + i) * 3), 1, 1, Color(210, 222, 236, 160));
    } else if (roll > 0.992) {   // bones: someone did not make it
        g_.fillRect(x + rx, y + ry, 5, 1, Color(206, 200, 184));
        g_.fillRect(x + rx, y + ry - 1, 1, 3, Color(206, 200, 184));
        g_.fillRect(x + rx + 4, y + ry - 1, 1, 3, Color(206, 200, 184));
        g_.fillRect(x + rx + 7, y + ry - 2, 3, 3, Color(196, 190, 176));
        g_.fillRect(x + rx + 8, y + ry - 1, 1, 1, Color(40, 34, 30));
    }
}

const Sprite& WorldView::minimap(Sim& sim, Map& m, bool fog) {
    MapView& v = view(sim, m);
    if (!v.mini) {
        v.mini = g_.makeStreaming(m.w, m.h, false);
        v.miniFull = g_.makeStreaming(m.w, m.h, false);
        v.miniPx.assign(static_cast<size_t>(m.w) * m.h * 4, 0);
        m.exploredDirty = v.fullDirty = true;
    }
    auto fill = [&](bool withFog) {
        for (int r = 0; r < m.h; r++)
            for (int c = 0; c < m.w; c++) {
                uint8_t* p = &v.miniPx[static_cast<size_t>(m.idx(c, r)) * 4];
                if (withFog && !m.explored.empty() && !m.explored[m.idx(c, r)]) {
                    p[3] = 0;
                    continue;
                }
                const uint8_t t = m.get(c, r);
                Color col = miniColor(t);
                if (m.outdoor && (t == GRASS || t == FLOWERS || t == TREE)) {
                    const ZoneDef& zd = db_.zones[m.zone[m.idx(c, r)]];
                    const Color z = zd.ground;
                    if (zd.snow && t != TREE) col = Color(250, 250, 250);
                    col = Color(static_cast<uint8_t>(col.r * z.r / 255), static_cast<uint8_t>(col.g * z.g / 255), static_cast<uint8_t>(std::min(255, col.b * z.b / 255 + (z.b > 230 && z.r < 240 ? 60 : 0))));
                }
                // the same washed-out grade as the art
                const Image::Grade& gr = Image::grade;
                const double lum = 0.3 * col.r + 0.59 * col.g + 0.11 * col.b;
                const uint8_t tint[3] = {gr.tint.r, gr.tint.g, gr.tint.b}, raw[3] = {col.r, col.g, col.b};
                for (int k = 0; k < 3; k++) p[k] = static_cast<uint8_t>(std::clamp((lum + (raw[k] - lum) * gr.saturation) * gr.brightness * tint[k] / 255.0, 0.0, 255.0));
                p[3] = 255;
            }
    };
    if (fog && m.exploredDirty) {
        m.exploredDirty = false;
        fill(true);
        g_.update(v.mini, v.miniPx.data());
    }
    if (!fog && v.fullDirty) {
        v.fullDirty = false;
        fill(false);
        g_.update(v.miniFull, v.miniPx.data());
    }
    return fog ? v.mini : v.miniFull;
}

void WorldView::drawSway(const Sprite& img, double x, double y, double lean) {
    if (!img || img.h < 4 || std::abs(lean) < 0.5) {
        g_.draw(img, x, y);
        return;
    }
    // The base stays rooted and the lean grows toward the top; rows that share an offset go out as one strip.
    auto shift = [&](int row) { return static_cast<int>(std::lround(lean * std::pow(1.0 - static_cast<double>(row) / img.h, 1.6))); };
    int start = 0, cur = shift(0);
    for (int row = 1; row <= img.h; row++) {
        const int s = row < img.h ? shift(row) : cur + 1000;
        if (s == cur) continue;
        g_.drawPart(img, {0, start, img.w, row - start}, x + cur, y + start, img.w, row - start);
        start = row;
        cur = s;
    }
}

// The body language of a still picture. Facing: + is the way it looks (face), so leaning forward is +face.
WorldView::Warp WorldView::gait(const Enemy& e, int tick) const {
    const EnemyDef& def = *e.def;
    const double f = e.face < 0 ? -1 : 1;
    Warp w;
    if (def.look == Look::Mimic) {   // a chest that breathes, and now and then shivers with hunger
        w.squash = 0.03 * std::sin(e.t * 0.06);
        if (!e.moving && e.hop <= 0 && (e.t + e.uid * 37) % 200 < 12) w.lean = (e.t >> 1) % 2 ? 1 : -1;
        if (e.hop > 0 || e.wind > 0) w.squash = 0.08 * std::sin(e.t * 0.7);
        return w;
    }
    if (def.fly) {   // wings beating: it stretches and gathers in quick strokes, swaying as it hovers
        const double beat = std::sin(e.t * (e.moving ? 0.45 : 0.3));
        w.squash = 0.09 * beat;
        w.lean = 1.2 * std::sin(e.t * 0.08) + (e.moving ? f : 0);
    } else if (e.hop > 0 && def.ai == Ai::Charger) {   // the charge: head down, low and long
        w.lean = f * 3;
        w.squash = -0.08 + 0.04 * std::sin(e.t * 0.7);
        w.foot = -f * 1.5 * std::sin(e.t * 0.7);
    } else if (e.moving) {   // a step: the body rocks over the feet, which swing the other way; flat on landing, tall mid-stride
        const double phase = e.t * (e.fast ? 0.35 : 0.22), step = std::sin(phase);
        w.lean = f * (e.fast ? 1.5 : 0.8) + step * 1.3;
        w.foot = -step * 1.4;
        w.squash = -0.07 * std::cos(2 * phase);
    } else {   // breathing, with a slow shift of weight
        w.squash = 0.035 * std::sin(e.t * 0.05 + e.uid);
        w.lean = 0.7 * std::sin(e.t * 0.021 + e.uid * 1.7);
    }
    // getting ready to strike: rears back and gathers itself, trembling
    if (e.wind > 0) {
        w.lean = -f * 2.5 + ((e.t >> 1) % 2 ? 0.6 : -0.6);
        w.squash = -0.1;
        w.foot = f * 0.8;
    }
    if (e.act > 0) {   // a blow (guards) or a shot (archers): rear back, then lunge into it
        const int total = def.ai == Ai::Archer ? (e.act > 40 ? 10 : 40) : 24;
        const int left = def.ai == Ai::Archer && e.act > 40 ? e.act - 40 : e.act;
        const double p = 1 - static_cast<double>(left) / total;
        if (def.ai == Ai::Archer && e.act <= 40) {   // reloading: a slow bend
            w.lean = -f * 1.5 * std::sin(PI * p);
            w.squash = -0.05 * std::sin(PI * p);
        } else if (p < 0.45) {
            w.lean = -f * 3 * (p / 0.45);
            w.squash = 0.06 * (p / 0.45);
        } else {
            const double q = std::min(1.0, (p - 0.45) / 0.25);
            w.lean = f * (4 * q - 3 * (1 - q));
            w.squash = -0.1 * q;
            w.foot = -f * q;
        }
    }
    if (def.isBoss()) {
        if (e.state == Enemy::State::SlamWind) {   // rises up tall, then comes down
            const double p = 1 - e.st / 52.0;
            w.squash = p < 0.8 ? 0.14 * p / 0.8 : 0.14 - 0.4 * (p - 0.8) / 0.2;
            w.lean = -f * 2 * std::min(1.0, p * 1.5) + ((tick >> 1) % 2 ? 0.5 : -0.5);
        } else if (e.state == Enemy::State::Roar || e.state == Enemy::State::Summon || e.state == Enemy::State::VolleyWind) {
            w.squash = 0.08 + 0.04 * std::sin(e.t * 0.9);
            w.lean = ((tick >> 1) % 2 ? 1 : -1);
        }
    }
    if (e.flash > 0) {   // struck: knocked back and squashed for a moment
        const double h = e.flash / 10.0;
        w.lean = -f * 2.5 * h;
        w.squash = std::min(w.squash, -0.12 * h);
    }
    return w;
}

void WorldView::drawWarp(const Sprite& img, double x, double y, double w, double h, bool flip, const Warp& wp, Color tint, bool additive) {
    if (!img) return;
    // whole art pixels: the stretched box keeps its foot on the ground and its middle where it was
    const double h2 = std::max(2.0, std::round(h * (1 + wp.squash))), w2 = std::max(2.0, std::round(w * (1 - 0.6 * wp.squash)));
    const double x2 = x + std::round((w - w2) / 2), y2 = y + h - h2, q = h2 / img.h;
    auto shift = [&](int row) {
        const double t = static_cast<double>(row) / img.h;   // 0 at the top, 1 at the foot
        return static_cast<int>(std::lround(wp.lean * std::pow(1 - t, 1.4) + wp.foot * std::max(0.0, (t - 0.65) / 0.35)));
    };
    int start = 0, cur = shift(0);
    for (int row = 1; row <= img.h; row++) {
        const int sft = row < img.h ? shift(row) : cur + 1000;
        if (sft == cur) continue;
        const double ya = y2 + start * q, yb = y2 + row * q;
        g_.drawPart(img, {0, start, img.w, row - start}, x2 + cur, ya, w2, yb - ya, flip, tint, additive);
        start = row;
        cur = sft;
    }
}

void WorldView::pixelArc(double cx, double cy, double R, double a, double b, Color col) {
    const double step = 0.8 / R;
    for (double t = a; t <= b; t += step) g_.fillRect(std::round(cx + std::cos(t) * R), std::round(cy + std::sin(t) * R), 1, 1, col);
}

void WorldView::render(Sim& sim, int zoom, bool withActors) {
    Map& m = sim.map();
    MapView& v = view(sim, m);
    sync(sim, m, v);
    const int DW = g_.width(), DH = g_.height();
    const double VW = static_cast<double>(DW) / zoom, VH = static_cast<double>(DH) / zoom;
    const int sx = sim.shake > 0 ? static_cast<int>((frand() - 0.5) * sim.shake * zoom * 0.6) : 0;
    const int sy = sim.shake > 0 ? static_cast<int>((frand() - 0.5) * sim.shake * zoom * 0.6) : 0;
    offX = std::round(sim.cam.x * zoom) - sx;
    offY = std::round(sim.cam.y * zoom) - sy;
    const double camX = offX / zoom, camY = offY / zoom;
    const int tick = sim.tickCount;
    const Player& P = sim.P;

    const int c0 = std::max(0, ifloor(camX / TILE) - 1), r0 = std::max(0, ifloor(camY / TILE) - 1);
    const int c1 = std::min(m.w, static_cast<int>(std::ceil((camX + VW) / TILE)) + 1), r1 = std::min(m.h, static_cast<int>(std::ceil((camY + VH) / TILE)) + 1);
    for (int cy = r0 / CHUNK; cy <= (r1 - 1) / CHUNK && cy < v.ch; cy++)
        for (int cx = c0 / CHUNK; cx <= (c1 - 1) / CHUNK && cx < v.cw; cx++)
            if (!v.baked[static_cast<size_t>(cy) * v.cw + cx]) bakeChunk(m, v, cx, cy);

    g_.setTarget(nullptr);
    g_.setTransform(1);
    g_.setAlpha(1);
    g_.clear(m.dark ? Color(7, 6, 11) : m.interior ? Color(20, 14, 12) : Color(33, 72, 42));
    g_.setTransform(zoom, offX, offY);
    for (int cy = r0 / CHUNK; cy <= (r1 - 1) / CHUNK && cy < v.ch; cy++)
        for (int cx = c0 / CHUNK; cx <= (c1 - 1) / CHUNK && cx < v.cw; cx++) g_.draw(v.chunks[static_cast<size_t>(cy) * v.cw + cx], cx * CHUNK * TILE, cy * CHUNK * TILE);

    // Water: whirlpool, ripples and splashes under everything that stands on the ground, then a moving shimmer
    if (m.hasWhirl && !spr_.wWhirl.empty()) {
        const Sprite& f = anim(spr_.wWhirl, tick, 6);
        g_.draw(f, m.whirl.c * TILE + 8 - f.w / 2, m.whirl.r * TILE + 8 - f.h / 2);
    }
    for (const auto& f : m.wfx) {
        const Frames& fr = f.splash ? spr_.wSplash : spr_.wRipple;
        const Sprite& img = fr[std::min(fr.size() - 1, static_cast<size_t>(f.t / 4))];
        g_.draw(img, std::round(f.x - img.w / 2.0), std::round(f.y - img.h + (f.splash ? 2 : 6)));
    }
    // Flowers nodding in the wind (kept out of the baked chunks so they can move)
    if (m.outdoor && !m.interior)
        for (int r = r0; r < r1; r++)
            for (int c = c0; c < c1; c++) {
                if (m.get(c, r) != FLOWERS) continue;
                for (int i = 0; i < 2; i++) {
                    const Sprite& f = spr_.flowers[static_cast<int>(hash2(c, r, 30 + i) * 4)];
                    const double fx = c * TILE + 1 + static_cast<int>(hash2(c, r, 32 + i) * (14 - f.w)) + i * 3;
                    const double fy = r * TILE + 2 + static_cast<int>(hash2(c, r, 34 + i) * (12 - f.h));
                    drawSway(f, fx, fy, wind(tick, fx + i * 5, fy, true) * 1.4);
                }
            }
    if (m.outdoor) {
        const Color shimmer(190, 228, 255, 128);
        for (int r = r0; r < r1; r++)
            for (int c = c0; c < c1; c++) {
                if (m.get(c, r) != WATER) continue;
                const double ph = tick * 0.03 + hash2(c, r, 9) * 6.28;
                const int ox = 2 + static_cast<int>((std::sin(ph) * 0.5 + 0.5) * 9), oy = 3 + ((c * 7 + r * 3) % 9);
                g_.fillRect(c * TILE + ox, r * TILE + oy, 3, 1, shimmer);
                if (hash2(c, r, 10) > 0.5) g_.fillRect(c * TILE + ((ox + 6) % 13), r * TILE + ((oy + 6) % 13), 2, 1, shimmer);
            }
    }

    // ── everything that stands up, sorted by the y of its base ──
    struct Entry {
        double key;
        int kind;
        const void* ptr;
    };
    std::vector<Entry> list;
    auto vis = [&](double x, double y, double pad) { return x > camX - pad && x < camX + VW + pad && y > camY - pad && y < camY + VH + pad + 16; };
    for (const auto& o : m.objects) {
        if (o.gone || !vis(o.c * TILE, o.r * TILE, 96)) continue;
        double key = o.r * TILE + 13;
        if (o.kind == ObjKind::WallTorch || o.kind == ObjKind::Trap || o.kind == ObjKind::Waypoint) key = -1;
        else if (o.kind == ObjKind::Gate) key = (o.r + 3) * TILE;
        else if (o.kind == ObjKind::Entrance) key = o.r * TILE + 15;
        else if (o.kind == ObjKind::Prop) {
            const PropDef* pd = db_.prop(o.id);
            if (pd && pd->flat) key = -1;
        }
        list.push_back({key, 0, &o});
    }
    for (int r = r0; r < std::min(m.h, r1 + 7); r++)
        for (const auto& p : v.props[r])
            if (p.x + p.img->w >= camX - 2 && p.x <= camX + VW + 2) list.push_back({p.base, 1, &p});
    for (const auto& f : m.falling) list.push_back({(f.r + 1) * TILE - 2.0, 2, &f});
    for (const auto& d : m.drops)
        if (vis(d.x, d.y, 16)) list.push_back({d.y, 3, &d});
    if (withActors) {
        for (const auto& c : m.corpses)
            if (vis(c.e.x, c.e.y, 60)) list.push_back({c.e.y - 2, 4, &c});
        for (const auto& e : m.enemies)
            if (e.alive && vis(e.x, e.y, 64)) list.push_back({e.y, 5, &e});
        list.push_back({sim.flying() ? 1e9 : P.y, 6, nullptr});   // a flying hero passes over trees and roofs
    }
    std::stable_sort(list.begin(), list.end(), [](const Entry& a, const Entry& b) { return a.key < b.key; });

    for (const auto& it : list) {
        switch (it.kind) {
            case 0: drawObject(sim, m, *static_cast<const Object*>(it.ptr)); break;
            case 1: {
                const Prop& p = *static_cast<const Prop*>(it.ptr);
                int sh = 0;
                if (p.tree) {
                    const int row = static_cast<int>((p.base + 2) / TILE) - 1;
                    auto hit = m.treeHit.find(m.idx(p.c, row));
                    if (hit != m.treeHit.end() && tick - hit->second.at < 12) sh = (tick >> 1) % 2 ? 1 : -1;   // shake while being chopped
                }
                // a big crown between the camera and the hero turns see-through, so the hero is never lost behind it
                const bool hides = p.img->h > 40 && P.y < p.base && P.y > p.y + 6 && P.x > p.x + 4 && P.x < p.x + p.img->w - 4;
                if (p.img->h > 30) {   // a long shadow thrown away from the light (upper left): squashed onto the ground
                    const double sw = p.img->w, shh = std::round(p.img->h * 0.38);
                    g_.drawTinted(*p.img, std::round(p.x + sw * 0.22), std::round(p.base - shh * 0.35), sw, shh, Color(4, 4, 8, 78));
                }
                if (hides) g_.setAlpha(0.45);
                drawSway(*p.img, p.x + sh, p.y, p.sway * wind(tick, p.x, p.base, m.outdoor));
                g_.setAlpha(1);
                break;
            }
            case 2: {   // a felled tree tips over around its trunk and fades out
                const FallingTree& f = *static_cast<const FallingTree*>(it.ptr);
                auto im = v.fallingImg.find(m.idx(f.c, f.r));
                if (im == v.fallingImg.end() || !im->second) break;
                const Sprite& img = *im->second;
                const double k = std::min(1.0, f.t / 26.0), a = k * k * (PI / 2) * f.dir;
                g_.setAlpha(f.t > 26 ? std::max(0.0, 1 - (f.t - 26) / 14.0) : 1);
                g_.drawRotated(img, f.c * TILE + 8 - img.w / 2, (f.r + 1) * TILE - img.h, a, img.w / 2.0, img.h);
                g_.setAlpha(1);
                break;
            }
            case 3: {
                const Drop& d = *static_cast<const Drop*>(it.ptr);
                const Sprite& img = d.globe ? spr_.get("globe") : spr_.icon(d.item.id);
                g_.fillRect(std::round(d.x) - 3, std::round(d.y) + 1, 6, 1, Color(0, 0, 0, 72));
                if (!d.globe && d.item.rarity > 0 && d.item.rarity < static_cast<int>(db_.rarities.size())) {   // finer loot shines
                    const Color glow = db_.rarities[d.item.rarity].glow;
                    const double pulse = 0.55 + 0.25 * std::sin(tick * 0.15 + d.x);
                    g_.drawTinted(spr_.glow, d.x - 14, d.y - 16, 28, 24, glow.alpha(pulse), true);
                    g_.fillRect(std::round(d.x), std::round(d.y) - 30, 1, 26, glow.alpha(0.5 * pulse));
                }
                const int bob = d.z != 0 ? 0 : static_cast<int>(std::lround(std::sin(d.t * 0.08)));
                g_.draw(img, std::round(d.x - img.w / 2.0), std::round(d.y - img.h - d.z) + bob);
                break;
            }
            case 4: drawCorpse(*static_cast<const Corpse*>(it.ptr)); break;
            case 5: drawEnemy(sim, *static_cast<const Enemy*>(it.ptr)); break;
            default: drawPlayer(sim); break;
        }
    }

    // ── telegraphs, projectiles, particles ──
    for (const auto& e : m.enemies) {
        if (!e.alive) continue;
        if (e.def->isBoss() && e.state == Enemy::State::SlamWind) {
            const double prog = 1 - e.st / 52.0, R = e.def->boss.slamRadius;
            const double rr = R * std::min(1.0, prog * 1.4);
            g_.drawTinted(spr_.glow, e.slamX - rr, e.slamY - rr, rr * 2, rr * 2, Color(224, 80, 60, static_cast<uint8_t>(90 + prog * 110)));
            pixelArc(e.slamX, e.slamY, R, 0, PI * 2, tick % 8 < 4 ? Color(255, 122, 106) : Color(224, 80, 60));
        }
        if (e.aim > 0) {   // dotted red line from a crossbowman to where he is aiming
            const double x0 = e.x, y0 = e.y - 8, vx = e.ax - x0, vy = e.ay - y0, len = std::max(1.0, hypot2(vx, vy));
            const Color col = e.aim <= 12 ? (tick % 6 < 3 ? Color(255, 90, 74) : Color(255, 176, 160)) : Color(224, 80, 60, 140);
            for (double t = 8; t < std::min(len + 24, 190.0); t += 4) g_.fillRect(std::round(x0 + vx / len * t), std::round(y0 + vy / len * t), 1, 1, col);
        }
    }
    if (withActors && sim.mode == Mode::Play && showTargetMark && sim.hasItem("bow") && sim.hasItem("arrows")) {
        if (const Enemy* e = sim.bowTarget()) {   // four pixel corners around what the keyboard crossbow will hit
            const double R = e->r + 3 + ((tick >> 4) % 2), cx = std::round(e->x), cy = std::round(e->y - e->cy - 2);
            const Color gold(242, 210, 122);
            for (int k = 0; k < 4; k++) {
                const int qx = k % 2 ? 1 : -1, qy = k / 2 ? 1 : -1;
                const double x = cx + qx * R, y = cy + qy * R;
                g_.fillRect(qx < 0 ? x : x - 2, y, 3, 1, gold);
                g_.fillRect(x, qy < 0 ? y : y - 2, 1, 3, gold);
            }
        }
    }
    for (const auto& p : m.proj) {
        const Sprite& img = p.kind == Projectile::Kind::Knife ? anim(spr_.knife, tick + static_cast<int>(p.x), 4)
                                                              : spr_.bolt8[((static_cast<int>(std::lround(std::atan2(p.vy, p.vx) / (PI / 4))) % 8) + 8) % 8];
        g_.draw(img, std::round(p.x - img.w / 2.0), std::round(p.y - img.h / 2.0));
    }
    for (const auto& p : sim.particles) g_.fillRect(std::round(p.x), std::round(p.y), p.s, p.s, p.col);

    // Mist drifting over the land (and thicker in the marshes at night)
    if (m.outdoor) {
        const double camX = offX / zoom, camY = offY / zoom, vw = g_.width() / static_cast<double>(zoom), vh = g_.height() / static_cast<double>(zoom);
        const double drift = tick * 0.12;
        const int CELL = 150;
        const int c0 = static_cast<int>(std::floor((camX - drift - 200) / CELL)), c1 = static_cast<int>(std::floor((camX - drift + vw + 200) / CELL));
        const int r0 = static_cast<int>(std::floor((camY - 120) / CELL)), r1 = static_cast<int>(std::floor((camY + vh + 120) / CELL));
        const double thick = 0.6 + 0.6 * sim.darkness();
        for (int r = r0; r <= r1; r++)
            for (int c = c0; c <= c1; c++) {
                const uint32_t hsh = static_cast<uint32_t>(c * 73856093) ^ static_cast<uint32_t>(r * 19349663);
                if (hsh % 3 == 0) continue;
                const double fx = c * CELL + drift + (hsh % 97), fy = r * CELL + ((hsh >> 8) % 83) + std::sin(tick * 0.004 + c) * 10;
                const double w = 220 + (hsh >> 4) % 120, h = 90 + (hsh >> 12) % 60;
                const uint8_t a = static_cast<uint8_t>(std::min(255.0, (30 + (hsh >> 16) % 26) * thick));
                g_.drawTinted(spr_.glow, std::round(fx - w / 2), std::round(fy - h / 2), w, h, Color(190, 200, 205, a));
            }
    }

    // ── light and time of day ──
    if (m.dark) drawLighting(sim, m, zoom, 0.94, Color(6, 5, 12));
    else {
        const double dk = m.interior ? 0 : sim.darkness(), h = sim.hourOfDay();   // lamps are lit indoors
        const double warm = m.interior ? 0 : std::max(0.0, 1 - std::abs(h - 18.6) / 1.4) + std::max(0.0, 1 - std::abs(h - 5.8) / 1.0);   // dusk and dawn
        if (warm > 0.01) {
            g_.setTransform(1);
            g_.fillRect(0, 0, DW, DH, Color(255, 130, 60, static_cast<uint8_t>(255 * 0.12 * std::min(1.0, warm))));
        }
        // A grim world: even at noon a gloom hangs over it, and torches, fires and windows cut pools of light out of it
        const double gloom = m.interior ? 0.34 : 0.26;
        if (dk > 0.01 || gloom > 0) drawLighting(sim, m, zoom, std::max(gloom, 0.8 * dk), dk > 0.3 ? Color(8, 10, 26) : Color(12, 16, 20));
    }
    // Ambient life, above the shadows: ash drifting down by day, fireflies after dusk, wisps over the marsh
    if (m.outdoor) {
        g_.setTransform(zoom, offX, offY);
        const double camX = offX / zoom, camY = offY / zoom, vw = DW / static_cast<double>(zoom), vh = DH / static_cast<double>(zoom);
        const double dk = sim.darkness();
        const int CELL = 40;
        for (int r = static_cast<int>(std::floor(camY / CELL)) - 1; r <= static_cast<int>((camY + vh) / CELL) + 1; r++)
            for (int c = static_cast<int>(std::floor(camX / CELL)) - 1; c <= static_cast<int>((camX + vw) / CELL) + 1; c++) {
                const uint32_t hsh = static_cast<uint32_t>(c * 2654435761u) ^ static_cast<uint32_t>(r * 40503u + 7);
                const int tc = c * CELL / TILE, tr = r * CELL / TILE;
                if (!m.inside(tc, tr)) continue;
                const bool marsh = zoneAt(db_, m, tc, tr).id == "marsh";
                const double ph = (hsh % 1000) / 159.0, t = tick / 60.0;
                if (dk > 0.25 && hsh % 4 == 0) {   // fireflies (green-gold) or marsh wisps (pale blue)
                    const double fx = c * CELL + 20 + std::sin(t * 0.7 + ph) * 16 + std::sin(t * 1.9 + ph * 2) * 4;
                    const double fy = r * CELL + 20 + std::cos(t * 0.5 + ph * 1.3) * 12;
                    const double blink = std::max(0.0, std::sin(t * 2.2 + ph * 3));
                    const Color col = marsh ? Color(150, 210, 255) : Color(214, 255, 120);
                    const uint8_t a = static_cast<uint8_t>(std::min(1.0, (dk - 0.25) * 2) * blink * 200);
                    g_.drawTinted(spr_.glow, std::round(fx) - 4, std::round(fy) - 4, 9, 9, col.alpha(a / 255.0 * 0.8), true);
                    g_.fillRect(std::round(fx), std::round(fy), 1, 1, col.alpha(a / 255.0));
                } else if (dk < 0.5 && hsh % 3 == 0) {   // ash and seed down, falling slow and slanted
                    const double fall = std::fmod(t * (6 + hsh % 7) + ph * 30, CELL * 3.0);
                    const double fx = c * CELL + (hsh >> 8) % CELL + std::sin(t * 0.9 + ph) * 5 + fall * 0.4;
                    const double fy = r * CELL - CELL + fall;
                    const double life = std::sin(PI * fall / (CELL * 3.0));
                    g_.fillRect(std::round(fx), std::round(fy), 1, 1, Color(214, 210, 200, static_cast<uint8_t>(110 * life * (1 - dk))));
                }
            }
    }
    g_.setTransform(1);
}

void WorldView::drawObject(Sim& sim, Map& m, const Object& o) {
    const double x = o.c * TILE, y = o.r * TILE;
    const int tick = sim.tickCount;
    auto drawAt = [&](const Sprite& img, double bx, double by) { g_.draw(img, std::round(bx - img.w / 2.0), std::round(by - img.h)); };
    switch (o.kind) {
        case ObjKind::Chest: {
            if (o.mimic && o.opened) break;
            const int f = !o.opened ? 0 : o.openAt <= 0 ? 3 : std::min(3, (tick - o.openAt) / 5);
            g_.draw(spr_.chest[std::min(spr_.chest.size() - 1, static_cast<size_t>(std::max(0, f)))], x - 8, y - 16);
            if (!o.opened && (tick + o.c * 13) % 90 < 8) {
                g_.fillRect(x + 11, y + 2, 1, 3, Color(255, 246, 200));
                g_.fillRect(x + 10, y + 3, 3, 1, Color(255, 246, 200));
            }
            break;
        }
        case ObjKind::Node: {
            if (o.left <= 0) break;   // fully mined or picked: gone until it regrows
            const Frames& stages = spr_.nodes[o.res];
            if (stages.empty()) break;
            const Sprite& img = stages[std::min(stages.size() - 1, static_cast<size_t>(o.max - o.left))];
            const int sh = tick - o.hitAt < 10 ? ((tick >> 1) % 2 ? 1 : -1) : 0;
            const int grow = tick - o.grewAt < 12 ? 1 : 0;
            if (o.res == "herb")   // herbs nod in the wind; ore and stone do not
                drawSway(img, std::round(x + 8 + sh - img.w / 2.0), std::round(y + 15 + grow - img.h), 1.4 * wind(tick, x, y, m.outdoor));
            else drawAt(img, x + 8 + sh, y + 18 + grow);
            break;
        }
        case ObjKind::Stump: drawAt(o.stage ? spr_.trees("").small : spr_.get("log"), x + 8, y + 16 + (o.stage ? 1 : 0)); break;
        case ObjKind::Pot: drawAt(spr_.get(o.sprite), x + 8, y + 15); break;
        case ObjKind::Prop: {
            const Frames& fr = spr_.frames(o.sprite);
            const Sprite& img = fr.size() > 1 ? anim(fr, tick + o.c * 7, 9) : spr_.get(o.sprite);
            const PropDef* pd = db_.prop(o.id);
            if (pd && pd->flat) {
                // a lily pad rides the ripples; rugs and bones lie still
                const double bob = o.sprite == "lily" ? std::round(std::sin(tick * 0.05 + o.c * 1.3 + o.r)) : 0;
                g_.draw(img, x + 8 - img.w / 2, y + 8 - img.h / 2 + bob);
            } else if (pd && pd->sway && fr.size() <= 1) {
                const double bx = x + 8 + pd->ox, by = y + 16 + (o.h > 1 && !m.interior ? 2 : 0) + pd->oy;
                drawSway(img, std::round(bx - img.w / 2.0), std::round(by - img.h), pd->sway * wind(tick, x, y, m.outdoor));
            } else drawAt(img, x + 8 + (pd ? pd->ox : 0), y + 16 + (o.h > 1 && !m.interior ? 2 : 0) + (pd ? pd->oy : 0));
            if (!o.text.empty() && m.outdoor) {   // a house's sign board: what the building is
                const Sprite& ic = spr_.get(o.text).w ? spr_.get(o.text) : spr_.icon(o.text);
                const double bx = x + 8 + 13, by = y - 13;
                g_.fillRect(bx - 1, by - 3, 1, 3, Color(60, 40, 26));
                g_.fillRect(bx + 13, by - 3, 1, 3, Color(60, 40, 26));
                g_.fillRect(bx - 2, by, 17, 14, Color(60, 40, 26));
                g_.fillRect(bx - 1, by + 1, 15, 12, Color(176, 128, 82));
                if (ic.w) g_.draw(ic, bx, by + 1, 13, 12);
            }
            break;
        }
        case ObjKind::Sign:
            if (o.id == "fallen") {
                const Frames& dead = spr_.character("ch1", "").get("dead");
                g_.draw(dead[std::min<size_t>(1, dead.size() - 1)], x - 8, y - 15);
            } else drawAt(spr_.get("sign"), x + 8, y + 15);
            break;
        case ObjKind::Npc: {
            const NpcDef* n = db_.npc(o.id);
            if (!n) break;
            int face = o.face;
            if (hypot2(sim.P.x - x - 8, sim.P.y - y - 8) < 60) face = sim.P.x < x + 8 ? -1 : 1;
            g_.fillRect(x + 3, y + 15, 10, 2, Color(0, 0, 0, 64));
            g_.draw(anim(spr_.character(n->sprite, n->tint).get("idle"), tick + o.c * 7, 12), x - 8, y - 15, face < 0);
            // Quest marks: yellow "!" = something new, "?" = hand in here (dimmed while still in progress)
            const int mark = sim.npcMarker(o.id);
            if (mark) {   // a small gold glyph with a dark rim and a faint glow, not a speech bubble
                static const char* BANG[7] = {".##.", ".##.", ".##.", ".##.", "....", ".##.", ".##."};
                static const char* ASK[7] = {".###", "#..#", "...#", "..#.", ".#..", "....", ".#.."};
                const char* const* g = mark == 1 ? BANG : ASK;
                const double bob = std::round(std::sin(tick * 0.08 + o.c) * 1.5), gx = x + 6, gy = y - 26 + bob;
                const Color gold = mark == 2 ? Color(150, 140, 120, 150) : Color(255, 204, 84);
                if (mark != 2) g_.drawTinted(spr_.glow, gx - 6, gy - 4, 16, 16, Color(255, 190, 80, 70), true);
                for (int rr = 0; rr < 7; rr++)
                    for (int cc = 0; cc < 4; cc++)
                        if (g[rr][cc] == '#') g_.fillRect(gx + cc - 1, gy + rr - 1, 3, 3, Color(20, 14, 8, mark == 2 ? 120 : 220));
                for (int rr = 0; rr < 7; rr++)
                    for (int cc = 0; cc < 4; cc++)
                        if (g[rr][cc] == '#') g_.fillRect(gx + cc, gy + rr, 1, 1, gold);
            }
            break;
        }
        case ObjKind::Campfire:
            if (!o.sprite.empty()) {   // an oven or hearth indoors, with a fire in its mouth
                const Sprite& ov = spr_.get(o.sprite);
                drawAt(ov, x + 8, y + 16);
                const Frames& fire = spr_.frames("fire");
                if (!fire.empty()) g_.draw(anim(fire, tick, 7), x, y - 3);
                if (tick % 11 == 0) sim.particles.push_back({x + 6 + frand() * 4, y - 2, (frand() - 0.5) * 0.2, -0.4, 0, 26, 1, Color(255, 179, 71)});
                break;
            }
            g_.draw(anim(spr_.campMeat, tick, 7), x - 8, y - 16);
            if (tick % 9 == 0) sim.particles.push_back({x + 6 + frand() * 4, y + 2, (frand() - 0.5) * 0.2, -0.4, 0, 30, 1, Color(255, 179, 71)});
            break;
        case ObjKind::WallTorch: g_.draw(anim(spr_.torch, tick + o.c * 5, 7), x, y + 1); break;
        case ObjKind::Trap:
            g_.draw(tilesAt(m, views_[m.id], o.c, o.r).slit, x, y);
            if (o.charge > 0) g_.fillRect(x + 7, y + 6, 2, 6, (tick >> 2) % 2 ? Color(255, 90, 74) : Color(255, 179, 71));
            break;
        case ObjKind::Gate: {
            if (o.id == "zone_gate") {   // an iron portcullis in the rock wall between regions
                if (o.text != "hidden") {
                    g_.drawTinted(spr_.glow, x - 20, y - 4, 56, 24, Color(0, 0, 0, 120));
                    g_.draw(spr_.gate[0], x + 8 - spr_.gate[0].w / 2, y + 16 - spr_.gate[0].h + 8);
                }
                break;
            }
            const int f = !sim.doorOpen ? 0 : std::min(5, std::max(0, (tick - sim.gateAt) / 6));
            g_.draw(spr_.gate[std::min(spr_.gate.size() - 1, static_cast<size_t>(f))], x, y - 16);
            break;
        }
        case ObjKind::Entrance: drawAt(spr_.get(o.sprite), x + 8, y + 18); break;
        case ObjKind::Travel: drawTravel(sim, o); break;
        case ObjKind::Shrine: {
            const bool ready = tick >= o.at;
            if (ready) g_.drawTinted(spr_.glow, x - 16, y - 30, 48, 56, Color(196, 160, 255, static_cast<uint8_t>(110 + 60 * std::sin(tick * 0.06))), true);
            const Sprite& st = spr_.get("statue");
            if (!ready) g_.setAlpha(0.6);
            drawAt(st, x + 8, y + 16);
            g_.setAlpha(1);
            break;
        }
        case ObjKind::Waypoint: {
            const Sprite& wp = spr_.get("waypoint");
            g_.draw(wp, x + 8 - wp.w / 2, y + 8 - wp.h / 2);
            if (sim.waypoints.count(o.id)) g_.drawTinted(spr_.glow, x - 12, y - 12, 40, 40, Color(120, 200, 255, static_cast<uint8_t>(90 + 50 * std::sin(tick * 0.07))), true);
            break;
        }
    }
}

std::vector<std::string> WorldView::heroLayers(const Sim& sim) const {
    auto layersOf = [&](EquipSlot s) {
        std::vector<std::string> out;
        const Item* it = sim.equipped(s);
        const std::string d = it ? db_.item(it->id).doll : std::string();
        for (size_t i = 0, j; i < d.size(); i = j + 1) {
            j = d.find('+', i);
            if (j == std::string::npos) j = d.size();
            if (j > i) out.push_back(d.substr(i, j - i));
        }
        return out;
    };
    const auto armor = layersOf(EquipSlot::Armor), helm = layersOf(EquipSlot::Helm), boots = layersOf(EquipSlot::Boots), shield = layersOf(EquipSlot::Shield);
    auto has = [](const std::vector<std::string>& v, const char* p) {
        for (const auto& l : v)
            if (l.rfind(p, 0) == 0) return true;
        return false;
    };
    // behind, body, legs, feet, chest, arms, shoulders, the cape over them, the head, the shield in front
    std::vector<std::string> L = {"cape_bg", "body", "head"};
    L.push_back(has(armor, "legs_") ? "legs_plate" : "pants");
    if (boots.empty()) L.push_back("boots");
    for (const auto& b : boots) L.push_back(b);
    bool chest = false;
    for (const auto& a : armor)
        if (a.rfind("torso_", 0) == 0) L.push_back(a), chest = true;
    if (!chest) L.push_back("shirt");
    for (const auto& a : armor)
        if (a == "arms_plate" || a == "pauldrons") L.push_back(a);
    L.push_back("cape_fg");
    if (helm.empty()) L.push_back("hood");
    for (const auto& h : helm) L.push_back(h);
    for (const auto& s : shield) L.push_back(s);
    return L;
}

void WorldView::drawPlayer(Sim& sim) {
    const Player& P = sim.P;
    const int tick = sim.tickCount, atkT = db_.tuning.attackTicks;
    const AnimalSet& K = spr_.knight(heroLayers(sim));
    const bool flip = P.face < 0;
    // a swing leans the hero into the blow
    const double swingP = P.atkT > 0 ? 1 - static_cast<double>(P.atkT) / atkT : 0, lunge = P.atkT > 0 ? 2.5 * std::sin(PI * swingP) : 0;
    const double x = std::round(P.x + std::cos(P.swingA) * lunge) - 20;
    double y = std::round(P.y + std::sin(P.swingA) * lunge * 0.6) - 39;
    // the knight faces the way he walks, or the way the blade swings
    const Dir facing = P.atkT > 0 ? snap4(std::cos(P.swingA), std::sin(P.swingA)) : P.dir;
    const int row = animalRow(facing);
    auto frames = [&](const char* name) -> const Frames& {
        static const Frames none;
        return K.anims.count(name) ? K.get(name)[row] : none;
    };
    if (P.leapT > 0) {   // Leap Slam: high in an arc, a shadow on the ground below
        const double k = 1 - P.leapT / 22.0;
        g_.drawTinted(spr_.glow, std::round(P.x) - 12, std::round(P.y) - 4, 24, 8, Color(0, 0, 0, 120));
        y -= std::round(std::sin(PI * k) * 18);
    }
    if (sim.mode == Mode::Dead) {
        const Frames& dead = frames("hurt");
        if (!dead.empty()) g_.draw(dead[std::min(dead.size() - 1, static_cast<size_t>((tick - sim.deadAt) / 10))], x, y);
        return;
    }
    if (P.iframes > 0 && P.roll <= 0 && (tick >> 2) % 2 == 0) return;
    const Frames& walk = frames("walk");
    if (walk.empty()) return;
    static Frames walking;   // the walk cycle without its standing frame
    walking.assign(walk.begin() + std::min<size_t>(1, walk.size() - 1), walk.end());
    const Sprite* img = &walk[0];
    if (P.atkT > 0 && !frames("slash").empty()) img = &progress(frames("slash"), atkT, P.atkT);
    else if (P.bowT > 0 && !frames("shoot").empty()) img = &progress(frames("shoot"), 10, P.bowT);
    else if (!P.mount.empty()) img = !P.moving && !frames("idle").empty() ? &anim(frames("idle"), tick, 30) : &walk[0];
    else if (P.moving || P.roll > 0) img = &anim(walking, P.animT, 5);
    else if (!frames("idle").empty()) img = &anim(frames("idle"), tick, 30);

    for (const auto& t : P.trail) {   // white ghosts behind a dodge roll
        g_.drawTinted(walk[0], std::round(t.x) - 20, std::round(t.y) - 39, 40, 40, Color(255, 255, 255, static_cast<uint8_t>(t.t / 10.0 * 0.5 * 255)), true);
    }
    if (!P.mount.empty()) {   // riding: the animal under the hero
        const ItemDef& md = db_.item(P.mount);
        const AnimalSet& A = spr_.animal(md.mountSprite, md.mountTint.empty() ? "#c8a888" : md.mountTint);
        if (md.mountFly) {   // high above the ground: a shadow below, a big bird with the hero on its back
            const double alt = 26 + std::round(std::sin(tick * 0.07) * 2);
            g_.drawTinted(spr_.glow, std::round(P.x) - 20, std::round(P.y) - 6, 40, 12, Color(0, 0, 0, 110));
            const auto& rows = A.get("walk");
            const Sprite& bird = anim(rows[animalRow(P.dir)], P.animT, P.moving ? 4 : 6);
            const double by = std::round(P.y - alt);
            g_.draw(bird, std::round(P.x) - 32, by - 40, 64, 64);
            y = by - 37;
        } else {
            const auto& rows = P.moving ? A.get("run") : A.get("idle");
            const Sprite& beast = anim(rows[animalRow(P.dir)], P.moving ? P.animT : tick, P.moving ? 5 : 10);
            g_.draw(beast, std::round(P.x) - 20, std::round(P.y) - 34, 40, 40);
            y -= 9 + (P.moving ? (P.animT / 5) % 2 : 0);
        }
    } else g_.drawTinted(spr_.glow, std::round(P.x) - 8, std::round(P.y) - 3, 16, 6, Color(0, 0, 0, 120));

    // The weapon: sheathed until it swings, then swept through an arc (any direction) with a smear of light behind it
    const Item* weapon = sim.equipped(EquipSlot::Weapon);
    const Sprite* blade = nullptr;
    if (weapon)
        if (auto rot = spr_.weaponRot.find(weapon->id); rot != spr_.weaponRot.end()) blade = &rot->second[0];   // pointing up
    double bladeA = 0, hx = x + 20, hy = y + 28;
    const bool swinging = P.atkT > 0 && sim.mode != Mode::Dead;
    if (!swinging) blade = nullptr;   // the knight's sword is sheathed between blows
    (void)flip;
    double f = 0;
    if (swinging) {
        f = atkT - P.atkT;
        bladeA = P.swingA + P.swingSide * Sim::swingOffset(f);
        hx = x + 20 + std::cos(bladeA) * 3;
        hy = y + 28 + std::sin(bladeA) * 3;
    }
    if (!P.mount.empty() && !swinging) blade = nullptr;   // riding: the sword stays sheathed
    auto weaponPass = [&] {
        if (blade && *blade) {
            // weapon art is drawn for the bag; in the hand it is a little smaller
            const double k = std::min(1.0, 14.0 / std::max(1, blade->h)), px = blade->w / 2.0, py = blade->h - 2.0;
            g_.drawRotated(*blade, hx - px * k, hy - py * k, bladeA + PI / 2, px, py, k);
        }
        if (swinging && f >= 2 && f <= 11) {   // the smear: arcs trailing behind the edge, fading
            const double R = blade ? std::max(9.0, std::min(14.0, blade->h - 2.0) + 2) : 8.0, cx = x + 20, cy = y + 28;
            const double fade = f <= 8 ? 1 : 1 - (f - 8) / 4.0;
            const double start = P.swingA + P.swingSide * (-Sim::SWING_SPAN), trail = 1.3;
            double a0 = bladeA - P.swingSide * trail;
            // never trail back past where the swing began
            if ((a0 - start) * P.swingSide < 0) a0 = start;
            const double lo = std::min(a0, bladeA), hi = std::max(a0, bladeA);
            for (int i = 0; i < 4; i++) {
                const double rr = R + 1 - i * 2.2, al = fade * (0.85 - i * 0.18);
                if (rr > 3 && al > 0) pixelArc(cx, cy, rr, lo, hi, Color(236, 232, 220, static_cast<uint8_t>(255 * al)));
            }
        }
    };
    const bool bladeBehind = std::sin(bladeA) < -0.25 || facing == Dir::Up;
    if (bladeBehind) weaponPass();
    g_.draw(*img, x, y);
    if (!bladeBehind) weaponPass();

    if (P.whirlT > 0) {   // whirlwind: an expanding ring of steel
        const double k = 1 - P.whirlT / 18.0;
        pixelArc(P.x, P.y - 6, 8 + k * (db_.tuning.whirlRadius - 8), 0, PI * 2, Color(255, 255, 255, static_cast<uint8_t>(255 * (1 - k))));
        pixelArc(P.x, P.y - 6, 6 + k * (db_.tuning.whirlRadius - 10), tick * 0.9, tick * 0.9 + 2.2, Color(255, 236, 170));
    }
    if (P.recallT > 0) g_.drawTinted(spr_.glow, P.x - 18, P.y - 30, 36, 40, Color(140, 200, 255, static_cast<uint8_t>(120 + 80 * std::sin(tick * 0.3))), true);
    if (P.mountCast > 0) g_.fillRect(x + 10, y + 12, 20.0 * (1 - static_cast<double>(P.mountCast) / db_.tuning.mountCastTicks), 2, Color(242, 210, 122));
    for (const auto& b : P.buffs)
        if (b.t > 0 && (b.t > 300 || (tick >> 3) % 2)) {
            const Color col = b.id == "frenzy" ? Color(255, 110, 80) : b.id == "protection" ? Color(200, 200, 215) : b.id == "swift" ? Color(140, 230, 255) : Color(255, 220, 110);
            g_.drawTinted(spr_.glow, P.x - 12, P.y - 4, 24, 10, col.alpha(0.5), true);
        }
}

const Sprite& WorldView::enemyFrame(Sim& sim, const Enemy& e, bool& flip) {
    const EnemyDef& def = *e.def;
    flip = false;
    if (def.look == Look::Animal) {
        const AnimalSet& A = spr_.animal(def.sprite, def.tint);
        const int row = animalRow(e.dir);
        if (A.anims.count("atk") && e.hop > 0 && e.fast && hypot2(sim.P.x - e.x, sim.P.y - e.y) < 30) return anim(A.get("atk")[row], e.t, 5);
        const auto& set = e.moving ? (e.fast && A.anims.count("run") ? A.get("run") : A.get("walk")) : A.get("idle");
        return anim(set[row], e.t, e.fast ? 5 : 8);
    }
    if (def.look == Look::Mimic) {
        flip = e.face < 0;
        static const int CHOMP[6] = {0, 1, 2, 3, 2, 1};
        return e.hop > 0 || e.wind > 0 ? spr_.chest[std::min<size_t>(spr_.chest.size() - 1, CHOMP[(e.t / 3) % 6])] : spr_.chest[std::min<size_t>(1, spr_.chest.size() - 1)];
    }
    if (def.look == Look::Static) {
        flip = e.face < 0;
        return spr_.monster(def.sprite, def.tint);
    }
    if (def.look == Look::Treant) {
        const Frames& row = spr_.treant(def.tint)[treantRow(e.dir)];
        return e.moving || e.hop > 0 ? anim(row, e.t, e.fast ? 4 : 8) : row[0];
    }
    const CharSet& S = spr_.character(def.sprite, def.tint);
    flip = e.face < 0;
    switch (def.ai) {
        case Ai::Guard: return e.act > 0 ? progress(S.get("atk"), 24, e.act) : e.moving ? anim(S.get("walk"), e.t, 8) : anim(S.get("idle"), e.t, 12);
        case Ai::Archer:
            return e.act > 40 ? progress(S.get("shot"), 10, e.act - 40) : e.act > 0 ? progress(S.get("reload"), 40, e.act) : e.moving ? anim(S.get("walk"), e.t, 8) : anim(S.get("idle"), e.t, 12);
        case Ai::Charger: return e.wind > 0 ? S.get("dash")[0] : e.hop > 0 ? anim(S.get("dash"), e.t, 5) : e.moving ? anim(S.get("walk"), e.t, 8) : anim(S.get("idle"), e.t, 12);
        case Ai::Boss:
            if (e.state == Enemy::State::SlamWind) return progress(S.get("atk"), 52, std::max(0, e.st));
            if (e.state == Enemy::State::VolleyWind || e.state == Enemy::State::Summon || e.state == Enemy::State::Roar) return anim(S.get("cast"), e.t, 8);
            return e.moving ? anim(S.get("walk"), e.t, 9) : anim(S.get("idle"), e.t, 12);
        default: return e.moving ? anim(S.get("walk"), e.t, 8) : anim(S.get("idle"), e.t, 12);
    }
}

void WorldView::drawEnemy(Sim& sim, const Enemy& e) {
    const EnemyDef& def = *e.def;
    const int tick = sim.tickCount;
    bool flip = false;
    const Sprite& img = enemyFrame(sim, e, flip);
    const bool animal = def.look == Look::Animal;
    const double k = def.scale;
    double yo = 0;
    if (def.fly) yo = -8 + std::round(std::sin(e.t * 0.1) * 2);
    if (e.elite) g_.drawTinted(spr_.glow, e.x - 14 * k, e.y - 10, 28 * k, 16, e.elite->glow.alpha(0.55 + 0.2 * std::sin(tick * 0.2)), true);
    else if (!animal) g_.fillRect(std::round(e.x - 5 * k), std::round(e.y), 10 * k, 2, Color(0, 0, 0, 64));
    const double top = animal ? e.y - 28 * k + yo : e.y - 30 * k;
    const int shake = e.wind > 0 && def.ai == Ai::Charger && def.look != Look::Human ? ((e.t >> 1) % 2 ? 1 : -1) : 0;
    const int crouch = e.wind > 0 && def.look == Look::Human ? 1 : 0;
    double dx = std::round(e.x - 16 * k) + shake, dy = std::round(top) + crouch;
    const bool flash = (e.flash > 0 && e.flash % 4 < 2) || (def.isBoss() && e.state == Enemy::State::VolleyWind && tick % 6 < 3);
    if (def.look == Look::Static || def.look == Look::Mimic) {   // still pictures are given a body that steps, breathes and strikes
        const Warp wp = gait(e, tick);
        double lift = 0;
        if (def.look == Look::Static) {
            const double phase = e.t * (e.fast ? 0.35 : 0.22);
            lift = e.moving && !def.fly ? std::round(std::abs(std::sin(phase)) * (e.fast ? 3 : 2)) : 0;   // each step leaves the ground
            if (e.hop > 0 && def.ai == Ai::Charger) lift = std::round(std::abs(std::sin(e.t * 0.5)) * 2);
            if (def.fly) lift = 4 + std::round(std::sin(e.t * 0.1) * 1.5);   // hovering
            const double sw = 18 * k * (1 - 0.15 * lift / 4) * (1 - wp.squash);   // the shadow shrinks as it leaves the ground
            g_.drawTinted(spr_.glow, std::round(e.x - sw / 2), std::round(e.y - 3), sw, 6, Color(0, 0, 0, static_cast<uint8_t>(110 - lift * 8)));
        }
        drawWarp(img, dx, dy - lift, 32 * k, 32 * k, flip, wp);
        if (flash) drawWarp(img, dx, dy - lift, 32 * k, 32 * k, flip, wp, Color(255, 255, 255), true);
    } else {
        g_.draw(img, dx, dy, 32 * k, 32 * k, flip);
        if (flash) g_.drawTinted(img, dx, dy, 32 * k, 32 * k, Color(255, 255, 255), true, flip);
    }
    // "!" over a monster that is about to attack
    if ((e.wind > 0 || e.aim > 0) && (tick >> 2) % 2 == 0) g_.draw(spr_.alert, std::round(e.x - spr_.alert.w / 2.0), std::round(top + (animal ? 2 : 6) * k - spr_.alert.h));
    if (!def.isBoss() && (e.hp < e.maxHp || e.elite)) {
        const double bw = e.elite ? 18 : 12, bx = std::round(e.x - bw / 2), by = std::round(e.y - (animal ? 24 * k : 22 * k) + yo);
        g_.fillRect(bx - 1, by - 1, bw + 2, 3, Color(22, 18, 31));
        g_.fillRect(bx, by, bw, 1, Color(74, 32, 48));
        g_.fillRect(bx, by, std::max(1.0, std::round(bw * e.hp / e.maxHp)), 1, e.elite ? e.elite->glow : Color(224, 80, 60));
    }
}

void WorldView::drawCorpse(const Corpse& c) {
    const Enemy& e = c.e;
    const EnemyDef& def = *e.def;
    if (e.sank || (c.t > 110 && (c.t >> 2) % 2)) return;
    const double k = def.scale;
    const bool animal = def.look == Look::Animal;
    const Sprite* img = nullptr;
    bool flip = false;
    if (animal) {
        const Frames& fr = spr_.animal(def.sprite, def.tint).get("dead")[animalRow(e.dir)];
        img = &fr[std::min(fr.size() - 1, static_cast<size_t>(c.t / 7))];
    } else if (def.look == Look::Mimic) img = &spr_.chest.back();
    else if (def.look == Look::Static) {   // no death frames: it reels back, slumps flat, darkens and fades
        g_.setAlpha(std::max(0.0, 1 - c.t / 50.0));
        const Sprite& im = spr_.monster(def.sprite, "#806060");
        const double fall = std::min(1.0, c.t / 18.0), ease = fall * (2 - fall);
        Warp wp;
        wp.lean = -e.face * (4 * ease + (c.t < 6 ? 1.5 * std::sin(c.t * 1.2) : 0));
        wp.squash = -0.45 * ease;
        drawWarp(im, std::round(e.x - 16 * k), std::round(e.y - 30 * k), 32 * k, 32 * k, e.face < 0, wp);
        g_.setAlpha(1);
        return;
    } else if (def.look == Look::Treant) {   // no death frames in the sheet: it sinks and fades
        g_.setAlpha(std::max(0.0, 1 - c.t / 40.0));
        img = &spr_.treant(def.tint)[treantRow(e.dir)][0];
    } else {
        const Frames& fr = spr_.character(def.sprite, def.tint).get("dead");
        img = &fr[std::min(fr.size() - 1, static_cast<size_t>(c.t / 7))];
        flip = e.face < 0;
    }
    g_.draw(*img, std::round(e.x - 16 * k), std::round(animal ? e.y - 28 * k : e.y - 30 * k), 32 * k, 32 * k, flip);
    g_.setAlpha(1);
}

// Darkness with soft round holes for every light. Computed on the CPU at half art-pixel resolution
// (a few thousand pixels) and stretched over the screen, so it works on every renderer.
void WorldView::drawLighting(Sim& sim, Map& m, int zoom, double alpha, Color col) {
    const int DW = g_.width(), DH = g_.height();
    const double cell = 2.0;   // art pixels per light-map pixel
    const int lw = static_cast<int>(std::ceil(DW / (zoom * cell))) + 1, lh = static_cast<int>(std::ceil(DH / (zoom * cell))) + 1;
    if (!light_ || light_.w != lw || light_.h != lh) {
        g_.destroy(light_);
        light_ = g_.makeStreaming(lw, lh, true);
        lightPx_.assign(static_cast<size_t>(lw) * lh * 4, 0);
        lightA_.assign(static_cast<size_t>(lw) * lh, 0);
    }
    // The shadow, deepening toward the edges of the screen (a vignette the lights still cut through)
    for (int y = 0; y < lh; y++)
        for (int x = 0; x < lw; x++) {
            const double vx = (x + 0.5) / lw * 2 - 1, vy = (y + 0.5) / lh * 2 - 1;
            const double v = std::clamp((std::sqrt(vx * vx * 0.8 + vy * vy) - 0.55) / 0.75, 0.0, 1.0);
            lightA_[static_cast<size_t>(y) * lw + x] = static_cast<float>(alpha + (1 - alpha) * 0.5 * v * v);
        }
    const double camX = offX / zoom, camY = offY / zoom;
    auto light = [&](double wx, double wy, double rad) {
        const double lx = (wx - camX) / cell, ly = (wy - camY) / cell, lr = rad / cell;
        const int x0 = std::max(0, static_cast<int>(lx - lr)), x1 = std::min(lw - 1, static_cast<int>(lx + lr) + 1);
        const int y0 = std::max(0, static_cast<int>(ly - lr)), y1 = std::min(lh - 1, static_cast<int>(ly + lr) + 1);
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                const double t = std::hypot(x + 0.5 - lx, y + 0.5 - ly) / lr;
                if (t >= 1) continue;
                const double f = t < 0.5 ? 1 - 0.3 * t : 0.85 * (1 - (t - 0.5) / 0.5);
                lightA_[static_cast<size_t>(y) * lw + x] *= static_cast<float>(1 - f);
            }
    };
    const int tick = sim.tickCount;
    light(sim.P.x, sim.P.y - 6, sim.hasItem("torch") ? 120 : m.dark ? 78 : 60);
    for (const auto& o : m.objects) {
        if (o.gone || std::abs(o.c * TILE - sim.P.x) > 420 || std::abs(o.r * TILE - sim.P.y) > 320) continue;
        if (o.kind == ObjKind::WallTorch) light(o.c * TILE + 8, o.r * TILE + 12, 54 + std::sin(tick * 0.2 + o.c) * 3);
        else if (o.kind == ObjKind::Campfire) light(o.c * TILE + 8, o.r * TILE + 6, 110 + std::sin(tick * 0.15) * 5);
        else if (o.kind == ObjKind::Shrine && tick >= o.at) light(o.c * TILE + 8, o.r * TILE, 40);
        else if (o.kind == ObjKind::Waypoint) light(o.c * TILE + 8, o.r * TILE + 8, 44);
        else if (o.kind == ObjKind::Travel) light(o.c * TILE + 8, o.r * TILE + 4, o.sprite == "portal" || o.sprite == "ritual" ? 56 : 36);
    }
    if (m.outdoor)   // town windows keep the streets lit at night
        for (const auto& t : db_.towns)
            if (t.realm == m.id && std::abs(t.at.c * TILE - sim.P.x) < 600 && std::abs(t.at.r * TILE - sim.P.y) < 500) light(t.at.c * TILE + 8, t.at.r * TILE + 8, t.radius * TILE * 0.9);
    for (const auto& p : m.proj)
        if (p.kind == Projectile::Kind::Knife) light(p.x, p.y, 14);
    for (const auto& e : m.enemies) {
        if (e.def->isBoss() && e.active) light(e.x, e.y - 14, 40);
        else if (e.elite) light(e.x, e.y - 8, 22);
    }
    for (size_t i = 0; i < lightA_.size(); i++) {
        lightPx_[i * 4] = col.r;
        lightPx_[i * 4 + 1] = col.g;
        lightPx_[i * 4 + 2] = col.b;
        lightPx_[i * 4 + 3] = static_cast<uint8_t>(std::clamp(lightA_[i], 0.0f, 1.0f) * 255);
    }
    g_.update(light_, lightPx_.data());
    g_.setTransform(1);
    g_.setAlpha(1);
    g_.draw(light_, 0, 0, lw * cell * zoom, lh * cell * zoom);
    g_.setTransform(zoom, offX, offY);
}

}   // namespace hc

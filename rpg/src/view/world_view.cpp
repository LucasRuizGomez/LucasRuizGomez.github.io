#include "view/world_view.h"

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
double frand() { return std::rand() / (RAND_MAX + 1.0); }

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
    out.x = c * TILE + 8 - out.img->w / 2 + (deep ? jx : 0);
    out.y = (r + 1) * TILE + (big ? 4 : 2) - out.img->h;
    return true;
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
    switch (t) {
        case GRASS:
        case FLOWERS:
        case TREE: {
            g_.draw(T.dirt[static_cast<int>(hv * 4)], x, y);
            // Grass over dirt: each 8x8 quarter picks an edge, corner or inner-corner piece from the
            // grass patch (x160 y224) and the grass-with-hole block (x240 y240) of the tileset.
            for (int qy = 0; qy < 2; qy++)
                for (int qx = 0; qx < 2; qx++) {
                    const int sx = qx ? 1 : -1, sy = qy ? 1 : -1;
                    const bool V = grassy(m.get(c, r + sy)), H = grassy(m.get(c + sx, r)), D = grassy(m.get(c + sx, r + sy));
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
                for (int i = 0; i < 2; i++) {
                    const Sprite& f = spr_.flowers[static_cast<int>(hash2(c, r, 30 + i) * 4)];
                    g_.draw(f, x + 1 + static_cast<int>(hash2(c, r, 32 + i) * (14 - f.w)) + i * 3, y + 2 + static_cast<int>(hash2(c, r, 34 + i) * (12 - f.h)));
                }
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
        default: break;
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
                p[0] = col.r;
                p[1] = col.g;
                p[2] = col.b;
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
    g_.clear(m.dark ? Color(7, 6, 11) : Color(33, 72, 42));
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
    for (int r = r0; r < std::min(m.h, r1 + 4); r++)
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
                g_.draw(*p.img, p.x + sh, p.y);
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

    // ── light and time of day ──
    if (m.dark) drawLighting(sim, m, zoom, 0.94, Color(6, 5, 12));
    else {
        const double dk = sim.darkness(), h = sim.hourOfDay();
        const double warm = std::max(0.0, 1 - std::abs(h - 18.6) / 1.4) + std::max(0.0, 1 - std::abs(h - 5.8) / 1.0);   // dusk and dawn
        if (warm > 0.01) {
            g_.setTransform(1);
            g_.fillRect(0, 0, DW, DH, Color(255, 130, 60, static_cast<uint8_t>(255 * 0.12 * std::min(1.0, warm))));
        }
        if (dk > 0.01) drawLighting(sim, m, zoom, 0.72 * dk, Color(10, 12, 34));
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
            drawAt(img, x + 8 + sh, y + 16 + (o.res == "herb" ? -1 : 2) + grow);
            break;
        }
        case ObjKind::Stump: drawAt(o.stage ? spr_.trees("").small : spr_.get("log"), x + 8, y + 16 + (o.stage ? 1 : 0)); break;
        case ObjKind::Pot: drawAt(spr_.get(o.sprite), x + 8, y + 15); break;
        case ObjKind::Prop: {
            const Frames& fr = spr_.frames(o.sprite);
            const Sprite& img = fr.size() > 1 ? anim(fr, tick + o.c * 7, 9) : spr_.get(o.sprite);
            const PropDef* pd = db_.prop(o.id);
            if (pd && pd->flat) g_.draw(img, x + 8 - img.w / 2, y + 8 - img.h / 2);
            else drawAt(img, x + 8 + (pd ? pd->ox : 0), y + 16 + (o.h > 1 ? 2 : 0));
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
            if (mark) {
                const Sprite& ms = spr_.get(mark == 1 ? "mark_new" : "mark_done");
                const double bob = std::round(std::sin(tick * 0.08 + o.c) * 1.5);
                if (mark == 2) g_.setAlpha(0.45);
                g_.draw(ms, x + 8 - ms.w / 2, y - 16 - ms.h + bob);
                g_.setAlpha(1);
            }
            break;
        }
        case ObjKind::Campfire:
            g_.draw(anim(spr_.campMeat, tick, 7), x - 8, y - 16);
            if (tick % 9 == 0) sim.particles.push_back({x + 6 + frand() * 4, y + 2, (frand() - 0.5) * 0.2, -0.4, 0, 30, 1, Color(255, 179, 71)});
            break;
        case ObjKind::WallTorch: g_.draw(anim(spr_.torch, tick + o.c * 5, 7), x, y + 1); break;
        case ObjKind::Trap:
            g_.draw(tilesAt(m, views_[m.id], o.c, o.r).slit, x, y);
            if (o.charge > 0) g_.fillRect(x + 7, y + 6, 2, 6, (tick >> 2) % 2 ? Color(255, 90, 74) : Color(255, 179, 71));
            break;
        case ObjKind::Gate: {
            const int f = !sim.doorOpen ? 0 : std::min(5, std::max(0, (tick - sim.gateAt) / 6));
            g_.draw(spr_.gate[std::min(spr_.gate.size() - 1, static_cast<size_t>(f))], x, y - 16);
            break;
        }
        case ObjKind::Entrance: drawAt(spr_.get(o.sprite), x + 8, y + 18); break;
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

void WorldView::drawPlayer(Sim& sim) {
    const Player& P = sim.P;
    const int tick = sim.tickCount, atkT = db_.tuning.attackTicks;
    const CharSet& S = spr_.character("ch8", "");
    const bool flip = P.face < 0;
    const double x = std::round(P.x) - 16;
    double y = std::round(P.y) - 30;
    if (sim.mode == Mode::Dead) {
        const Frames& dead = S.get("dead");
        g_.draw(dead[std::min(dead.size() - 1, static_cast<size_t>((tick - sim.deadAt) / 10))], x, y, flip);
        return;
    }
    if (P.iframes > 0 && P.roll <= 0 && (tick >> 2) % 2 == 0) return;
    const Sprite* img;
    const bool side = P.dir == Dir::Left || P.dir == Dir::Right;
    if (P.atkT > 0 && side) img = &progress(S.get("atk"), atkT, P.atkT);
    else if (P.bowT > 0) img = &progress(S.get("thrust"), 10, P.bowT);
    else if (!P.mount.empty()) img = &S.get("idle")[0];
    else img = P.moving || P.roll > 0 ? &anim(S.get("walk"), P.animT, 7) : &anim(S.get("idle"), tick, 12);

    for (const auto& t : P.trail) {   // white ghosts behind a dodge roll
        const Sprite& ghost = S.get("walk")[0];
        g_.drawTinted(ghost, std::round(t.x) - 16, std::round(t.y) - 30, 32, 32, Color(255, 255, 255, static_cast<uint8_t>(t.t / 10.0 * 0.5 * 255)), true, t.face < 0);
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
    } else g_.fillRect(x + 11, y + 30, 10, 2, Color(0, 0, 0, 64));

    // The hero's sheet only swings sideways, so up/down swings show the blade and an arc
    auto weaponPass = [&] {
        if (P.atkT <= 0) return;
        const Vec2 d = dirVec(P.dir);
        const double p = 1 - static_cast<double>(P.atkT) / atkT, ext = p < 0.4 ? p / 0.4 : 1 - (p - 0.4) / 0.6;
        if (const Item* w = sim.equipped(EquipSlot::Weapon)) {
            auto rot = spr_.weaponRot.find(w->id);
            if (rot != spr_.weaponRot.end()) {
                const Sprite& ws = rot->second[static_cast<int>(P.dir)];
                const double cx = P.x + d.x * (6 + ext * 7), cy = P.y - 6 + d.y * (6 + ext * 7);
                g_.draw(ws, std::round(cx - ws.w / 2.0), std::round(cy - ws.h / 2.0));
            }
        }
        if (p > 0.1 && p < 0.75) {
            const double R = sim.weaponReach() * 0.85 + 4, a0 = std::atan2(d.y, d.x), span = 1.15, frac = (p - 0.1) / 0.65;
            pixelArc(P.x, P.y - 6, R, a0 - span, a0 - span + span * 2 * std::min(1.0, frac * 1.6), Color(255, 255, 255, static_cast<uint8_t>(255 * (0.9 - frac * 0.6))));
        }
    };
    if (P.dir == Dir::Up) weaponPass();
    g_.draw(*img, x, y, flip);
    if (P.dir != Dir::Up) weaponPass();

    if (P.whirlT > 0) {   // whirlwind: an expanding ring of steel
        const double k = 1 - P.whirlT / 18.0;
        pixelArc(P.x, P.y - 6, 8 + k * (db_.tuning.whirlRadius - 8), 0, PI * 2, Color(255, 255, 255, static_cast<uint8_t>(255 * (1 - k))));
        pixelArc(P.x, P.y - 6, 6 + k * (db_.tuning.whirlRadius - 10), tick * 0.9, tick * 0.9 + 2.2, Color(255, 236, 170));
    }
    if (P.recallT > 0) g_.drawTinted(spr_.glow, P.x - 18, P.y - 30, 36, 40, Color(140, 200, 255, static_cast<uint8_t>(120 + 80 * std::sin(tick * 0.3))), true);
    if (P.mountCast > 0) g_.fillRect(x + 6, y - 4, 20.0 * (1 - static_cast<double>(P.mountCast) / db_.tuning.mountCastTicks), 2, Color(242, 210, 122));
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
    const double dx = std::round(e.x - 16 * k) + shake, dy = std::round(top) + crouch;
    g_.draw(img, dx, dy, 32 * k, 32 * k, flip);
    const bool flash = (e.flash > 0 && e.flash % 4 < 2) || (def.isBoss() && e.state == Enemy::State::VolleyWind && tick % 6 < 3);
    if (flash) g_.drawTinted(img, dx, dy, 32 * k, 32 * k, Color(255, 255, 255), true, flip);
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
    else if (def.look == Look::Treant) {   // no death frames in the sheet: it sinks and fades
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
    std::fill(lightA_.begin(), lightA_.end(), static_cast<float>(alpha));
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
    }
    if (m.outdoor)   // town windows keep the streets lit at night
        for (const auto& t : db_.towns)
            if (std::abs(t.at.c * TILE - sim.P.x) < 600 && std::abs(t.at.r * TILE - sim.P.y) < 500) light(t.at.c * TILE + 8, t.at.r * TILE + 8, t.radius * TILE * 0.9);
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

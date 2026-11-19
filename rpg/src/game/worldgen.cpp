// Builds the overworld (zones, towns, roads, points of interest, spawn points) and the dungeons.
// Everything is derived from seeds, so the same config always gives the same world.
#include "game/world.h"

#include <cmath>
#include <set>

namespace hc {

const ZoneDef& zoneAt(const Database& db, const Map& over, int c, int r) {
    c = std::clamp(c, 0, over.w - 1);
    r = std::clamp(r, 0, over.h - 1);
    return db.zones[over.zone.empty() ? 0 : over.zone[over.idx(c, r)]];
}

const TownDef* townAt(const Database& db, int c, int r, int margin, const std::string& realm) {
    for (const auto& t : db.towns)
        if (t.realm == realm && hypot2(c - t.at.c, r - t.at.r) <= t.radius + margin) return &t;
    return nullptr;
}

namespace {

const int DC4[4] = {1, -1, 0, 0}, DR4[4] = {0, 0, 1, -1};

const char* compass(double dx, double dy) {
    static const char* names[8] = {"east", "south-east", "south", "south-west", "west", "north-west", "north", "north-east"};
    const double a = std::atan2(dy, dx);
    return names[((static_cast<int>(std::lround(a / (PI / 4))) % 8) + 8) % 8];
}

struct OverworldGen {
    const Database& db;
    Map m;
    Rng rnd;
    int W, H, S;
    std::vector<TilePos> poiSpots;
    int chestCount = 0;

    // One outdoor map: the main one ("over"), or a realm of its own (realms in world.json)
    std::string realm;
    const RealmDef* rd = nullptr;
    std::vector<TownDef> towns;         // the towns, dungeons and regions on this map only
    std::vector<DungeonDef> dungeons;
    static int sizeOf(const Database& d, const std::string& id, bool w) {
        const RealmDef* r = d.realm(id);
        return r ? (w ? r->w : r->h) : (w ? d.tuning.worldW : d.tuning.worldH);
    }
    static uint32_t seedOf(const Database& d, const std::string& id) {
        const RealmDef* r = d.realm(id);
        return r ? r->seed : d.tuning.worldSeed;
    }
    OverworldGen(const Database& d, const std::string& id)
        : db(d), m(makeMap(id, sizeOf(d, id, true), sizeOf(d, id, false), GRASS)), rnd(seedOf(d, id)), W(sizeOf(d, id, true)), H(sizeOf(d, id, false)),
          S(static_cast<int>(seedOf(d, id))), realm(id), rd(d.realm(id)) {
        m.name = rd ? rd->name : "The Realm";
        for (const auto& t : d.towns)
            if (t.realm == id) towns.push_back(t);
        for (const auto& dd : d.dungeons)
            if (dd.realm == id) dungeons.push_back(dd);
        m.outdoor = true;
        m.zone.assign(static_cast<size_t>(W) * H, 0);
        m.explored.assign(static_cast<size_t>(W) * H, 0);
    }

    const ZoneDef& zone(int c, int r) const { return zoneAt(db, m, c, r); }
    bool inner(int c, int r, int pad = 2) const { return c >= pad && r >= pad && c < W - pad && r < H - pad; }

    void disk(double cx, double cy, double rad, uint8_t t) {
        for (int r = ifloor(cy - rad); r <= cy + rad; r++)
            for (int c = ifloor(cx - rad); c <= cx + rad; c++)
                if ((c - cx) * (c - cx) + (r - cy) * (r - cy) <= rad * rad && inner(c, r)) m.set(c, r, t);
    }

    bool areaFree(int c, int r, int w, int h) const {   // footprint: bottom row r, centred on c
        for (int rr = r - h + 1; rr <= r; rr++)
            for (int cc = c - w / 2; cc <= c - w / 2 + w - 1; cc++) {
                if (!inner(cc, rr)) return false;
                if (m.block[m.idx(cc, rr)] || m.objectAt(cc, rr)) return false;
                const uint8_t t = m.get(cc, rr);
                if (!walkable(t) || t == STAIRS_DOWN) return false;
            }
        return true;
    }

    bool placeProp(const std::string& id, int c, int r, bool force = false) {
        const PropDef* p = db.prop(id);
        if (!p) return false;
        if (!force && !areaFree(c, r, p->w, p->h)) return false;
        Object o;
        o.kind = ObjKind::Prop;
        o.id = id;
        o.sprite = p->sprite;
        o.c = c;
        o.r = r;
        o.w = p->w;
        o.h = p->h;
        o.solid = p->solid;
        m.addObject(std::move(o));
        return true;
    }

    Object& addChest(int c, int r, std::vector<Item> loot, int lootLevel, bool mimic = false) {
        Object o;
        o.kind = ObjKind::Chest;
        o.id = "ow_" + std::to_string(chestCount++);
        o.c = c;
        o.r = r;
        o.solid = true;
        o.loot = std::move(loot);
        o.lootLevel = lootLevel;
        o.mimic = mimic;
        m.addObject(std::move(o));
        return m.objects.back();
    }

    void addNode(const std::string& res, int c, int r) {
        Object o;
        o.kind = ObjKind::Node;
        o.res = res;
        o.c = c;
        o.r = r;
        o.max = o.left = res == "herb" ? 2 : 3;
        o.solid = res != "herb";
        m.addObject(std::move(o));
    }

    void addSpawn(int c, int r, const std::string& enemy, bool elite = false) {
        SpawnPoint s;
        s.c = c;
        s.r = r;
        s.def = &db.enemy(enemy);
        s.forceElite = elite;
        m.spawns.push_back(s);
    }

    // ── 1. zones and raw terrain ──
    void terrain() {
        for (int r = 0; r < H; r++)
            for (int c = 0; c < W; c++) {
                const double wx = c + (fbm(c / 26.0, r / 26.0, S + 50) - 0.5) * 44, wy = r + (fbm(c / 26.0, r / 26.0, S + 51) - 0.5) * 44;
                double best = 1e18;
                int bi = 0;
                for (const auto& z : db.zones)
                    for (const auto& ctr : z.centers) {
                        if (z.realm != realm) break;
                        const double d = (wx - ctr.c) * (wx - ctr.c) + (wy - ctr.r) * (wy - ctr.r);
                        if (d < best) {
                            best = d;
                            bi = z.index;
                        }
                    }
                m.zone[m.idx(c, r)] = static_cast<uint8_t>(bi);
            }
        for (int r = 0; r < H; r++)
            for (int c = 0; c < W; c++) {
                const ZoneDef& z = zone(c, r);
                const double wv = fbm(c / 14.0, r / 14.0, S + 1), fv = fbm(c / 9.0, r / 9.0, S + 2), rv = fbm(c / 7.0, r / 7.0, S + 3);
                uint8_t t = GRASS;
                if (wv < z.water) t = WATER;
                else if (fv > z.trees) t = TREE;
                if (rv > z.rocks && t != WATER) t = ROCK;
                if (!rd && r > H - 7 - fbm(c / 12.0, 0.5, S + 4) * 9) t = WATER;   // the southern sea
                if (rd && rd->shape == "island") {   // the sea closes in on every side
                    const double ex = (c - W / 2.0) / (W * 0.44), ey = (r - H / 2.0) / (H * 0.42);
                    if (ex * ex + ey * ey > 0.75 + (fbm(c / 10.0, r / 10.0, S + 5) - 0.5) * 0.5) t = WATER;
                }
                m.set(c, r, t);
            }
        if (!rd) {
            river(100, 40, H - 1, 90, 112);
            river(184, 0, 100, 174, 192);
        } else if (rd->shape != "island")
            river(W / 2 + static_cast<int>(rnd.range(20)) - 10, 0, H - 1, W / 2 - 22, W / 2 + 22);
        for (int r = 0; r < H; r++)
            for (int c = 0; c < W; c++)
                if (c < 2 || r < 2 || c >= W - 2 || r >= H - 2) m.set(c, r, TREE);
    }

    void river(int startC, int r0, int r1, int minC, int maxC) {
        int rx = startC;
        for (int r = r0; r <= r1; r++) {
            rx += static_cast<int>(std::lround((rnd.next() - 0.5) * 2.2));
            rx = std::clamp(rx, minC, maxC);
            for (int k = 0; k < 3; k++) m.set(rx + k, r, WATER);
        }
    }

    // ── 2. towns ──
    void buildTown(const TownDef& t) {
        const int tc = t.at.c, tr = t.at.r, R = t.radius;
        disk(tc, tr, R + 0.5, GRASS);
        const bool big = t.houses >= 3, fountain = t.has("fountain");
        const int pw = big ? 5 : 4, ph = big ? 3 : 2;
        for (int r = tr - ph; r <= tr + ph; r++)
            for (int c = tc - pw; c <= tc + pw; c++) m.set(c, r, PLAZA);

        if (fountain) placeProp("fountain", tc, tr, true);
        {
            Object fire;
            fire.kind = ObjKind::Campfire;
            fire.c = fountain ? tc + 3 : tc;
            fire.r = fountain ? tc * 0 + tr + 2 : tr;
            fire.solid = true;
            fire.id = t.id;
            m.addObject(std::move(fire));
        }
        if (t.has("waypoint")) {
            Object wp;
            wp.kind = ObjKind::Waypoint;
            wp.c = tc - 3;
            wp.r = tr + (fountain ? 2 : 1);
            wp.id = t.id;
            wp.text = t.name;
            m.addObject(std::move(wp));
        }

        // Buildings around the square, nearest plots first, each with a cobbled path from its door.
        // The door tile is a way in: it links to the building's own interior map.
        static const int PLOT[9][2] = {{0, -9}, {-9, -4}, {9, -4}, {-10, 6}, {10, 6}, {-16, -3}, {16, -3}, {-12, 12}, {12, 12}};
        int plot = 0;
        for (size_t bi = 0; bi < t.buildings.size(); bi++) {
            const BuildingDef* bd = db.building(t.buildings[bi]);
            if (!bd) continue;
            while (plot < 9 && hypot2(PLOT[plot][0], PLOT[plot][1]) + 3 > R) plot++;
            if (plot >= 9) break;
            const int hc = tc + PLOT[plot][0], hr = tr + PLOT[plot][1];
            plot++;
            for (int r = hr - 5; r <= hr + 1; r++)
                for (int c = hc - 3; c <= hc + 3; c++)
                    if (inner(c, r) && m.get(c, r) != PLAZA) m.set(c, r, GRASS);
            if (!placeProp(bi % 2 ? "house_b" : "house_a", hc, hr)) continue;
            m.objects.back().text = bd->sign;   // the little board over the door
            m.objects.back().res = bd->id;
            m.set(hc, hr, STAIRS_DOWN);
            m.block[m.idx(hc, hr)] = 0;
            m.links.push_back({hc, hr, "b_" + bd->id, -1, -1, Dir::Up});
            m.doors[bd->id] = {hc, hr};
            int c = hc, r = hr + 1;
            for (int guard = 0; guard < 40 && (std::abs(c - tc) > pw || std::abs(r - tr) > ph); guard++) {
                if (m.get(c, r) == GRASS) m.set(c, r, COBBLE);
                if (std::abs(c - tc) > pw && !m.block[m.idx(c + sign(tc - c), r)]) c += sign(tc - c);
                else if (r != tr) r += sign(tr - r);
                else c += sign(tc - c);
            }
            placeProp(bi % 2 ? "barrel" : "crate", hc + 3, hr);
            if (hash2(hc, hr, S + 70) < 0.6) placeProp("plant", hc - 3, hr);
        }

        // Market stalls on the north edge of the square
        std::vector<TilePos> stallSpots;
        static const int STALL[2] = {1, -1};
        for (int i = 0; i < std::min(t.stalls, 2); i++) {
            const int sc = tc + STALL[i] * (pw - 1), sr = tr - ph;
            if (placeProp("stall", sc, sr)) {
                stallSpots.push_back({sc, sr + 1});
                placeProp(i ? "veg2" : "veg1", sc + 2 * STALL[i], sr);
                placeProp("sack", sc - 2 * STALL[i], sr);
            }
        }

        // People: traders stand at a stall when there is one, everyone else around the square
        static const int SLOT[7][2] = {{-2, 0}, {2, 0}, {0, 1}, {-4, -1}, {4, -1}, {-3, 1}, {3, 1}};
        size_t nextStall = 0;
        int nextSlot = 0;
        for (const auto& n : t.npcs) {
            TilePos p{tc, tr + ph + 1};
            const bool trader = n.role == "merchant" || n.role == "smith" || n.role == "herbalist";
            if (trader && nextStall < stallSpots.size()) p = stallSpots[nextStall++];
            else {
                for (; nextSlot < 7; nextSlot++) {
                    const TilePos q{tc + SLOT[nextSlot][0], tr + (SLOT[nextSlot][1] <= 0 ? ph + SLOT[nextSlot][1] : ph + SLOT[nextSlot][1])};
                    if (areaFree(q.c, q.r, 1, 1)) {
                        p = q;
                        nextSlot++;
                        break;
                    }
                }
            }
            Object o;
            o.kind = ObjKind::Npc;
            o.id = n.id;
            o.sprite = n.sprite;
            o.c = p.c;
            o.r = p.r;
            o.solid = true;
            m.addObject(std::move(o));
            if (n.role == "stable") {
                placeProp("hut", p.c - 2, p.r);
                placeProp("log", p.c + 2, p.r);
            }
        }

        placeProp("banner", tc - pw - 1, tr + ph + 1);
        placeProp("banner", tc + pw + 1, tr + ph + 1);
        placeProp("bench", tc - pw - 3, tr);
        placeProp("barrel2", tc + pw + 1, tr - 1);
        placeProp("crate2", tc + pw + 2, tr - 1);
        if (big) placeProp("statue", tc, tr - ph - 2);
        if (t.has("towers")) {
            const int k = static_cast<int>((R - 1) * 0.7);
            placeProp("tower", tc - k, tr - k + 2);
            placeProp("tower_roof", tc + k, tr - k + 2);
            placeProp("tower_roof", tc - k, tr + k);
            placeProp("tower", tc + k, tr + k);
        }
        if (t.has("fence"))
            for (int r = tr - R; r <= tr + R; r++)
                for (int c = tc - R; c <= tc + R; c++)
                    if (static_cast<int>(std::lround(hypot2(c - tc, r - tr))) == R && hash2(c, r, S + 71) < 0.7 && (c + r) % 2 == 0) placeProp("fence", c, r);
        if (t.has("start")) m.start = {tc, tr + ph + 2};
    }

    void townSigns() {
        for (const auto& t : towns) {
            std::string text = t.name + ".";
            std::vector<std::pair<double, const TownDef*>> others;
            for (const auto& o : towns)
                if (o.id != t.id) others.push_back({hypot2(o.at.c - t.at.c, o.at.r - t.at.r), &o});
            std::sort(others.begin(), others.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            for (size_t i = 0; i < std::min<size_t>(3, others.size()); i++)
                text += std::string(" ") + others[i].second->name + ": " + compass(others[i].second->at.c - t.at.c, others[i].second->at.r - t.at.r) + ".";
            for (const auto& d : dungeons)
                if (d.under.empty() && hypot2(d.at.c - t.at.c, d.at.r - t.at.r) < 70) text += " " + d.name + ": " + compass(d.at.c - t.at.c, d.at.r - t.at.r) + ".";
            const int pw = t.houses >= 3 ? 5 : 4, ph = t.houses >= 3 ? 3 : 2;
            for (int k = 0; k < 6; k++) {
                const int c = t.at.c + pw + 1 + k % 2, r = t.at.r + ph + 2 + k / 2;
                if (!areaFree(c, r, 1, 1)) continue;
                Object o;
                o.kind = ObjKind::Sign;
                o.c = c;
                o.r = r;
                o.solid = true;
                o.id = t.id;
                o.sprite = "sign";
                o.text = text;
                m.addObject(std::move(o));
                break;
            }
        }
    }

    // ── 3. dungeon entrances: a door in a rock face, reached from the south ──
    void buildEntrance(const DungeonDef& d) {
        const int c = d.at.c, r = d.at.r;
        disk(c, r + 2, 3.8, GRASS);
        for (int rr = r - 4; rr <= r; rr++)
            for (int cc = c - 5; cc <= c + 5; cc++) {
                const double dist = hypot2(cc - c, rr - r);
                if (dist >= 1 && dist < 4.6 && inner(cc, rr)) m.set(cc, rr, ROCK);
            }
        m.set(c, r, STAIRS_DOWN);
        m.set(c, r + 1, GRASS);
        Object o;
        o.kind = ObjKind::Entrance;
        o.c = c;
        o.r = r;
        o.id = d.id;
        o.sprite = "entrance_" + d.entrance;
        o.text = d.name;
        m.objects.push_back(std::move(o));
        m.links.push_back({c, r, d.id, -1, -1, Dir::Up});
    }

    // ── 4. sand, flowers ──
    void dressing() {
        for (int r = 1; r < H - 1; r++)
            for (int c = 1; c < W - 1; c++) {
                if (m.get(c, r) != GRASS || townAt(db, c, r, 0, realm)) continue;
                const ZoneDef& z = zone(c, r);
                bool nearWater = false;
                const int reach = z.sandy ? 2 : 1;
                for (int dr = -reach; dr <= reach && !nearWater; dr++)
                    for (int dc = -reach; dc <= reach; dc++)
                        if (std::abs(dc) + std::abs(dr) <= reach && m.get(c + dc, r + dr) == WATER) {
                            nearWater = true;
                            break;
                        }
                if (nearWater && hash2(c, r, S + 9) < (z.sandy ? 0.92 : 0.7)) m.set(c, r, SAND);
                else if (hash2(c, r, S + 11) < z.flowers) m.set(c, r, FLOWERS);
            }
    }

    // ── 5. roads between towns and to every dungeon ──
    void roads() {
        struct Node {
            TilePos p;
            bool town;
        };
        std::vector<Node> nodes;
        for (const auto& t : towns) nodes.push_back({{t.at.c, t.at.r + (t.houses >= 3 ? 4 : 3)}, true});
        const size_t towns = nodes.size();
        for (const auto& d : dungeons)
            if (d.under.empty()) nodes.push_back({{d.at.c, d.at.r + 1}, false});
        std::set<std::pair<size_t, size_t>> edges;
        auto nearestTowns = [&](size_t i, size_t n) {
            std::vector<std::pair<double, size_t>> ds;
            for (size_t j = 0; j < towns; j++)
                if (j != i) ds.push_back({hypot2(nodes[i].p.c - nodes[j].p.c, nodes[i].p.r - nodes[j].p.r), j});
            std::sort(ds.begin(), ds.end());
            for (size_t k = 0; k < std::min(n, ds.size()); k++) edges.insert({std::min(i, ds[k].second), std::max(i, ds[k].second)});
        };
        for (size_t i = 0; i < nodes.size(); i++) nearestTowns(i, nodes[i].town ? 2 : 1);

        for (const auto& [a, b] : edges) {
            const auto path = astar(m, nodes[a].p, nodes[b].p, [&](int c, int r) -> double {
                if (m.block[m.idx(c, r)]) return -1;
                const uint8_t t = m.get(c, r);
                if (t == STAIRS_DOWN) return -1;
                if (t == PATH || t == BRIDGE || t == COBBLE || t == PLAZA) return 0.35;
                if (t == ROCK) return 9;
                if (t == TREE) return 5;
                if (t == WATER) return r > H - 16 ? 40 : 7;
                return 1 + hash2(c, r, 77) * 0.6;
            });
            for (const auto& p : path) {
                const uint8_t t = m.get(p.c, p.r);
                if (t == PLAZA || t == COBBLE || t == STAIRS_DOWN || t == BRIDGE) continue;
                if (t == WATER) m.set(p.c, p.r, BRIDGE);
                else m.set(p.c, p.r, townAt(db, p.c, p.r, -1, realm) ? COBBLE : PATH);
            }
        }
        // Fences give way where a road passes
        for (auto& o : m.objects) {
            if (o.kind != ObjKind::Prop || o.id != "fence") continue;
            for (int dr = -1; dr <= 1 && !o.gone; dr++)
                for (int dc = -1; dc <= 1; dc++) {
                    const uint8_t t = m.get(o.c + dc, o.r + dr);
                    if (t == PATH || t == COBBLE || t == BRIDGE) {
                        o.gone = true;
                        m.setBlock(o, false);
                        break;
                    }
                }
        }
        std::erase_if(m.objects, [](const Object& o) { return o.gone; });
    }

    // ── 7b. high ground: plateaus one or two levels up, ringed by cliffs, climbed by stone ramps ──
    // Cliffs face south (the face hangs below the edge, the way the eye expects in a top-down view); the other
    // edges are a rocky lip on the high ground itself. Kept clear of roads, water, towns and anything placed.
    std::vector<TilePos> highGround;
    void plateaus() {
        m.height.assign(static_cast<size_t>(W) * H, 0);
        std::vector<uint8_t> keep(static_cast<size_t>(W) * H, 0);   // 1 = must stay low and open
        for (int r = 0; r < H; r++)
            for (int c = 0; c < W; c++) {
                const uint8_t t = m.get(c, r);
                const bool feature = !(isGround(t) || t == TREE || t == ROCK) || m.block[m.idx(c, r)];
                if (c < 5 || r < 5 || c >= W - 5 || r >= H - 6 || townAt(db, c, r, 7, realm)) keep[m.idx(c, r)] = 1;
                if (!feature) continue;
                for (int dr = -3; dr <= 4; dr++)
                    for (int dc = -3; dc <= 3; dc++)
                        if (m.inside(c + dc, r + dr)) keep[m.idx(c + dc, r + dr)] = 1;
            }
        for (const auto& o : m.objects)
            for (int dr = -4; dr <= 4; dr++)
                for (int dc = -4; dc <= 4; dc++)
                    if (m.inside(o.c + dc, o.r + dr)) keep[m.idx(o.c + dc, o.r + dr)] = 1;
        // raw heights from noise, then opened (eroded and grown back) so no plateau is a sliver
        std::vector<uint8_t> h(static_cast<size_t>(W) * H, 0);
        for (int r = 0; r < H; r++)
            for (int c = 0; c < W; c++) {
                if (keep[m.idx(c, r)] || zone(c, r).snow) continue;
                const double n = fbm(c / 21.0, r / 21.0, S + 77);
                h[m.idx(c, r)] = n > 0.71 ? 2 : n > 0.58 ? 1 : 0;
            }
        auto open = [&](int level, int rad) {
            std::vector<uint8_t> er(h.size(), 0);
            for (int r = rad; r < H - rad; r++)
                for (int c = rad; c < W - rad; c++) {
                    bool all = true;
                    for (int dr = -rad; dr <= rad && all; dr++)
                        for (int dc = -rad; dc <= rad; dc++)
                            if (h[m.idx(c + dc, r + dr)] < level) {
                                all = false;
                                break;
                            }
                    er[m.idx(c, r)] = all;
                }
            for (int r = rad; r < H - rad; r++)
                for (int c = rad; c < W - rad; c++) {
                    if (h[m.idx(c, r)] < level) continue;
                    bool near = false;
                    for (int dr = -rad; dr <= rad && !near; dr++)
                        for (int dc = -rad; dc <= rad; dc++)
                            if (er[m.idx(c + dc, r + dr)]) {
                                near = true;
                                break;
                            }
                    if (!near) h[m.idx(c, r)] = static_cast<uint8_t>(level - 1);
                }
        };
        open(1, 3);
        open(2, 3);
        auto H_ = [&](int c, int r) { return m.inside(c, r) ? h[m.idx(c, r)] : 0; };
        // the upper level sits well inside the lower one, so each cliff is one step high
        for (int r = 0; r < H; r++)
            for (int c = 0; c < W; c++)
                if (H_(c, r) == 2)
                    for (int dr = -3; dr <= 6; dr++)
                        for (int dc = -3; dc <= 3; dc++)
                            if (H_(c + dc, r + dr) == 0) h[m.idx(c, r)] = 1;
        m.height = h;
        // cliffs: a face below every south edge, a lip along the others
        for (int r = 1; r < H - 1; r++)
            for (int c = 1; c < W - 1; c++) {
                const int here = H_(c, r);
                if (H_(c, r - 1) > here || H_(c, r - 2) > here) {   // the face of the ground above: two tiles tall
                    m.set(c, r, CLIFF);
                    continue;
                }
                if (here == 0) continue;
                // a lip wherever lower ground touches, except along a plain south edge (the face below closes that)
                const bool s = H_(c, r + 1) < here;
                const bool other = H_(c - 1, r) < here || H_(c + 1, r) < here || H_(c, r - 1) < here || H_(c - 1, r - 1) < here || H_(c + 1, r - 1) < here;
                const bool diagS = H_(c - 1, r + 1) < here || H_(c + 1, r + 1) < here;
                if (other || (diagS && !s)) m.set(c, r, CLIFF);
            }
        // ramps: on each run of south face, a stair every so often (at least one per plateau)
        std::vector<uint8_t> seen(h.size(), 0);
        for (int r = 1; r < H - 1; r++)
            for (int c = 1; c < W - 1; c++) {
                if (seen[m.idx(c, r)] || H_(c, r) == 0) continue;
                // one plateau (one level): gather its south faces
                const int lv = H_(c, r);
                std::vector<int> stack{m.idx(c, r)};
                seen[m.idx(c, r)] = 1;
                std::vector<TilePos> faces;
                while (!stack.empty()) {
                    const int i = stack.back();
                    stack.pop_back();
                    const int pc = i % W, pr = i / W;
                    if (m.get(pc, pr + 1) == CLIFF && H_(pc, pr + 1) < lv && H_(pc, pr + 2) < lv && m.get(pc, pr + 2) == CLIFF && m.get(pc, pr + 3) != CLIFF)
                        faces.push_back({pc, pr + 1});   // the upper tile of a two-tile face
                    for (int k = 0; k < 4; k++) {
                        const int nc = pc + DC4[k], nr = pr + DR4[k];
                        if (!m.inside(nc, nr) || seen[m.idx(nc, nr)] || H_(nc, nr) != lv) continue;
                        seen[m.idx(nc, nr)] = 1;
                        stack.push_back(m.idx(nc, nr));
                    }
                }
                // a ramp needs open ground above and below it, and cliff on both sides
                auto rampOk = [&](TilePos f) {
                    return m.tileFree(f.c, f.r + 2, false) && walkable(m.get(f.c, f.r - 1)) && m.get(f.c, f.r - 1) != CLIFF && m.get(f.c - 1, f.r) == CLIFF &&
                           m.get(f.c + 1, f.r) == CLIFF && m.get(f.c - 1, f.r + 1) == CLIFF && m.get(f.c + 1, f.r + 1) == CLIFF;
                };
                // remember an open cell of it, so connectivity() can make sure it is reachable
                for (int k = 0; k < 4; k++)
                    if (walkable(m.get(c + DC4[k], r + DR4[k])) && H_(c + DC4[k], r + DR4[k]) == lv) {
                        highGround.push_back({c + DC4[k], r + DR4[k]});
                        break;
                    }
                int placed = 0;
                for (size_t i = 0; i < faces.size(); i++) {
                    const TilePos f = faces[(i * 7 + static_cast<size_t>(hash2(c, r, S + 78) * faces.size())) % faces.size()];
                    if (!rampOk(f)) continue;
                    bool close = false;
                    for (const auto& o : faces)
                        if (m.get(o.c, o.r) == RAMP && std::abs(o.c - f.c) + std::abs(o.r - f.r) < 14) close = true;
                    if (close) continue;
                    m.set(f.c, f.r, RAMP);
                    m.set(f.c, f.r + 1, RAMP);
                    if (++placed >= 1 + static_cast<int>(faces.size() / 24)) break;
                }
            }
    }

    // ── 6. points of interest ──
    bool poiSpot(TilePos& out, int clear) {
        for (int tries = 0; tries < 400; tries++) {
            const int c = 6 + rnd.range(W - 12), r = 6 + rnd.range(H - 18);
            if (townAt(db, c, r, 10, realm)) continue;
            bool ok = true;
            for (const auto& d : dungeons)
                if (d.under.empty() && hypot2(c - d.at.c, r - d.at.r) < 10) ok = false;
            for (const auto& p : poiSpots)
                if (hypot2(c - p.c, r - p.r) < 13) ok = false;
            if (!ok) continue;
            int ground = 0, total = 0;
            for (int dr = -clear; dr <= clear; dr++)
                for (int dc = -clear; dc <= clear; dc++) {
                    total++;
                    const uint8_t t = m.get(c + dc, r + dr);
                    if (t == GRASS || t == FLOWERS || t == SAND || t == TREE || t == ROCK) ground++;
                    if (t == PATH || t == BRIDGE) ground -= 3;
                }
            if (ground < total * 0.85) continue;
            out = {c, r};
            poiSpots.push_back(out);
            return true;
        }
        return false;
    }

    std::string campEnemy(const ZoneDef& z, int i) const {
        if (z.id == "coast") return i % 3 == 2 ? "smuggler_archer" : i % 3 == 1 ? "smuggler_spear" : "smuggler";
        if (z.id == "frost") return i % 2 ? "miner_spear" : "miner";
        if (z.id == "marsh") return i % 2 ? "drowned_archer" : "drowned";
        return i % 3 == 2 ? "bandit_archer" : "bandit";
    }

    std::vector<Item> stash(const ZoneDef& z, int kind) {
        std::vector<Item> loot;
        loot.push_back({"gold", 3 + z.levelMin / 4 + rnd.range(6)});
        static const char* extra[5] = {"bandage", "arrows", "roast", "herb", "wood"};
        const char* e = extra[rnd.range(5)];
        loot.push_back({e, std::string(e) == "arrows" ? 8 : 2});
        if (kind > 0 && z.levelMin >= 25) loot.push_back({"potion", 1});
        return loot;
    }

    void clearGround(int c, int r, int rad) {
        for (int dr = -rad; dr <= rad; dr++)
            for (int dc = -rad; dc <= rad; dc++) {
                const uint8_t t = m.get(c + dc, r + dr);
                if ((t == TREE || t == ROCK) && inner(c + dc, r + dr)) m.set(c + dc, r + dr, GRASS);
            }
    }

    void pois() {
        auto each = [&](const char* kind, int clear, const std::function<void(int, int, const ZoneDef&)>& build) {
            auto it = db.pois.find(kind);
            const int n = it == db.pois.end() ? 0 : it->second;
            for (int i = 0; i < n; i++) {
                TilePos p;
                if (poiSpot(p, clear)) build(p.c, p.r, zone(p.c, p.r));
            }
        };
        each("shrine", 2, [&](int c, int r, const ZoneDef&) {
            clearGround(c, r, 2);
            Object o;
            o.kind = ObjKind::Shrine;
            o.c = c;
            o.r = r;
            o.solid = true;
            o.sprite = "statue";
            o.id = "shrine_" + std::to_string(c) + "_" + std::to_string(r);
            m.addObject(std::move(o));
            for (int k = 0; k < 4; k++)
                if (m.get(c + DC4[k] * 2, r + DR4[k] * 2) == GRASS) m.set(c + DC4[k] * 2, r + DR4[k] * 2, FLOWERS);
        });
        each("camp", 3, [&](int c, int r, const ZoneDef& z) {
            clearGround(c, r, 3);
            Object fire;
            fire.kind = ObjKind::Campfire;
            fire.c = c;
            fire.r = r;
            fire.solid = true;
            fire.id = "camp";
            m.addObject(std::move(fire));
            placeProp("crate", c - 2, r - 1);
            placeProp("barrel", c + 2, r - 1);
            placeProp("log", c - 1, r + 2);
            if (areaFree(c + 2, r + 1, 1, 1)) addChest(c + 2, r + 1, stash(z, 1), z.levelMin);
            for (int i = 0; i < 3; i++) addSpawn(c - 3 + i * 3, r + (i == 1 ? -3 : 3), campEnemy(z, i), i == 1 && hash2(c, r, S + 80) < 0.35);
        });
        each("ruin", 4, [&](int c, int r, const ZoneDef& z) {
            clearGround(c, r, 5);
            for (int dr = -3; dr <= 3; dr++)
                for (int dc = -4; dc <= 4; dc++) {
                    const bool edge = std::abs(dr) == 3 || std::abs(dc) == 4;
                    const bool gap = (dr == 3 && std::abs(dc) <= 1) || hash2(c + dc, r + dr, S + 81) < 0.22;
                    if (edge && !gap) m.set(c + dc, r + dr, WALL);
                    else if (!edge) m.set(c + dc, r + dr, PLANKS);
                }
            addChest(c, r - 2, stash(z, 1), z.levelMin + 8);
            for (int k = 0; k < 3; k++) {
                Object pot;
                pot.kind = ObjKind::Pot;
                pot.c = c - 3 + k * 3;
                pot.r = r - 2 + (k % 2) * 3;
                pot.solid = true;
                pot.sprite = k % 2 ? "pot2" : "pot";
                if (areaFree(pot.c, pot.r, 1, 1)) m.addObject(std::move(pot));
            }
            placeProp("pillar", c - 2, r + 1);
            placeProp("pillar", c + 2, r + 1);
            for (int i = 0; i < 3; i++) addSpawn(c - 2 + i * 2, r + (i % 2), campEnemy(z, i + 1), i == 0);
        });
        each("graveyard", 3, [&](int c, int r, const ZoneDef& z) {
            clearGround(c, r, 4);
            for (int j = 0; j < 2; j++)
                for (int i = 0; i < 4; i++) {
                    const int gc = c - 3 + i * 2, gr = r - 1 + j * 3;
                    if (hash2(gc, gr, S + 82) < 0.8) placeProp("grave", gc, gr);
                    else placeProp(hash2(gc, gr, S + 83) < 0.5 ? "bones" : "skull", gc, gr);
                }
            if (areaFree(c + 4, r, 1, 1)) addChest(c + 4, r, stash(z, 0), z.levelMin);
            placeProp("skull", c, r + 1);
            addSpawn(c - 1, r + 4, campEnemy(z, 0));
            addSpawn(c + 2, r + 4, campEnemy(z, 2));
        });
        each("cache", 1, [&](int c, int r, const ZoneDef& z) {
            clearGround(c, r, 1);
            addChest(c, r, stash(z, 0), z.levelMin, hash2(c, r, S + 84) < 0.08 && z.levelMin >= 25);
        });
        each("stones", 4, [&](int c, int r, const ZoneDef& z) {
            clearGround(c, r, 4);
            for (int k = 0; k < 8; k++) {
                if (k == 2) continue;   // leave the south side open: a stone there would hide the chest
                const double a = k / 8.0 * PI * 2;
                placeProp("menhir", c + static_cast<int>(std::lround(std::cos(a) * 3.4)), r + static_cast<int>(std::lround(std::sin(a) * 2.6)));
            }
            addChest(c, r, stash(z, 1), z.levelMin + 16);
            m.set(c, r + 1, FLOWERS);
            addSpawn(c, r + 5, z.spawns.front().first, true);
        });
        each("pond", 3, [&](int c, int r, const ZoneDef&) {
            for (int dr = -2; dr <= 2; dr++)
                for (int dc = -3; dc <= 3; dc++)
                    if (dc * dc / 9.0 + dr * dr / 4.0 <= 1.0 + hash2(c + dc, r + dr, S + 85) * 0.3) m.set(c + dc, r + dr, WATER);
            for (int k = 0; k < 4; k++) {
                const int nc = c + DC4[k] * 4, nr = r + DR4[k] * 3;
                if (isGround(m.get(nc, nr)) && !m.objectAt(nc, nr)) addNode("herb", nc, nr);
            }
        });
    }

    // ── 7. resources ──
    void nodes() {
        auto covered = [&](int c, int r) {
            for (int d = 1; d <= 2; d++) {
                const uint8_t t = m.get(c, r + d);
                if (t == TREE || t == ROCK) return true;
            }
            return false;
        };
        int placed = 0;
        for (int tries = 0; tries < 60000 && placed < 520; tries++) {
            const int c = 3 + rnd.range(W - 6), r = 3 + rnd.range(H - 6);
            if (!isGround(m.get(c, r)) || m.objectAt(c, r) || m.block[m.idx(c, r)] || covered(c, r) || townAt(db, c, r, 2, realm)) continue;
            const ZoneDef& z = zone(c, r);
            double total = 0;
            for (const auto& [id, w] : z.nodes) total += w;
            double roll = rnd.next() * total;
            std::string res = z.nodes.front().first;
            for (const auto& [id, w] : z.nodes) {
                if (roll < w) {
                    res = id;
                    break;
                }
                roll -= w;
            }
            addNode(res, c, r);
            placed++;
        }
    }

    // ── 8. make sure nothing important is walled in, then place monsters ──
    // ── 8b. the ways to other maps: a carriage, a bone portal, a ferry, a ritual circle, a lift ──
    void travels() {
        auto place = [&](TilePos at, const std::string& dest, const std::string& kind, const std::string& needs, const std::string& label) {
            for (int rad = 0; rad < 8; rad++)
                for (int dr = -rad; dr <= rad; dr++)
                    for (int dc = -rad; dc <= rad; dc++) {
                        const int c = at.c + dc, r = at.r + dr;
                        if (!m.tileFree(c, r, false) || !m.tileFree(c, r + 1, false) || m.objectAt(c, r)) continue;
                        Object o;
                        o.kind = ObjKind::Travel;
                        o.c = c;
                        o.r = r;
                        o.solid = true;
                        o.id = dest;
                        o.sprite = kind;
                        o.res = needs;
                        o.text = label;
                        m.addObject(std::move(o));
                        return TilePos{c, r};
                    }
            return at;
        };
        for (const auto& r : db.realms) {
            if (r.travelFrom != realm) continue;
            for (const auto& t : towns)
                if (t.id == r.travelTown) {
                    const TilePos at = place({t.at.c + t.radius - 3, t.at.r + 3}, r.id, r.travelKind, r.needs, r.label);
                    ensureReach(m, m.start, at.c, at.r + 1);
                }
        }
        if (rd) {   // the way home stands where you arrive
            m.start = rd->arrive;
            disk(rd->arrive.c, rd->arrive.r, 4.5, GRASS);
            place({rd->arrive.c, rd->arrive.r - 2}, rd->travelFrom, rd->travelKind, "", "Return");
        }
    }

    void connectivity() {
        const TilePos from = m.start;
        m.set(from.c, from.r, m.get(from.c, from.r) == PLAZA ? PLAZA : GRASS);
        std::vector<uint8_t> mask = reachMask(m, from.c, from.r);
        for (const auto& t : towns) ensureReach(m, from, t.at.c, t.at.r + 3, &mask);
        for (const auto& d : dungeons)
            if (d.under.empty()) ensureReach(m, from, d.at.c, d.at.r + 1, &mask);
        // high ground: every stretch of it (woods on a plateau can wall pockets off) gets a way in
        for (const auto& p : highGround) ensureReach(m, from, p.c, p.r, &mask);
        for (int r = 2; r < H - 2; r += 3)
            for (int c = 2; c < W - 2; c += 3)
                if (m.heightAt(c, r) > 0 && m.tileFree(c, r, false) && !mask[m.idx(c, r)]) ensureReach(m, from, c, r, &mask);
        const size_t n = m.objects.size();
        for (size_t i = 0; i < n; i++) {
            const ObjKind k = m.objects[i].kind;
            if (k == ObjKind::Chest || k == ObjKind::Shrine) ensureReach(m, from, m.objects[i].c, m.objects[i].r, &mask);
        }
        m.reach = mask;
    }

    // ── 9b. Regions walled off from each other (the Diablo way): a ridge of rock along every border, and an iron
    // gate where a road crosses into a region that is still closed. The gate remembers which boss opens it.
    void borders() {
        auto zi = [&](int c, int r) { return m.zone[m.idx(c, r)]; };
        auto gateOf = [&](int a, int b) {   // the later of the two regions' gates
            const std::string& ga = db.zones[a].gate;
            const std::string& gb = db.zones[b].gate;
            if (ga.empty()) return gb;
            if (gb.empty()) return ga;
            const DungeonDef* da = nullptr;
            const DungeonDef* dbb = nullptr;
            for (const auto& d : dungeons) {
                if (d.id == ga) da = &d;
                if (d.id == gb) dbb = &d;
            }
            return da && dbb && da->level >= dbb->level ? ga : gb;
        };
        std::vector<uint8_t> band(static_cast<size_t>(W) * H, 0);
        std::vector<std::string> need(static_cast<size_t>(W) * H);
        for (int r = 1; r < H - 1; r++)
            for (int c = 1; c < W - 1; c++)
                for (int dr = -1; dr <= 1; dr++)
                    for (int dc = -1; dc <= 1; dc++) {
                        const int a = zi(c, r), b = zi(c + dc, r + dr);
                        if (a == b) continue;
                        const std::string g = gateOf(a, b);
                        if (g.empty()) continue;   // two open regions: no wall between them
                        band[m.idx(c, r)] = 1;
                        need[m.idx(c, r)] = g;
                    }
        // roads through the band become gates (one visible portcullis per crossing, the rest of it barred unseen)
        std::vector<uint8_t> seen(band.size(), 0);
        for (int r = 1; r < H - 1; r++)
            for (int c = 1; c < W - 1; c++) {
                const int i = m.idx(c, r);
                if (!band[i] || seen[i]) continue;
                const uint8_t t = m.get(c, r);
                if (t != PATH && t != BRIDGE && t != COBBLE) continue;
                std::vector<TilePos> crossing;
                std::vector<int> stack{i};
                seen[i] = 1;
                while (!stack.empty()) {
                    const int k = stack.back();
                    stack.pop_back();
                    crossing.push_back({k % W, k / W});
                    for (int d = 0; d < 4; d++) {
                        const int nc = k % W + DC4[d], nr = k / W + DR4[d];
                        if (!m.inside(nc, nr)) continue;
                        const int ni = m.idx(nc, nr);
                        const uint8_t nt = m.get(nc, nr);
                        if (seen[ni] || !band[ni] || (nt != PATH && nt != BRIDGE && nt != COBBLE)) continue;
                        seen[ni] = 1;
                        stack.push_back(ni);
                    }
                }
                double sc = 0, sr = 0;
                for (const auto& p : crossing) sc += p.c, sr += p.r;
                sc /= crossing.size();
                sr /= crossing.size();
                size_t best = 0;
                for (size_t k = 1; k < crossing.size(); k++)
                    if (std::hypot(crossing[k].c - sc, crossing[k].r - sr) < std::hypot(crossing[best].c - sc, crossing[best].r - sr)) best = k;
                for (size_t k = 0; k < crossing.size(); k++) {
                    Object g;
                    g.kind = ObjKind::Gate;
                    g.c = crossing[k].c;
                    g.r = crossing[k].r;
                    g.solid = true;
                    g.id = "zone_gate";
                    g.res = need[m.idx(crossing[k].c, crossing[k].r)];
                    g.text = k == best ? "" : "hidden";
                    m.addObject(std::move(g));
                }
            }
        // everything else in the band turns to rock and is barred for good (no tunnelling through it)
        for (int r = 1; r < H - 1; r++)
            for (int c = 1; c < W - 1; c++) {
                const int i = m.idx(c, r);
                if (!band[i] || m.block[i]) continue;
                const uint8_t t = m.get(c, r);
                if (t == WATER || t == STAIRS_DOWN || t == PATH || t == BRIDGE || t == COBBLE || t == PLAZA) continue;
                if (m.objectAt(c, r)) continue;
                m.set(c, r, ROCK);
                m.block[i] = 1;
            }
        // Wherever a wall cut something off (a region no road crosses into), dig a gated pass through it
        auto gatesOpen = [&](bool open) {
            for (auto& o : m.objects)
                if (o.kind == ObjKind::Gate && o.id == "zone_gate") m.setBlock(o, !open);
        };
        gatesOpen(true);
        std::vector<uint8_t> mask = reachMask(m, m.start.c, m.start.r);
        std::vector<TilePos> targets;
        for (const auto& t : towns) targets.push_back({t.at.c, t.at.r + 3});
        for (const auto& d : dungeons)
            if (d.under.empty()) targets.push_back({d.at.c, d.at.r + 1});
        for (const auto& o : m.objects)
            if (o.kind == ObjKind::Chest || o.kind == ObjKind::Shrine) targets.push_back({o.c, o.r});
        for (const auto& p : highGround) targets.push_back(p);
        auto near = [&](TilePos p) {
            for (int dr = -1; dr <= 1; dr++)
                for (int dc = -1; dc <= 1; dc++)
                    if (m.inside(p.c + dc, p.r + dr) && mask[m.idx(p.c + dc, p.r + dr)]) return true;
            return false;
        };
        for (const auto& goal : targets) {
            if (near(goal)) continue;
            const auto path = astar(m, m.start, goal, [&](int nc, int nr) -> double {
                if (nc == goal.c && nr == goal.r) return 1;
                const int i = m.idx(nc, nr);
                if (band[i]) return m.get(nc, nr) == WATER ? -1 : 12;
                if (m.block[i]) return -1;
                const uint8_t t = m.get(nc, nr);
                if (walkable(t)) return 1;
                return t == TREE ? 3 : t == ROCK ? 4 : t == WATER ? 5 : t == CLIFF ? 6 : -1;
            });
            bool first = true;
            for (const auto& p : path) {
                if (p.c == goal.c && p.r == goal.r) continue;
                const int i = m.idx(p.c, p.r);
                const uint8_t t = m.get(p.c, p.r);
                if (band[i]) {
                    m.set(p.c, p.r, PATH);
                    m.block[i] = 0;
                    if (!m.objectAt(p.c, p.r)) {
                        Object g;
                        g.kind = ObjKind::Gate;
                        g.c = p.c;
                        g.r = p.r;
                        g.id = "zone_gate";
                        g.res = need[i];
                        g.text = first ? "" : "hidden";
                        first = false;
                        m.objects.push_back(std::move(g));   // open for now; shut below with the rest
                    }
                } else if (t == TREE || t == ROCK) m.set(p.c, p.r, GRASS);
                else if (t == WATER) m.set(p.c, p.r, BRIDGE);
                else if (t == CLIFF) m.set(p.c, p.r, RAMP);
            }
            if (!path.empty()) mask = reachMask(m, m.start.c, m.start.r);
        }
        for (auto& o : m.objects)
            if (o.kind == ObjKind::Gate && o.id == "zone_gate") o.solid = true;
        gatesOpen(false);
        m.reach = mask;   // where monsters may live: everything reachable once the gates are open
    }

    void spawns() {
        std::vector<int> area(db.zones.size(), 0);
        for (int r = 0; r < H; r++)
            for (int c = 0; c < W; c++)
                if (m.reach[m.idx(c, r)]) area[m.zone[m.idx(c, r)]]++;
        for (const auto& z : db.zones) {
            const int target = static_cast<int>(area[z.index] / 1000.0 * z.density);
            double total = 0;
            for (const auto& [id, w] : z.spawns) total += w;
            int placed = 0;
            for (int tries = 0; tries < target * 400 && placed < target; tries++) {
                const int c = 3 + rnd.range(W - 6), r = 3 + rnd.range(H - 6);
                if (m.zone[m.idx(c, r)] != z.index || !m.reach[m.idx(c, r)] || !isGround(m.get(c, r)) || m.block[m.idx(c, r)]) continue;
                if (townAt(db, c, r, 9, realm)) continue;
                double roll = rnd.next() * total;
                std::string id = z.spawns.front().first;
                for (const auto& [eid, w] : z.spawns) {
                    if (roll < w) {
                        id = eid;
                        break;
                    }
                    roll -= w;
                }
                addSpawn(c, r, id);
                placed++;
            }
        }
        // Spawn points written by camps and ruins may sit on a blocked tile: nudge them onto open ground
        for (auto& s : m.spawns) {
            if (m.tileFree(s.c, s.r, false)) continue;
            for (int rad = 1; rad <= 3; rad++)
                for (int k = 0; k < 4; k++)
                    if (m.tileFree(s.c + DC4[k] * rad, s.r + DR4[k] * rad, false)) {
                        s.c += DC4[k] * rad;
                        s.r += DR4[k] * rad;
                        rad = 99;
                        break;
                    }
        }
    }

    void whirlpool() {
        int best = 0;
        for (int r = 4; r < H - 14; r++)
            for (int c = 4; c < W - 4; c++) {
                int n = 0;
                for (int dr = -2; dr <= 2; dr++)
                    for (int dc = -3; dc <= 3; dc++)
                        if (m.get(c + dc, r + dr) == WATER) n++;
                if (n > best) {
                    best = n;
                    m.whirl = {c, r};
                }
            }
        m.hasWhirl = best >= 33;
    }

    Map run() {
        terrain();
        for (const auto& t : towns) buildTown(t);
        for (const auto& d : dungeons)
            if (d.under.empty()) buildEntrance(d);
        dressing();
        roads();
        townSigns();
        pois();
        plateaus();
        nodes();
        travels();
        connectivity();
        borders();
        spawns();
        whirlpool();
        return std::move(m);
    }
};

Object chest(const std::string& id, int c, int r, std::vector<Item> loot, int level, bool mimic = false) {
    Object o;
    o.kind = ObjKind::Chest;
    o.id = id;
    o.c = c;
    o.r = r;
    o.solid = true;
    o.loot = std::move(loot);
    o.lootLevel = level;
    o.mimic = mimic;
    return o;
}

Object simple(ObjKind kind, int c, int r, bool solid = false) {
    Object o;
    o.kind = kind;
    o.c = c;
    o.r = r;
    o.solid = solid;
    return o;
}

void carve(Map& m, int x, int y, int w, int h) {
    for (int r = y; r < y + h; r++)
        for (int c = x; c < x + w; c++)
            if (c > 0 && r > 0 && c < m.w - 1 && r < m.h - 1) m.set(c, r, FLOOR);
}

// The hand-built Hollow Crypt: entry, hall, two wings, the sealed gate room and the throne room.
Map buildCrypt(const DungeonDef& def) {
    static const Room ENTRY{28, 40, 8, 5}, HALL{25, 28, 14, 8}, WEST{6, 26, 13, 11}, EAST{45, 26, 13, 11}, GATE{23, 14, 18, 9}, BOSS{20, 1, 24, 10};
    Map m = makeMap(def.id, def.w, def.h, WALL);
    m.name = def.name;
    m.dark = true;
    m.dungeon = &def;
    m.rooms = {ENTRY, HALL, WEST, EAST, GATE, BOSS};
    for (const Room& R : m.rooms) carve(m, R.x, R.y, R.w, R.h);
    carve(m, 31, 36, 3, 4);
    carve(m, 19, 31, 6, 3);
    carve(m, 39, 31, 6, 3);
    carve(m, 31, 23, 3, 5);
    carve(m, 31, 11, 3, 3);
    for (int r = 11; r <= 13; r++)
        for (int c = 31; c <= 33; c++) m.set(c, r, DOOR);
    m.objects.push_back(simple(ObjKind::Gate, 30, 11));
    m.set(32, 44, STAIRS_UP);
    m.start = {32, 42};
    static const int PILLARS[12][2] = {{25, 4}, {38, 4}, {25, 8}, {38, 8}, {27, 17}, {36, 17}, {27, 20}, {36, 20}, {10, 30}, {14, 30}, {50, 30}, {53, 33}};
    for (const auto& p : PILLARS) m.set(p[0], p[1], WALL);
    for (const Room& R : m.rooms) {
        const std::vector<int> xs = R.w > 12 ? std::vector<int>{R.x + 2, R.x + R.w / 2, R.x + R.w - 3} : std::vector<int>{R.x + 1, R.x + R.w - 2};
        for (int c : xs)
            if (m.get(c, R.y - 1) == WALL && c != 32) m.objects.push_back(simple(ObjKind::WallTorch, c, R.y - 1));
    }
    m.addObject(chest("d_west", 7, 27, {{"knight_blade", 1}, {"roast", 1}}, def.level));
    m.addObject(chest("d_east", 56, 27, {{"key", 1}, {"potion", 1}, {"arrows", 10}}, def.level));
    m.addObject(chest("d_gate", 24, 15, {{"bandage", 3}, {"torch", 1}}, def.level));
    m.addObject(chest("d_mimic", 56, 35, {}, def.level, true));
    static const int TRAPS[4][2] = {{22, 30}, {41, 30}, {27, 13}, {37, 13}};
    for (const auto& t : TRAPS) {
        Object o = simple(ObjKind::Trap, t[0], t[1]);
        o.t = (t[0] * 7) % 60;
        m.objects.push_back(std::move(o));
    }
    Object knight = simple(ObjKind::Sign, 36, 15, true);
    knight.id = "fallen";
    knight.sprite = "fallen";
    knight.text = "A fallen knight. His journal reads: \"The King's gate is sealed. The key is kept in the east wing.\"";
    m.addObject(std::move(knight));
    m.bossRoom = BOSS;
    m.links.push_back({32, 44, def.realm, def.at.c, def.at.r + 1, Dir::Down});
    m.links.push_back({32, 3, "vaults", -1, -1, Dir::Down});
    m.explored.assign(static_cast<size_t>(m.w) * m.h, 0);
    return m;
}

Map buildGenerated(const Database& db, const DungeonDef& def) {
    const int W = def.w, H = def.h;
    Map m = makeMap(def.id, W, H, WALL);
    m.name = def.name;
    m.dark = true;
    m.dungeon = &def;
    Rng rnd(def.seed);
    std::vector<Room> rooms;
    for (int i = 0; i < 900 && static_cast<int>(rooms.size()) < def.rooms; i++) {
        const int w = 8 + rnd.range(6), h = 6 + rnd.range(4), x = 2 + rnd.range(W - w - 4), y = 2 + rnd.range(H - h - 4);
        bool clash = false;
        for (const Room& R : rooms)
            if (x < R.x + R.w + 4 && x + w + 4 > R.x && y < R.y + R.h + 4 && y + h + 4 > R.y) clash = true;
        if (!clash) rooms.push_back({x, y, w, h});
    }
    // Chain the rooms from the lowest one, always walking to the nearest unvisited room
    std::sort(rooms.begin(), rooms.end(), [](const Room& a, const Room& b) { return (b.y + b.h) < (a.y + a.h); });
    std::vector<Room> chain{rooms.front()};
    rooms.erase(rooms.begin());
    while (!rooms.empty()) {
        const Room& L = chain.back();
        size_t bi = 0;
        double bd = 1e18;
        for (size_t i = 0; i < rooms.size(); i++) {
            const double d = hypot2(rooms[i].x + rooms[i].w / 2.0 - L.x - L.w / 2.0, rooms[i].y + rooms[i].h / 2.0 - L.y - L.h / 2.0);
            if (d < bd) {
                bd = d;
                bi = i;
            }
        }
        chain.push_back(rooms[bi]);
        rooms.erase(rooms.begin() + bi);
    }
    for (const Room& R : chain) carve(m, R.x, R.y, R.w, R.h);
    for (size_t i = 1; i < chain.size(); i++) {
        const TilePos a = chain[i - 1].center(), b = chain[i].center();
        carve(m, std::min(a.c, b.c) - 1, a.r - 1, std::abs(b.c - a.c) + 3, 3);
        carve(m, b.c - 1, std::min(a.r, b.r) - 1, 3, std::abs(b.r - a.r) + 3);
    }
    auto inRoom = [&](int c, int r) {
        for (const Room& R : chain)
            if (c >= R.x && c < R.x + R.w && r >= R.y && r < R.y + R.h) return true;
        return false;
    };
    for (size_t i = 1; i + 1 < chain.size(); i++) {
        const Room& R = chain[i];
        if (R.w >= 11 && R.h >= 8) {
            m.set(R.x + 2, R.y + 2, WALL);
            m.set(R.x + R.w - 3, R.y + 2, WALL);
            m.set(R.x + 2, R.y + R.h - 3, WALL);
            m.set(R.x + R.w - 3, R.y + R.h - 3, WALL);
        }
    }
    const TilePos s = chain.front().center();
    m.set(s.c, s.r, STAIRS_UP);
    m.start = {s.c, s.r + 1};
    if (def.under.empty()) m.links.push_back({s.c, s.r, def.realm, def.at.c, def.at.r + 1, Dir::Down});
    else m.links.push_back({s.c, s.r, def.under, 32, 5, Dir::Down});

    for (const Room& R : chain)
        for (int c : {R.x + 1, R.x + R.w - 2})
            if (m.get(c, R.y - 1) == WALL && m.get(c, R.y) == FLOOR) m.objects.push_back(simple(ObjKind::WallTorch, c, R.y - 1));
    int traps = 0;
    for (int r = 2; r < H - 2 && traps < def.traps; r++)
        for (int c = 2; c < W - 2 && traps < def.traps; c++)
            if (m.get(c, r) == WALL && m.get(c, r + 1) == FLOOR && m.get(c, r + 2) == FLOOR && !inRoom(c, r + 1) && hash2(c, r, 61) < 0.08 && !m.objectAt(c, r)) {
                Object o = simple(ObjKind::Trap, c, r);
                o.t = (c * 7) % 60;
                m.objects.push_back(std::move(o));
                traps++;
            }

    const Room& last = chain.back();
    m.bossRoom = last;
    std::vector<Item> reward;
    for (const auto& [id, q] : def.reward) reward.push_back({id, q});
    m.addObject(chest(def.id + "_reward", last.center().c, last.y, std::move(reward), def.level + 8));
    auto chestIn = [&](size_t i, const char* tag, std::vector<Item> loot, bool mimic) {
        if (i >= chain.size() - 1) return;
        const Room& R = chain[i];
        const int c = R.center().c + 2;
        if (m.get(c, R.y) == FLOOR && !m.objectAt(c, R.y)) m.addObject(chest(def.id + "_" + tag, c, R.y, std::move(loot), def.level, mimic));
    };
    chestIn(2, "gold", {{"gold", 6 + def.level / 8 * 3}, {"roast", 1}}, false);
    chestIn(4, "mimic", {}, def.level >= 25);
    chestIn(5, "supplies", {{"arrows", 12}, {"bandage", 2}}, false);
    chestIn(7, "cache", {{"gold", 10 + def.level / 8 * 3}, {"potion", 1}}, false);
    // Breakable pots in room corners, idols flanking the last room
    for (size_t i = 1; i < chain.size(); i++) {
        const Room& R = chain[i];
        const int spots[4][2] = {{R.x, R.y}, {R.x + R.w - 1, R.y}, {R.x, R.y + R.h - 1}, {R.x + R.w - 1, R.y + R.h - 1}};
        for (int k = 0; k < 4; k++) {
            const int c = spots[k][0], r = spots[k][1];
            if (hash2(c, r, static_cast<int>(def.seed)) > 0.55 || m.get(c, r) != FLOOR || m.objectAt(c, r)) continue;
            // a corridor may run through this corner: only keep pots that leave a way around
            if (m.get(c, r - 1) == FLOOR || (m.get(c - 1, r) == FLOOR && m.get(c + 1, r) == FLOOR)) continue;
            Object pot = simple(ObjKind::Pot, c, r, true);
            pot.sprite = k % 2 ? "pot2" : "pot";
            m.addObject(std::move(pot));
        }
    }
    m.rooms = chain;
    m.explored.assign(static_cast<size_t>(m.w) * m.h, 0);
    (void)db;
    return m;
}

}   // namespace

Map generateOverworld(const Database& db) { return OverworldGen(db, "over").run(); }
Map generateRealm(const Database& db, const std::string& id) { return OverworldGen(db, id).run(); }

Map generateDungeon(const Database& db, const DungeonDef& def, const Map&) { return def.kind == "crypt" ? buildCrypt(def) : buildGenerated(db, def); }

}   // namespace hc

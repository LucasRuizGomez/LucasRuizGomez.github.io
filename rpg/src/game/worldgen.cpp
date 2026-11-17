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

const TownDef* townAt(const Database& db, int c, int r, int margin) {
    for (const auto& t : db.towns)
        if (hypot2(c - t.at.c, r - t.at.r) <= t.radius + margin) return &t;
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

    explicit OverworldGen(const Database& d)
        : db(d), m(makeMap("over", d.tuning.worldW, d.tuning.worldH, GRASS)), rnd(d.tuning.worldSeed), W(d.tuning.worldW), H(d.tuning.worldH),
          S(static_cast<int>(d.tuning.worldSeed)) {
        m.name = "The Realm";
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
                if (r > H - 7 - fbm(c / 12.0, 0.5, S + 4) * 9) t = WATER;   // the southern sea
                m.set(c, r, t);
            }
        river(100, 40, H - 1, 90, 112);
        river(184, 0, 100, 174, 192);
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

        // Houses around the square, each with a cobbled path from its door
        static const int HOUSE[5][2] = {{-8, -4}, {8, -4}, {-9, 6}, {9, 6}, {0, -8}};
        for (int i = 0; i < std::min(t.houses, 5); i++) {
            const int hc = tc + HOUSE[i][0], hr = tr + HOUSE[i][1];
            for (int r = hr - 5; r <= hr + 1; r++)
                for (int c = hc - 3; c <= hc + 3; c++)
                    if (inner(c, r) && m.get(c, r) != PLAZA) m.set(c, r, GRASS);
            if (!placeProp(i % 2 ? "house_b" : "house_a", hc, hr)) continue;
            int c = hc, r = hr + 1;
            for (int guard = 0; guard < 40 && (std::abs(c - tc) > pw || std::abs(r - tr) > ph); guard++) {
                if (m.get(c, r) == GRASS) m.set(c, r, COBBLE);
                if (std::abs(c - tc) > pw && !m.block[m.idx(c + sign(tc - c), r)]) c += sign(tc - c);
                else if (r != tr) r += sign(tr - r);
                else c += sign(tc - c);
            }
            placeProp(i % 2 ? "barrel" : "crate", hc + 3, hr);
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
        for (const auto& t : db.towns) {
            std::string text = t.name + ".";
            std::vector<std::pair<double, const TownDef*>> others;
            for (const auto& o : db.towns)
                if (o.id != t.id) others.push_back({hypot2(o.at.c - t.at.c, o.at.r - t.at.r), &o});
            std::sort(others.begin(), others.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            for (size_t i = 0; i < std::min<size_t>(3, others.size()); i++)
                text += std::string(" ") + others[i].second->name + ": " + compass(others[i].second->at.c - t.at.c, others[i].second->at.r - t.at.r) + ".";
            for (const auto& d : db.dungeons)
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
                if (m.get(c, r) != GRASS || townAt(db, c, r, 0)) continue;
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
        for (const auto& t : db.towns) nodes.push_back({{t.at.c, t.at.r + (t.houses >= 3 ? 4 : 3)}, true});
        const size_t towns = nodes.size();
        for (const auto& d : db.dungeons)
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
                else m.set(p.c, p.r, townAt(db, p.c, p.r, -1) ? COBBLE : PATH);
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

    // ── 6. points of interest ──
    bool poiSpot(TilePos& out, int clear) {
        for (int tries = 0; tries < 400; tries++) {
            const int c = 6 + rnd.range(W - 12), r = 6 + rnd.range(H - 18);
            if (townAt(db, c, r, 10)) continue;
            bool ok = true;
            for (const auto& d : db.dungeons)
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
        loot.push_back({"gold", 3 + z.levelMin * 2 + rnd.range(6)});
        static const char* extra[5] = {"bandage", "arrows", "roast", "herb", "wood"};
        const char* e = extra[rnd.range(5)];
        loot.push_back({e, std::string(e) == "arrows" ? 8 : 2});
        if (kind > 0 && z.levelMin >= 4) loot.push_back({"potion", 1});
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
            addChest(c, r - 2, stash(z, 1), z.levelMin + 1);
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
            addChest(c, r, stash(z, 0), z.levelMin, hash2(c, r, S + 84) < 0.08 && z.levelMin >= 4);
        });
        each("stones", 4, [&](int c, int r, const ZoneDef& z) {
            clearGround(c, r, 4);
            for (int k = 0; k < 8; k++) {
                const double a = k / 8.0 * PI * 2;
                placeProp("menhir", c + static_cast<int>(std::lround(std::cos(a) * 3.4)), r + static_cast<int>(std::lround(std::sin(a) * 2.6)));
            }
            addChest(c, r, stash(z, 1), z.levelMin + 2);
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
            if (!isGround(m.get(c, r)) || m.objectAt(c, r) || m.block[m.idx(c, r)] || covered(c, r) || townAt(db, c, r, 2)) continue;
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
    void connectivity() {
        const TilePos from = m.start;
        m.set(from.c, from.r, m.get(from.c, from.r) == PLAZA ? PLAZA : GRASS);
        std::vector<uint8_t> mask = reachMask(m, from.c, from.r);
        for (const auto& t : db.towns) ensureReach(m, from, t.at.c, t.at.r + 3, &mask);
        for (const auto& d : db.dungeons)
            if (d.under.empty()) ensureReach(m, from, d.at.c, d.at.r + 1, &mask);
        const size_t n = m.objects.size();
        for (size_t i = 0; i < n; i++) {
            const ObjKind k = m.objects[i].kind;
            if (k == ObjKind::Chest || k == ObjKind::Shrine) ensureReach(m, from, m.objects[i].c, m.objects[i].r, &mask);
        }
        m.reach = mask;
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
                if (townAt(db, c, r, 9)) continue;
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
        for (const auto& t : db.towns) buildTown(t);
        for (const auto& d : db.dungeons)
            if (d.under.empty()) buildEntrance(d);
        dressing();
        roads();
        townSigns();
        pois();
        nodes();
        connectivity();
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
    m.links.push_back({32, 44, "over", def.at.c, def.at.r + 1, Dir::Down});
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
    if (def.under.empty()) m.links.push_back({s.c, s.r, "over", def.at.c, def.at.r + 1, Dir::Down});
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
    m.addObject(chest(def.id + "_reward", last.center().c, last.y, std::move(reward), def.level + 1));
    auto chestIn = [&](size_t i, const char* tag, std::vector<Item> loot, bool mimic) {
        if (i >= chain.size() - 1) return;
        const Room& R = chain[i];
        const int c = R.center().c + 2;
        if (m.get(c, R.y) == FLOOR && !m.objectAt(c, R.y)) m.addObject(chest(def.id + "_" + tag, c, R.y, std::move(loot), def.level, mimic));
    };
    chestIn(2, "gold", {{"gold", 6 + def.level * 3}, {"roast", 1}}, false);
    chestIn(4, "mimic", {}, def.level >= 4);
    chestIn(5, "supplies", {{"arrows", 12}, {"bandage", 2}}, false);
    chestIn(7, "cache", {{"gold", 10 + def.level * 3}, {"potion", 1}}, false);
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

Map generateOverworld(const Database& db) { return OverworldGen(db).run(); }

Map generateDungeon(const Database& db, const DungeonDef& def, const Map&) { return def.kind == "crypt" ? buildCrypt(def) : buildGenerated(db, def); }

}   // namespace hc

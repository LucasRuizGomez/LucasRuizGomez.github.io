// Using the world: what the hero is facing, chests, trees, rocks, pots, shrines, doors,
// and the slow background systems (regrowth, water ripples, nightfall, exploration).
#include "game/sim.h"

#include <cmath>

namespace hc {

namespace {
const int RIPPLE_FRAMES = 23, SPLASH_FRAMES = 16;   // frames in assets/generated/water_*.png
const int DC4[4] = {0, -1, 1, 0}, DR4[4] = {-1, 0, 0, 1};
}   // namespace

void Sim::updateFocus(Map& m) {
    if (flying()) {
        focus = Focus();
        return;
    }
    const Vec2 v = dirVec(P.dir);
    const double px = P.x + v.x * 9, py = P.y - 4 + v.y * 9;
    int best = -1;
    double bd = 20;
    for (size_t i = 0; i < m.objects.size(); i++) {
        const Object& o = m.objects[i];
        if (o.gone) continue;
        switch (o.kind) {
            case ObjKind::WallTorch:
            case ObjKind::Gate:
            case ObjKind::Prop:
            case ObjKind::Entrance:
            case ObjKind::Stump:
            case ObjKind::Trap:
            case ObjKind::Pot: continue;
            case ObjKind::Node:
                if (o.left <= 0) continue;
                break;
            case ObjKind::Chest:
                if (o.opened) continue;
                break;
            case ObjKind::Shrine:
                if (tickCount < o.at) continue;
                break;
            default: break;
        }
        const double ox = o.c * TILE + 8, oy = o.r * TILE + 10;
        if (hypot2(ox - P.x, oy - (P.y - 4)) > 24) continue;   // must be right next to you
        const double d = hypot2(ox - px, oy - py);              // prefer the one you face
        if (d < bd) {
            bd = d;
            best = static_cast<int>(i);
        }
    }
    focus = Focus();
    if (best >= 0) {
        const Object& o = m.objects[best];
        std::string label;
        switch (o.kind) {
            case ObjKind::Chest: label = "Open"; break;
            case ObjKind::Campfire: label = isNight() && m.outdoor ? "Sleep" : "Rest"; break;
            case ObjKind::Npc: {
                const NpcDef* n = db.npc(o.id);
                label = n && !n->shop.empty() && npcMarker(o.id) != 1 && npcMarker(o.id) != 3 ? "Trade" : "Talk";
                break;
            }
            case ObjKind::Node: label = o.res == "stone" ? "Mine" : o.res == "iron" ? "Mine iron" : o.res == "coal" ? "Mine coal" : "Pick herb"; break;
            case ObjKind::Shrine: label = "Awaken"; break;
            case ObjKind::Waypoint: label = "Travel"; break;
            case ObjKind::Sign: label = o.id == "fallen" ? "Search" : "Read"; break;
            default: break;
        }
        focus = {Focus::Kind::Object, best, o.c, o.r, label, o.c * TILE + 8.0, o.r * TILE + 0.0};
        return;
    }
    const int tr = ifloor((P.y - 3 + v.y * 11) / TILE), tc = ifloor((P.x + v.x * 10) / TILE);
    const uint8_t t = m.get(tc, tr);
    if (m.outdoor) {
        if (t == TREE) {
            focus = {Focus::Kind::Tree, -1, tc, tr, "Chop", tc * TILE + 8.0, tr * TILE + 0.0};
            return;
        }
        // Not facing a tree: any tree touching you can still be chopped
        const int pc = ifloor(P.x / TILE), pr = ifloor((P.y - 3) / TILE);
        for (int k = 0; k < 4; k++) {
            const int c = pc + DC4[k], r = pr + DR4[k];
            if (m.get(c, r) != TREE) continue;
            const double nx = std::clamp(P.x, c * TILE + 0.0, c * TILE + 16.0), ny = std::clamp(P.y - 3, r * TILE + 0.0, r * TILE + 16.0);
            if (hypot2(nx - P.x, ny - (P.y - 3)) < 8) {
                focus = {Focus::Kind::Tree, -1, c, r, "Chop", c * TILE + 8.0, r * TILE + 0.0};
                return;
            }
        }
    }
    if (t == DOOR) focus = {Focus::Kind::Door, -1, tc, tr, hasItem("key") ? "Unlock" : "Locked", tc * TILE + 8.0, tr * TILE + 0.0};
}

void Sim::interact(Map& m) {
    if (focus.kind == Focus::Kind::None || P.actCd > 0) return;
    P.actCd = 14;
    if (focus.kind == Focus::Kind::Tree) {
        chopTree(m, focus.c, focus.r);
        return;
    }
    if (focus.kind == Focus::Kind::Door) {
        if (!hasItem("key")) {
            toast("Locked. The key must be somewhere in the crypt.");
            play("deny");
            return;
        }
        removeItem("key", 1);
        openDoor(m, false);
        toast("The gate grinds open.", Color(242, 210, 122));
        play("door");
        shake = 6;
        wantSave = true;
        return;
    }
    if (focus.obj < 0 || focus.obj >= static_cast<int>(m.objects.size())) return;
    Object& o = m.objects[focus.obj];
    switch (o.kind) {
        case ObjKind::Chest: openChest(m, o); break;
        case ObjKind::Npc: talkTo(o); break;
        case ObjKind::Node: harvest(m, o); break;
        case ObjKind::Shrine: useShrine(o); break;
        case ObjKind::Campfire:
            // No resting with monsters about: sleeping would heal you and skip the fight
            if (const std::string why = restBlocked(); !why.empty()) {
                toast(why, Color(255, 154, 122));
                play("deny");
                break;
            }
            P.hp = maxHp();
            play("heal");
            burst(o.c * TILE + 8, o.r * TILE + 4, Color(255, 179, 71), 12, 0.8, -0.03);
            if (isNight() && m.outdoor)
                transition([this] {
                    sleepUntilMorning();
                    wantSave = true;
                    toast("You sleep by the fire until morning. Game saved.", Color(123, 224, 138));
                });
            else {
                wantSave = true;
                toast("You rest by the fire. Health restored and game saved. You can cook and forge here.", Color(123, 224, 138));
            }
            break;
        case ObjKind::Waypoint:
            waypoints.insert(o.id);
            mode = Mode::Travel;
            play("uiOpen");
            break;
        case ObjKind::Sign:
            signTitle = o.id == "fallen" ? "Fallen knight" : "Signpost";
            signText = o.text;
            mode = Mode::Sign;
            play("uiOpen");
            break;
        default: break;
    }
}

void Sim::openChest(Map& m, Object& o) {
    if (o.mimic) {   // surprise: the chest bites
        o.opened = true;
        m.setBlock(o, false);
        Enemy e = makeEnemy(db.enemy("mimic"), o.c * TILE + 8, o.r * TILE + 14, false);
        e.wind = 20;
        e.face = P.x < e.x ? -1 : 1;
        m.enemies.push_back(e);
        toast("It's a mimic!", Color(255, 154, 122));
        play("roar");
        shake = 8;
        burst(e.x, e.y - 8, Color(242, 210, 122), 10, 1.2);
        wantSave = true;
        return;
    }
    const bool reward = o.id.size() > 7 && o.id.compare(o.id.size() - 7, 7, "_reward") == 0;
    if (o.openAt < 0) {   // first time: roll the random part of the loot
        for (auto& it : o.loot)
            if (db.item(it.id).isGear() && !it.uid) it = makeGear(it.id, reward ? 1 : 0, 1.5);
        if (reward) o.loot.push_back(randomGear(o.lootLevel, 2));
        else if (rng.chance(0.35)) o.loot.push_back(randomGear(o.lootLevel, 0));
    }
    o.opened = true;
    o.openAt = tickCount;
    play("open");
    burst(o.c * TILE + 8, o.r * TILE + 4, Color(242, 210, 122), 14, 1.1, 0.03);
    std::vector<Item> left;
    for (const auto& it : o.loot) {
        const ItemDef& def = db.item(it.id);
        const std::string name = itemName(it);
        if (addItem(it)) {
            const bool announced = def.isGear() || def.type == ItemType::Bow || def.type == ItemType::Tool || def.type == ItemType::Light || def.type == ItemType::Key;
            if (!announced || (!isEquipped(it) && def.isGear())) toast((def.isGear() ? std::string("Found: ") : "+" + std::to_string(it.qty) + " ") + name, def.isGear() && it.rarity ? db.rarities[it.rarity].glow : Color(236, 230, 216));
        } else left.push_back(it);
    }
    o.loot = left;
    if (!left.empty()) o.opened = false;
    wantSave = true;
}

void Sim::chopTree(Map& m, int c, int r) {
    if (c < 2 || r < 2 || c >= m.w - 2 || r >= m.h - 2) {
        toast("That tree is too deep in the forest to reach.");
        play("deny");
        return;
    }
    TreeHit& h = m.treeHit[m.idx(c, r)];
    h.hits++;
    h.at = tickCount;
    const int n = rng.chance(0.4) ? 2 : 1;
    addItem({"wood", n});
    toast("+" + std::to_string(n) + " Wood");
    play("chop");
    const double x = c * TILE + 8, y = r * TILE;
    burst(x, y - 4, Color(163, 177, 58), 8, 1);
    burst(x, y + 12, Color(125, 83, 50), 4, 0.8);
    if (h.hits >= db.tuning.treeChops) fellTree(m, c, r);
}

void Sim::fellTree(Map& m, int c, int r) {
    m.treeHit.erase(m.idx(c, r));
    m.falling.push_back({c, r, 0, P.x < c * TILE + 8 ? 1 : -1});
    m.change(c, r, GRASS);
    Object stump;
    stump.kind = ObjKind::Stump;
    stump.c = c;
    stump.r = r;
    stump.at = tickCount + db.tuning.stumpTicks;
    m.objects.push_back(stump);
    addItem({"wood", 2});
    toast("Timber! +2 Wood", Color(242, 210, 122));
    shake = std::max(shake, 3.0);
    burst(c * TILE + 8, r * TILE - 8, Color(163, 177, 58), 18, 1.4);
    burst(c * TILE + 8, r * TILE + 12, Color(125, 83, 50), 8, 1);
}

// True when the hero or a monster stands on the tile (regrowth waits so nobody gets stuck)
bool Sim::tileOccupied(const Map& m, int c, int r) const {
    const double x0 = c * TILE, y0 = r * TILE;
    auto inside = [&](double x, double y, double hw, double hh) { return x + hw > x0 && x - hw < x0 + TILE && y + hh > y0 && y - hh < y0 + TILE; };
    if (inside(P.x, P.y, 4, 3)) return true;
    for (const auto& e : m.enemies)
        if (e.alive && !e.def->fly && inside(e.x, e.y, e.hw, e.hh)) return true;
    return false;
}

void Sim::harvest(Map& m, Object& o) {
    if (o.left <= 0) {
        toast("Nothing left here yet. Come back later.");
        play("deny");
        return;
    }
    if ((o.res == "iron" || o.res == "coal") && !hasItem("pickaxe")) {
        toast("You need a pickaxe for this. Craft one from wood and stone.");
        play("deny");
        return;
    }
    const int n = o.res == "herb" && rng.chance(0.3) ? 2 : 1;
    if (!addItem({o.res, n})) return;
    o.left--;
    o.hitAt = tickCount;
    if (o.left <= 0) {
        o.respawn = tickCount + 60 * (o.res == "herb" ? 45 : 70);
        m.setBlock(o, false);   // the rock is gone, you can walk here
        burst(o.c * TILE + 8, o.r * TILE + 10, o.res == "herb" ? Color(224, 80, 60) : Color(107, 101, 120), 12, 1.3);
    }
    toast("+" + std::to_string(n) + " " + db.item(o.res).name);
    play(o.res == "herb" ? "pick" : "mine");
    const Color col = o.res == "stone" ? Color(146, 152, 164) : o.res == "iron" ? Color(224, 138, 72) : o.res == "coal" ? Color(43, 43, 51) : Color(124, 196, 99);
    burst(o.c * TILE + 8, o.r * TILE + 9, col, 9, 1.1);
}

void Sim::breakPot(Map& m, Object& o) {
    if (o.gone) return;
    o.gone = true;
    m.setBlock(o, false);
    brokenPots_.push_back({m.id, {o.c, o.r}});
    play("break");
    const double x = o.c * TILE + 8, y = o.r * TILE + 8;
    burst(x, y, Color(190, 150, 110), 12, 1.3);
    const int level = m.dungeon ? m.dungeon->level : zoneAt(db, m, o.c, o.r).levelMin;
    const double roll = rng.next();
    if (roll < 0.55) dropItem(m, {"gold", 1 + rng.range(2 + level)}, x, y);
    else if (roll < 0.68) dropItem(m, {"bandage", 1}, x, y);
    else if (roll < 0.76) dropItem(m, {"arrows", 3}, x, y);
    else if (roll < 0.8) dropItem(m, randomGear(level, 0), x, y);
}

void Sim::useShrine(Object& o) {
    static const struct {
        const char *id, *name, *text;
    } BLESSINGS[4] = {{"frenzy", "Shrine of Fury", "Your blows hit half again as hard"},
                      {"protection", "Shrine of Stone", "You take half damage"},
                      {"swift", "Shrine of the Wind", "You move a quarter faster"},
                      {"fortune", "Shrine of Fortune", "Monsters drop more gold and finer gear"}};
    const auto& b = BLESSINGS[rng.range(4)];
    std::erase_if(P.buffs, [&](const Buff& x) { return x.id == b.id; });
    P.buffs.push_back({b.id, db.tuning.shrineBuff});
    o.at = tickCount + db.tuning.shrineRecharge;
    showBanner("shrine", b.name, std::string(b.text) + " for a minute", Color(90, 60, 160));
    play("shrine");
    burst(o.c * TILE + 8, o.r * TILE - 4, Color(196, 160, 255), 26, 1.6, -0.02);
    if (shrinesFound.insert(o.id).second) {
        questEvent("shrine", "");
        gainXp(10);
    }
}

void Sim::openDoor(Map& m, bool silent) {
    std::vector<TilePos> cells;
    for (int r = 0; r < m.h; r++)
        for (int c = 0; c < m.w; c++)
            if (m.get(c, r) == DOOR) {
                m.change(c, r, FLOOR);
                cells.push_back({c, r});
            }
    doorOpen = true;
    gateAt = silent ? -999 : tickCount;
    if (!silent)
        for (const auto& p : cells) burst(p.c * TILE + 8, p.r * TILE + 8, Color(107, 101, 120), 8, 1);
}

void Sim::updateNodes(Map& m) {
    for (auto& f : m.falling) f.t++;
    std::erase_if(m.falling, [](const FallingTree& f) { return f.t >= 40; });
    if (tickCount % 30) return;
    bool erased = false;
    for (auto& o : m.objects) {
        if (o.gone) {
            erased = true;
            continue;
        }
        if (o.kind == ObjKind::Node && o.left <= 0 && tickCount >= o.respawn) {
            if (o.solid && tileOccupied(m, o.c, o.r)) {
                o.respawn = tickCount + 60;
                continue;
            }
            o.left = o.max;
            o.grewAt = tickCount;
            if (o.solid) m.setBlock(o, true);
        } else if (o.kind == ObjKind::Stump && tickCount >= o.at) {
            if (tileOccupied(m, o.c, o.r)) {
                o.at = tickCount + 60;
                continue;
            }
            if (!o.stage) {   // sapling
                o.stage = 1;
                o.at = tickCount + db.tuning.saplingTicks;
                m.block[m.idx(o.c, o.r)] = 1;
            } else {   // grown back
                o.gone = true;
                erased = true;
                m.block[m.idx(o.c, o.r)] = 0;
                m.change(o.c, o.r, TREE);
            }
        }
    }
    if (erased) {
        std::erase_if(m.objects, [](const Object& o) { return o.gone; });
        focus = Focus();
    }
}

void Sim::waterFx(Map& m, double x, double y, bool splash) {
    m.wfx.push_back({splash, x, y, 0});
    if (splash && hypot2(x - P.x, y - P.y) < 200) play("splash");
}

// Ripples and the odd jumping fish on visible open water; splashes are added where things fall in
void Sim::updateWater(Map& m) {
    if (!m.outdoor) return;
    for (auto& f : m.wfx) f.t++;
    std::erase_if(m.wfx, [](const WaterFx& f) { return f.t >= (f.splash ? SPLASH_FRAMES : RIPPLE_FRAMES) * 4; });
    if (tickCount % 9 || m.wfx.size() > 10) return;
    for (int i = 0; i < 8; i++) {
        const int c = ifloor((cam.x + rng.next() * viewW) / TILE), r = ifloor((cam.y + rng.next() * viewH) / TILE);
        bool open = m.get(c, r) == WATER;
        for (int k = 0; k < 4 && open; k++) open = m.get(c + DC4[k], r + DR4[k]) == WATER;
        if (!open || (m.hasWhirl && hypot2(c - m.whirl.c, r - m.whirl.r) <= 4)) continue;
        m.wfx.push_back({rng.chance(0.15), c * TILE + 8.0, r * TILE + 10.0, 0});
        break;
    }
}

// At nightfall a pack of predators comes out of the wild, somewhere out of sight
void Sim::updateNight(Map& m) {
    if (!m.outdoor) return;
    const bool night = isNight();
    if (night && !nightPack) {
        nightPack = true;
        if (!townId.empty()) return;
        const ZoneDef& z = zoneAt(db, m, ifloor(P.x / TILE), ifloor(P.y / TILE));
        const EnemyDef* pack = &db.enemy("fox");
        for (const auto& [id, w] : z.spawns)
            if (db.enemy(id).ai == Ai::Chaser) {
                pack = &db.enemy(id);
                break;
            }
        int n = 0;
        for (int i = 0; i < 300 && n < 4; i++) {
            const double a = rng.next() * PI * 2, d = 200 + rng.next() * 80, x = P.x + std::cos(a) * d, y = P.y + std::sin(a) * d;
            const int c = ifloor(x / TILE), r = ifloor(y / TILE);
            if (c < 2 || r < 2 || c >= m.w - 2 || r >= m.h - 2 || !m.reach[m.idx(c, r)] || !m.tileFree(c, r, false) || townAt(db, c, r, 4)) continue;
            Enemy e = makeEnemy(*pack, c * TILE + 8, r * TILE + 12, false);
            e.night = true;
            m.enemies.push_back(e);
            n++;
        }
        if (n) toast("Night falls. Something howls in the dark...", Color(159, 180, 255));
    } else if (!night) nightPack = false;
}

// Fog of war for the minimap and the world map
void Sim::explore(Map& m) {
    if (m.explored.empty()) return;
    const int pc = ifloor(P.x / TILE), pr = ifloor(P.y / TILE), R = m.outdoor ? (flying() ? 18 : 12) : hasItem("torch") ? 7 : 5;
    for (int r = pr - R; r <= pr + R; r++)
        for (int c = pc - R; c <= pc + R; c++) {
            if (!m.inside(c, r) || (c - pc) * (c - pc) + (r - pr) * (r - pr) > R * R) continue;
            uint8_t& e = m.explored[m.idx(c, r)];
            if (!e) {
                e = 1;
                m.exploredDirty = true;
            }
        }
}

}   // namespace hc

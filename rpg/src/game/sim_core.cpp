// Simulation lifecycle, the main tick, camera, clock, levels, spawning and travel.
#include "game/sim.h"

#include <cmath>
#include <cstdio>

namespace hc {

namespace {

// Nearest walkable, unblocked tile to (c, r).
TilePos freeNear(const Map& m, int c, int r) {
    for (int rad = 0; rad < 12; rad++)
        for (int dr = -rad; dr <= rad; dr++)
            for (int dc = -rad; dc <= rad; dc++)
                if (std::max(std::abs(dc), std::abs(dr)) == rad && m.tileFree(c + dc, r + dr, false) && m.get(c + dc, r + dr) != STAIRS_DOWN && m.get(c + dc, r + dr) != STAIRS_UP)
                    return {c + dc, r + dr};
    return {c, r};
}

}   // namespace

Sim::Sim(const Database& database, uint32_t seed) : db(database), rng(seed) {}

void Sim::buildWorld() {
    maps.clear();
    maps.emplace("over", generateOverworld(db));
    const Map& over = maps.at("over");
    for (const auto& d : db.dungeons) maps.emplace(d.id, generateDungeon(db, d, over));
    worldReady = true;
}

// Things that depend on saved progress rather than on the seed.
void Sim::applyWorldFlags() {
    if (auto it = maps.find("crypt"); it != maps.end()) {
        if (doorOpen) openDoor(it->second, true);
        if (bossesDefeated.count("crypt")) it->second.set(32, 3, STAIRS_DOWN);
    }
}

void Sim::newGame(const std::string& heroName) {
    buildWorld();
    P = Player();
    P.name = heroName.empty() ? "Wanderer" : heroName;
    tickCount = 0;
    time = static_cast<int>(db.tuning.dayLength * (db.tuning.startHour - 6) / 24);
    doorOpen = victorySeen = nightPack = false;
    deaths = victoryAt = hitstop = 0;
    gateAt = -999;
    bossesDefeated.clear();
    zonesFound.clear();
    waypoints.clear();
    shrinesFound.clear();
    quests.clear();
    toasts.clear();
    floaters.clear();
    particles.clear();
    banner.reset();
    zoneId.clear();
    townId.clear();
    P.hp = maxHp();
    const Map& over = overworld();
    enterMap("over", over.start.c * TILE + 8, over.start.r * TILE + 10);
    mode = Mode::Play;
    toast("Speak to Warden Aldric, the man with the yellow mark over his head.", Color(242, 210, 122));
    wantSave = true;
}

void Sim::transition(std::function<void()> mid) {
    if (!fade) fade = Fade{0, 22, std::move(mid)};
}

void Sim::enterMap(const std::string& id, double x, double y) {
    mapId = id;
    Map& m = map();
    P.x = x;
    P.y = y;
    P.kbx = P.kby = 0;
    m.proj.clear();
    m.drops.clear();
    m.corpses.clear();
    if (!m.outdoor) {
        dismount(true);
        P.mount.clear();
    }
    // Overworld monsters come and go with their spawn points. Dungeon monsters are placed once and stay
    // dead until the hero goes back outside; only then does every dungeon fill up again.
    if (m.outdoor) {
        for (auto& [mid, other] : maps)
            if (!other.outdoor) other.populated = false;
        spawnEnemies(m);
    } else if (!m.populated) {
        spawnEnemies(m);
        m.populated = true;
    } else
        for (auto& e : m.enemies) {   // the survivors forget the chase
            e.aggroT = 9999;
            e.kbx = e.kby = 0;
        }
    zoneId.clear();
    townId.clear();
    if (!m.outdoor) area = AreaTitle{m.name, m.dungeon ? "Level " + std::to_string(m.dungeon->level) : "", 200};
    updatePlace(m);
    updateCamera(true);
    focus = Focus();
}

Enemy Sim::makeEnemy(const EnemyDef& def, double x, double y, bool allowElite) {
    Enemy e;
    e.def = &def;
    e.uid = nextEnemyUid_++;
    e.x = x;
    e.y = y;
    e.hp = e.maxHp = def.hp;
    e.dmg = def.dmg;
    e.r = def.r;
    e.cy = def.cy;
    e.hw = def.hw;
    e.hh = def.hh;
    e.t = rng.range(60);
    e.aggroT = 9999;
    if (allowElite && !def.isBoss() && !def.passive && !db.elites.empty() && rng.chance(db.tuning.eliteChance)) {
        e.elite = &db.elites[rng.range(static_cast<int>(db.elites.size()))];
        e.hp = e.maxHp = static_cast<int>(std::ceil(def.hp * 2.5));
        e.dmg += e.elite->dmgAdd;
        e.r *= 1.15;
    }
    return e;
}

void Sim::spawnEnemies(Map& m) {
    m.enemies.clear();
    if (m.outdoor) {
        for (auto& s : m.spawns) s.enemyUid = 0;
        return;
    }
    auto at = [&](const std::string& type, int c, int r) { m.enemies.push_back(makeEnemy(db.enemy(type), c * TILE + 8, r * TILE + 12)); };
    const DungeonDef* def = m.dungeon;
    if (!def) return;
    if (def->kind == "crypt") {
        static const struct {
            const char* type;
            int c, r;
        } PLACED[] = {{"guard", 28, 30}, {"guard", 36, 30}, {"archer", 32, 29}, {"guard", 9, 28},  {"guard", 15, 34}, {"guard", 8, 34},   {"spear", 15, 28},
                      {"archer", 48, 29}, {"archer", 55, 35}, {"spear", 49, 35}, {"spear", 55, 30}, {"guard", 51, 31}, {"guard", 26, 18},  {"guard", 38, 19},
                      {"spear", 30, 21},  {"spear", 35, 17},  {"archer", 25, 21}, {"archer", 39, 15}};
        for (const auto& p : PLACED) at(p.type, p.c, p.r);
    } else {
        for (size_t i = 1; i < m.rooms.size(); i++) {
            const Room& R = m.rooms[i];
            const bool last = i + 1 == m.rooms.size(), bossHere = last && !def->boss.empty();
            const int n = bossHere ? 2 : last ? 5 : 2 + static_cast<int>(i % 2) + (def->level >= 6 ? 1 : 0);
            bool eliteLeft = rng.chance(0.3);
            for (int k = 0; k < n && !def->enemies.empty(); k++)
                for (int tries = 0; tries < 30; tries++) {
                    const int c = R.x + 1 + rng.range(std::max(1, R.w - 2)), r = R.y + 2 + rng.range(std::max(1, R.h - 3));
                    if (!m.tileFree(c, r, false)) continue;
                    Enemy e = makeEnemy(db.enemy(def->enemies[rng.range(static_cast<int>(def->enemies.size()))]), c * TILE + 8, r * TILE + 12, false);
                    if (eliteLeft && !db.elites.empty()) {
                        eliteLeft = false;
                        e.elite = &db.elites[rng.range(static_cast<int>(db.elites.size()))];
                        e.hp = e.maxHp = static_cast<int>(std::ceil(e.def->hp * 2.5));
                        e.dmg += e.elite->dmgAdd;
                    }
                    m.enemies.push_back(e);
                    break;
                }
        }
    }
    if (!def->boss.empty() && !bossesDefeated.count(def->id)) {
        const TilePos c = m.bossRoom.center();
        m.enemies.push_back(makeEnemy(db.enemy(def->boss), c.c * TILE + 8, (m.bossRoom.y + 3) * TILE + 8, false));
    }
}

// Overworld monsters live at spawn points and only exist near the player.
void Sim::updateSpawns(Map& m) {
    if (!m.outdoor) return;
    const Tuning& t = db.tuning;
    const double pc = P.x / TILE, pr = P.y / TILE;
    for (size_t i = 0; i < m.spawns.size(); i++) {
        SpawnPoint& s = m.spawns[i];
        if (s.enemyUid || tickCount < s.respawnAt || !s.def) continue;
        const double d = hypot2(s.c - pc, s.r - pr);
        if (d > t.spawnActive || d < 13) continue;
        Enemy e = makeEnemy(*s.def, s.c * TILE + 8, s.r * TILE + 12, true);
        if (s.forceElite && !e.elite && !db.elites.empty()) {
            e.elite = &db.elites[rng.range(static_cast<int>(db.elites.size()))];
            e.hp = e.maxHp = static_cast<int>(std::ceil(e.def->hp * 2.5));
            e.dmg += e.elite->dmgAdd;
        }
        e.spawn = static_cast<int>(i);
        s.enemyUid = e.uid;
        m.enemies.push_back(e);
    }
    for (auto& e : m.enemies) {
        if (!e.alive || (e.spawn < 0 && !e.night)) continue;
        if (hypot2(e.x / TILE - pc, e.y / TILE - pr) > t.spawnDespawn) {
            if (e.spawn >= 0) m.spawns[e.spawn].enemyUid = 0;
            e.alive = false;
            e.sank = true;   // vanish without a corpse
        }
    }
}

void Sim::toast(const std::string& text, Color col) {
    toasts.push_back({text, col, 220});
    if (toasts.size() > 4) toasts.erase(toasts.begin());
}
void Sim::showBanner(const std::string& icon, const std::string& title, const std::string& sub, Color col) { banner = Banner{icon, title, sub, col, 170}; }
void Sim::floater(double x, double y, const std::string& text, Color col) { floaters.push_back({x, y, y, -1.6, 8, 56, text, col}); }
void Sim::burst(double x, double y, Color col, int n, double speed, double grav) {
    for (int i = 0; i < n; i++) {
        const double a = rng.next() * PI * 2, s = (0.3 + rng.next()) * speed;
        particles.push_back({x, y, std::cos(a) * s, std::sin(a) * s - 0.4, grav, 20 + rng.range(20), rng.chance(0.3) ? 2 : 1, col});
    }
}

double Sim::hourOfDay() const {
    const int day = db.tuning.dayLength;
    return std::fmod(6.0 + 24.0 * (time % day) / day, 24.0);
}
double Sim::darkness() const {
    const double h = hourOfDay();
    if (h >= 7 && h < 18) return 0;
    if (h >= 18 && h < 20) return (h - 18) / 2;
    if (h >= 5 && h < 7) return 1 - (h - 5) / 2;
    return 1;
}
std::string Sim::clockText() const {
    const double h = hourOfDay();
    const int hh = static_cast<int>(h), mm = static_cast<int>((h - hh) * 6) * 10;
    char buf[64];
    std::snprintf(buf, sizeof buf, "Day %d · %02d:%02d%s", time / db.tuning.dayLength + 1, hh, mm, isNight() ? " · night" : "");
    return buf;
}
void Sim::sleepUntilMorning() {
    const int day = db.tuning.dayLength, t7 = day / 24, base = time / day * day;
    time = hourOfDay() < 7 ? base + t7 : base + day + t7;
}

void Sim::gainXp(int amount) {
    if (amount <= 0 || P.level >= db.tuning.maxLevel) return;
    P.xp += amount;
    floater(P.x, P.y - 30, "+" + std::to_string(amount) + " xp", Color(196, 160, 255));
    while (P.level < db.tuning.maxLevel && P.xp >= db.xpForLevel(P.level)) {
        P.xp -= db.xpForLevel(P.level);
        P.level++;
        P.hp = maxHp();
        const int pts = db.tuning.skillPointsPerLevel;
        std::string perk = "+" + std::to_string(pts) + (pts == 1 ? " skill point" : " skill points");
        if (db.tuning.heartEvery > 0 && P.level % db.tuning.heartEvery == 0) perk += " · +1 heart";
        if (P.level % db.tuning.damageEvery == 0) perk += " · +1 damage";
        showBanner("levelup", "Level " + std::to_string(P.level), perk, Color(90, 60, 160));
        play("levelup");
        burst(P.x, P.y - 10, Color(255, 226, 138), 30, 1.8, -0.01);
    }
    if (P.level >= db.tuning.maxLevel) P.xp = 0;
}

int Sim::skillRank(const std::string& id) const {
    auto it = P.skills.find(id);
    return it == P.skills.end() ? 0 : it->second;
}

double Sim::skillStat(const std::string& stat) const {
    double total = 0;
    for (const auto& [id, rank] : P.skills)
        if (const SkillDef* s = db.skill(id))
            for (const auto& [k, v] : s->effects)
                if (k == stat) total += v * rank;
    return total;
}

int Sim::skillPoints() const {
    int spent = 0;
    for (const auto& [id, rank] : P.skills) spent += rank;
    return (P.level - 1) * db.tuning.skillPointsPerLevel - spent;
}

std::string Sim::skillBlocked(const SkillDef& s) const {
    const int rank = skillRank(s.id);
    if (rank >= s.maxRank) return "Fully learned.";
    if (!s.requires_.empty() && skillRank(s.requires_) < s.requiresRank) {
        const SkillDef* r = db.skill(s.requires_);
        return "Learn " + (r ? r->name : s.requires_) + (s.requiresRank > 1 ? " to rank " + std::to_string(s.requiresRank) : std::string()) + " first.";
    }
    if (P.level < s.level) return "Requires level " + std::to_string(s.level) + ".";
    if (skillPoints() <= 0) return "No skill points left. Each level gives one.";
    return "";
}

bool Sim::learnSkill(const std::string& id) {
    const SkillDef* s = db.skill(id);
    if (!s) return false;
    if (const std::string why = skillBlocked(*s); !why.empty()) {
        toast(why);
        play("deny");
        return false;
    }
    const int before = maxHp();
    P.skills[id]++;
    P.hp = std::min(maxHp(), P.hp + std::max(0, maxHp() - before));
    const int rank = P.skills[id];
    showBanner("levelup", s->name, s->maxRank > 1 ? "Rank " + std::to_string(rank) + " of " + std::to_string(s->maxRank) : "Learned", Color(90, 60, 160));
    play("learn");
    wantSave = true;
    return true;
}

int Sim::whirlCooldown() const {
    const double cut = std::clamp(skillStat("whirlcd"), 0.0, 60.0);
    return static_cast<int>(std::lround(db.tuning.whirlCooldown * (1 - cut / 100)));
}

bool Sim::flying() const {
    if (P.mount.empty()) return false;
    const ItemDef* d = db.findItem(P.mount);
    return d && d->mountFly;
}

std::string Sim::restBlocked() const {
    if (P.combatT < db.tuning.combatLockTicks) return "You cannot rest in the middle of a fight.";
    for (const auto& e : map().enemies) {
        if (!e.alive || e.def->passive) continue;
        if (hypot2(e.x - P.x, e.y - P.y) < db.tuning.restEnemyRadius) return "Monsters are too close. You cannot rest with enemies nearby.";
    }
    return "";
}

const Enemy* Sim::activeBoss() const {
    for (const auto& e : map().enemies)
        if (e.alive && e.def->isBoss() && e.active) return &e;
    return nullptr;
}

int Sim::combatLevel() const {
    int level = 0;
    for (const auto& e : map().enemies) {
        if (!e.alive || e.def->passive) continue;
        if (e.def->isBoss()) {
            if (e.active) return 3;
            continue;
        }
        if (e.aggroT < 200) level = std::max(level, e.elite ? 2 : 1);
    }
    return level;
}

void Sim::updateCamera(bool snap) {
    const Map& m = map();
    const double mw = m.w * TILE, mh = m.h * TILE;
    double tx = P.x - viewW / 2, ty = P.y - 8 - viewH / 2;
    tx = mw <= viewW ? (mw - viewW) / 2 : std::clamp(tx, 0.0, mw - viewW);
    ty = mh <= viewH ? (mh - viewH) / 2 : std::clamp(ty, 0.0, mh - viewH);
    if (snap) cam = {tx, ty};
    else {
        cam.x += (tx - cam.x) * 0.2;
        cam.y += (ty - cam.y) * 0.2;
    }
}

void Sim::updateFx() {
    for (auto& p : particles) {
        p.x += p.vx;
        p.y += p.vy;
        p.vy += p.g;
        p.vx *= 0.96;
        p.t--;
    }
    std::erase_if(particles, [](const Particle& p) { return p.t <= 0; });
    for (auto& f : floaters) {
        f.y += f.vy;
        f.vy += 0.1;
        if (f.y > f.y0 && f.vy > 0) {
            f.y = f.y0;
            f.vy = f.vy > 0.6 ? -f.vy * 0.4 : 0;
        }
        if (f.vy == 0) {
            f.y0 -= 0.15;
            f.y = f.y0;
        }
        if (f.pop > 0) f.pop--;
        f.t--;
    }
    std::erase_if(floaters, [](const Floater& f) { return f.t <= 0; });
    if (shake > 0) {
        shake *= 0.86;
        if (shake < 0.3) shake = 0;
    }
}

// Which zone and town the hero stands in: area titles, discoveries, waypoints, visit quests.
void Sim::updatePlace(Map& m) {
    if (!m.outdoor) return;
    const int c = ifloor(P.x / TILE), r = ifloor(P.y / TILE);
    const TownDef* town = townAt(db, c, r, 0);
    const std::string newTown = town ? town->id : "";
    const ZoneDef& z = zoneAt(db, m, c, r);
    if (newTown != townId) {
        townId = newTown;
        if (town) {
            area = AreaTitle{town->name, "Safe haven", 200};
            P.lastTown = town->id;
            if (town->has("waypoint") && waypoints.insert(town->id).second && waypoints.size() > 1) toast("Waypoint unlocked: " + town->name, Color(159, 210, 255));
            questEvent("visit", town->id);
            wantSave = true;
        } else area = AreaTitle{z.name, "Levels " + std::to_string(z.levelMin) + " to " + std::to_string(z.levelMax), 200};
    }
    if (z.id != zoneId) {
        zoneId = z.id;
        if (!town) area = AreaTitle{z.name, "Levels " + std::to_string(z.levelMin) + " to " + std::to_string(z.levelMax), 200};
        if (zonesFound.insert(z.id).second && zonesFound.size() > 1) {
            toast("Discovered: " + z.name, Color(242, 210, 122));
            gainXp(db.tuning.discoverZoneXp);
        }
    }
}

void Sim::dismount(bool quiet) {
    P.mountCast = 0;
    if (P.mount.empty()) return;
    if (flying()) {   // never drop the hero into a lake or a forest
        if (!land(quiet)) return;
        return;
    }
    P.mount.clear();
    if (!quiet) burst(P.x, P.y, Color(200, 176, 144), 6, 0.7, 0.02);
}

bool Sim::land(bool quiet) {
    Map& m = map();
    const int c = ifloor(P.x / TILE), r = ifloor(P.y / TILE);
    if (!m.boxFree(P.x, P.y, 4, 3, false)) {
        const TilePos p = freeNear(m, c, r);
        if (!m.tileFree(p.c, p.r, false) || hypot2(p.c - c, p.r - r) > 8) {
            if (!quiet) {
                toast("Nowhere to land here. Fly over open ground.");
                play("deny");
            }
            return false;
        }
        P.x = p.c * TILE + 8;
        P.y = p.r * TILE + 10;
    }
    P.mount.clear();
    P.mountCast = 0;
    if (!quiet) {
        play("fly");
        burst(P.x, P.y, Color(240, 220, 170), 14, 1.2, 0.03);
    }
    return true;
}

void Sim::travelTo(const std::string& town) {
    const TownDef* t = db.town(town);
    if (!t) return;
    play("waypoint");
    transition([this, t] {
        const Map& over = overworld();
        TilePos p{t->at.c, t->at.r + 3};
        for (const auto& o : over.objects)
            if (o.kind == ObjKind::Waypoint && o.id == t->id) p = {o.c, o.r + 1};
        p = freeNear(over, p.c, p.r);
        enterMap("over", p.c * TILE + 8, p.r * TILE + 10);
        P.dir = Dir::Down;
        mode = Mode::Play;
        wantSave = true;
    });
}

void Sim::respawn() {
    P.hp = maxHp();
    P.iframes = 90;
    P.recallT = 0;
    const int lost = static_cast<int>(countItem("gold") * db.tuning.goldLost);
    if (lost) removeItem("gold", lost);
    const TownDef* t = db.town(P.lastTown);
    if (!t) t = &db.towns.front();
    const Map& over = overworld();
    TilePos p{t->at.c, t->at.r + 3};
    for (const auto& o : over.objects)
        if (o.kind == ObjKind::Campfire && o.id == t->id) p = {o.c, o.r + 1};
    p = freeNear(over, p.c, p.r);
    enterMap("over", p.c * TILE + 8, p.r * TILE + 10);
    mode = Mode::Play;
    toast(lost ? "You wake by the fire in " + t->name + ". You dropped " + std::to_string(lost) + " gold." : "You wake by the fire in " + t->name + ".", Color(242, 210, 122));
    wantSave = true;
}

void Sim::action(const std::string& a, bool mouse) {
    if (mode != Mode::Play) return;
    if (a == "attack") {
        attackBuf_ = db.tuning.inputBuffer;
        attackMouse_ = mouse;
    } else if (a == "shoot") {
        shootBuf_ = db.tuning.inputBuffer;
        shootMouse_ = mouse;
    } else if (a == "dodge") wantDodge_ = true;
    else if (a == "interact") wantInteract_ = true;
    else if (a == "heal") wantHeal_ = true;
    else if (a == "skill") wantSkill_ = true;
    else if (a == "mount") wantMount_ = true;
    else if (a == "recall") wantRecall_ = true;
    else if (a == "quick1") quickUse("bandage");
    else if (a == "quick2") quickUse("roast");
    else if (a == "quick3") quickUse("potion");
}

void Sim::tick(const SimInput& in) {
    in_ = in;
    tickCount++;
    if (fade) {
        fade->t++;
        if (fade->t == fade->dur && fade->mid) {
            auto mid = std::move(fade->mid);
            fade->mid = nullptr;
            mid();
        }
        if (fade && fade->t >= fade->dur * 2) fade.reset();
    }
    for (auto& t : toasts) t.t--;
    std::erase_if(toasts, [](const Toast& t) { return t.t <= 0; });
    if (banner && --banner->t <= 0) banner.reset();
    if (area && --area->t <= 0) area.reset();
    if (craftFlash) craftFlash--;
    if (!worldReady) return;

    if (mode == Mode::Title) {
        const Map& over = overworld();
        cam.x = std::clamp(over.start.c * TILE - viewW / 2 + std::sin(tickCount / 900.0) * 260, 0.0, over.w * TILE - viewW);
        cam.y = std::clamp(over.start.r * TILE - viewH / 2 + std::cos(tickCount / 1100.0) * 150, 0.0, over.h * TILE - viewH);
        return;
    }
    if (mode != Mode::Play) {
        updateFx();
        return;
    }
    if (hitstop > 0) {   // the world freezes for a few frames when a blow lands
        hitstop--;
        return;
    }
    time++;
    Map& m = map();
    updatePlayer();
    if (mapId != m.id) return;   // a transition moved us
    updateEnemies(m);
    updateProjectiles(m);
    updateDrops(m);
    updateNodes(m);
    for (auto& c : m.corpses) c.t++;
    std::erase_if(m.corpses, [](const Corpse& c) { return c.t >= 150; });
    updateWater(m);
    updateTraps(m);
    updateNight(m);
    if (tickCount % 20 == 0) updateSpawns(m);
    if (tickCount % 15 == 0) updatePlace(m);
    updateFx();
    updateFocus(m);
    updateCamera(false);
    if (tickCount % 6 == 0) explore(m);
    if (victoryAt && tickCount >= victoryAt) {
        victoryAt = 0;
        mode = Mode::Victory;
        wantSave = true;
    }
    if (time % db.tuning.autosaveTicks == 0) wantSave = true;
}

}   // namespace hc

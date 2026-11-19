// Unit tests for the rules and the world generator. They run headless (Node for the web build).
#include "doctest.h"

#include "core/rng.h"
#include "game/sim.h"

#include <cmath>

using namespace hc;

namespace {

const Database& database() {
    static const Database db = Database::load(HC_CONFIG_DIR);
    return db;
}

// A fresh game with the hero standing at the Greenhollow start point.
Sim freshGame(uint32_t seed = 7) {
    Sim sim(database(), seed);
    sim.newGame("Tester");
    return sim;
}

void run(Sim& sim, int ticks, SimInput in = {}) {
    for (int i = 0; i < ticks; i++) sim.tick(in);
}

}   // namespace

TEST_CASE("rng is deterministic and in range") {
    Rng a(1337), b(1337);
    for (int i = 0; i < 1000; i++) {
        const double v = a.next();
        CHECK(v == b.next());
        CHECK(v >= 0.0);
        CHECK(v < 1.0);
    }
    CHECK(hash2(3, 4, 5) == hash2(3, 4, 5));
    CHECK(hash2(3, 4, 5) != hash2(4, 3, 5));
    const double n = fbm(1.5, 2.5, 9);
    CHECK(n >= 0.0);
    CHECK(n <= 1.0);
}

TEST_CASE("config loads and cross-references resolve") {
    const Database& db = database();
    CHECK(db.items.size() > 30);
    CHECK(db.enemies.size() > 25);
    CHECK(db.zones.size() == 10);
    CHECK(db.towns.size() == 7);
    CHECK(db.dungeons.size() == 11);
    CHECK(db.quests.size() >= 20);
    CHECK(db.enemy("wolf").look == Look::Static);   // inherited from its base
    CHECK(db.enemy("wolf").hp > db.enemy("fox").hp);   // "base" copies, the variant overrides
    CHECK(db.item("iron_sword").isGear());
    CHECK(db.item("wood").stacks());
    CHECK(db.xpForLevel(2) > db.xpForLevel(1));
    for (const auto& q : db.quests) CHECK(db.npc(q.giver) != nullptr);
}

TEST_CASE("collision: boxes stop at walls and slide along them") {
    Map m = makeMap("t", 8, 8, FLOOR);
    for (int r = 0; r < 8; r++) m.set(4, r, WALL);
    CHECK(m.tileFree(1, 1, false));
    CHECK_FALSE(m.tileFree(4, 1, false));
    CHECK_FALSE(m.tileFree(-1, 0, false));
    double x = 3 * TILE + 8, y = 3 * TILE + 8;
    const auto [hx, hy] = m.moveBox(x, y, 6, 2, 4, 3, false);
    CHECK(hx);
    CHECK_FALSE(hy);
    CHECK(x == doctest::Approx(3 * TILE + 8));
    CHECK(y == doctest::Approx(3 * TILE + 10));
}

TEST_CASE("pathfinding goes around obstacles") {
    Map m = makeMap("t", 12, 12, FLOOR);
    for (int r = 0; r < 10; r++) m.set(6, r, WALL);
    const auto path = astar(m, {2, 2}, {9, 2}, [&](int c, int r) { return walkable(m.get(c, r)) ? 1.0 : -1.0; });
    REQUIRE_FALSE(path.empty());
    CHECK(path.front() == TilePos{2, 2});
    CHECK(path.back() == TilePos{9, 2});
    for (const auto& p : path) CHECK(m.get(p.c, p.r) != WALL);
}

TEST_CASE("overworld: towns, dungeons and chests are reachable from the start") {
    const Database& db = database();
    Map over = generateOverworld(db);
    CHECK(over.w == db.tuning.worldW);
    CHECK(over.h == db.tuning.worldH);
    REQUIRE(over.tileFree(over.start.c, over.start.r, false));
    for (auto& o : over.objects)   // every boss slain: the region gates stand open
        if (o.kind == ObjKind::Gate && o.id == "zone_gate") over.setBlock(o, false);
    const auto mask = reachMask(over, over.start.c, over.start.r);
    auto near = [&](int c, int r) {
        for (int dr = -1; dr <= 1; dr++)
            for (int dc = -1; dc <= 1; dc++)
                if (over.inside(c + dc, r + dr) && mask[over.idx(c + dc, r + dr)]) return true;
        return false;
    };
    for (const auto& t : db.towns) CHECK_MESSAGE(near(t.at.c, t.at.r + 3), "town not reachable: " << t.id);
    for (const auto& d : db.dungeons)
        if (d.under.empty()) {
            CHECK_MESSAGE(near(d.at.c, d.at.r + 1), "dungeon not reachable: " << d.id);
            CHECK(over.get(d.at.c, d.at.r) == STAIRS_DOWN);
        }
    int chests = 0, npcs = 0, shrines = 0;
    for (const auto& o : over.objects) {
        if (o.kind == ObjKind::Chest) {
            chests++;
            CHECK_MESSAGE(near(o.c, o.r), "chest not reachable at " << o.c << "," << o.r);
        }
        if (o.kind == ObjKind::Npc) npcs++;
        if (o.kind == ObjKind::Shrine) shrines++;
    }
    CHECK(chests > 20);
    CHECK(shrines >= 6);
    size_t wantNpcs = 0;
    for (const auto& t : db.towns) wantNpcs += t.npcs.size();
    CHECK(npcs == wantNpcs);
    CHECK(over.spawns.size() > 150);
    // Same seed, same world
    const Map again = generateOverworld(db);
    CHECK(again.t == over.t);
    CHECK(again.objects.size() == over.objects.size());
}

TEST_CASE("dungeons: every room connects to the entrance and the reward is reachable") {
    const Database& db = database();
    const Map over = generateOverworld(db);
    for (const auto& d : db.dungeons) {
        const Map m = generateDungeon(db, d, over);
        REQUIRE_MESSAGE(m.tileFree(m.start.c, m.start.r, false), d.id);
        const auto mask = reachMask(m, m.start.c, m.start.r);
        if (d.kind == "generated") {
            for (const auto& R : m.rooms) CHECK_MESSAGE(mask[m.idx(R.center().c, R.center().r)] == 1, d.id);
            bool reward = false;
            for (const auto& o : m.objects)
                if (o.kind == ObjKind::Chest && o.id == d.id + "_reward") reward = mask[m.idx(o.c, o.r + 1)] == 1;
            CHECK_MESSAGE(reward, d.id);
        }
        CHECK_FALSE(m.links.empty());
    }
}

TEST_CASE("inventory: stacking, gear instances, equipping, removal") {
    Sim sim = freshGame();
    CHECK(sim.countItem("wood") == 0);
    CHECK(sim.addItem({"wood", 3}));
    CHECK(sim.addItem({"wood", 2}));
    CHECK(sim.countItem("wood") == 5);
    CHECK(sim.P.inv.size() == 1);
    CHECK_FALSE(sim.removeItem("wood", 9));
    CHECK(sim.removeItem("wood", 5));
    CHECK(sim.P.inv.empty());

    const int fists = sim.weaponDamage();
    CHECK(sim.addItem({"wood_sword", 1}));
    REQUIRE(sim.equipped(EquipSlot::Weapon) != nullptr);
    CHECK(sim.weaponDamage() == fists - 1 + 2);
    CHECK(sim.addItem({"iron_sword", 1}));   // better tier: auto-equips
    CHECK(sim.equipped(EquipSlot::Weapon)->id == "iron_sword");
    CHECK(sim.P.inv.size() == 2);            // gear never stacks
    CHECK(sim.P.inv[0].uid != sim.P.inv[1].uid);
    sim.removeItem("iron_sword", 1);
    CHECK(sim.equipped(EquipSlot::Weapon) == nullptr);

    const int hp = sim.maxHp();
    sim.addItem({"leather_armor", 1});
    CHECK(sim.maxHp() == hp + 1);   // half a heart
}

TEST_CASE("crafting chain: wood to sword, fire recipes need a fire") {
    Sim sim = freshGame();
    const RecipeDef* sword = nullptr;
    const RecipeDef* roast = nullptr;
    for (const auto& r : sim.db.recipes) {
        if (r.out == "wood_sword") sword = &r;
        if (r.out == "roast") roast = &r;
    }
    REQUIRE(sword);
    REQUIRE(roast);
    CHECK_FALSE(sim.canCraft(*sword));
    sim.addItem({"wood", 4});
    CHECK(sim.canCraft(*sword));
    CHECK(sim.craft(*sword, 1) == 1);
    CHECK(sim.countItem("wood") == 0);
    CHECK(sim.hasItem("wood_sword"));

    sim.addItem({"meat", 1});
    sim.addItem({"herb", 1});
    // Walk the hero far from any fire: the recipe must be refused
    sim.P.x = 5 * TILE;
    sim.P.y = 5 * TILE;
    CHECK(sim.recipeBlocked(*roast) != "");
    CHECK(sim.craft(*roast, 1) == 0);
    CHECK(sim.hasItem("meat"));
}

TEST_CASE("gear rolls respect rarity and slots") {
    Sim sim = freshGame();
    for (int i = 0; i < 200; i++) {
        const Item it = sim.makeGear("iron_sword", 2);
        CHECK(it.rarity >= 2);
        CHECK(it.affixes.size() == static_cast<size_t>(sim.db.rarities[it.rarity].affixes));
        for (const auto& [id, v] : it.affixes) {
            const AffixDef* a = sim.db.affix(id);
            REQUIRE(a);
            CHECK(v >= a->min);
            CHECK(v <= a->max);
            CHECK(std::find(a->slots.begin(), a->slots.end(), ItemType::Weapon) != a->slots.end());
        }
    }
    const Item gear = sim.randomGear(1);
    CHECK(sim.db.item(gear.id).isGear());
}

TEST_CASE("combat: shields reduce strong hits only, hits grant invulnerability, death and respawn") {
    Sim sim = freshGame();
    sim.P.hp = sim.maxHp();
    const int full = sim.P.hp;
    sim.hurtPlayer(1, 0, 0);
    CHECK(sim.P.hp == full - 1);
    sim.hurtPlayer(4, 0, 0);   // still invulnerable
    CHECK(sim.P.hp == full - 1);
    sim.P.iframes = 0;
    sim.addItem({"wood_shield", 1});
    sim.hurtPlayer(3, 0, 0);   // 3 - 1 block
    CHECK(sim.P.hp == full - 3);
    sim.P.iframes = 0;
    sim.hurtPlayer(1, 0, 0);   // weak hits are never reduced to zero
    CHECK(sim.P.hp == full - 4);

    sim.addItem({"gold", 40});
    sim.P.iframes = 0;
    sim.hurtPlayer(99, 0, 0);
    CHECK(sim.mode == Mode::Dead);
    sim.respawn();
    CHECK(sim.mode == Mode::Play);
    CHECK(sim.P.hp == sim.maxHp());
    CHECK(sim.countItem("gold") == 30);
}

TEST_CASE("killing monsters gives xp, levels and quest credit") {
    Sim sim = freshGame();
    Map& m = sim.map();
    // Take the boar quest directly
    sim.quests["thin_herd"] = {1, {0}};
    const int level = sim.P.level;
    for (int i = 0; i < 6; i++) {
        m.enemies.push_back(sim.makeEnemy(sim.db.enemy("boar"), sim.P.x + 200, sim.P.y, false));
        sim.damageEnemy(m.enemies.back(), 99, 0, 0);
    }
    const QuestDef* q = sim.db.quest("thin_herd");
    REQUIRE(q);
    CHECK(sim.objectiveProgress(*q, 0) == 6);
    CHECK(sim.questReady(*q));
    CHECK(sim.npcMarker("mira") == 3);
    CHECK((sim.P.level > level || sim.P.xp > 0));
    sim.gainXp(100000000);
    CHECK(sim.P.level == sim.db.tuning.maxLevel);
}

TEST_CASE("quests: prerequisites gate availability") {
    Sim sim = freshGame();
    const QuestDef* first = sim.db.quest("first_blade");
    const QuestDef* stew = sim.db.quest("camp_stew");
    REQUIRE(first);
    REQUIRE(stew);
    CHECK(sim.questAvailable(*first));
    CHECK_FALSE(sim.questAvailable(*stew));
    CHECK(sim.npcMarker("warden") == 1);
    sim.quests["first_blade"] = {2, {1}};
    CHECK(sim.questAvailable(*stew));
}

TEST_CASE("simulation runs: walking moves the hero, the clock advances, nothing crashes") {
    Sim sim = freshGame();
    const double x0 = sim.P.x, y0 = sim.P.y;
    const int t0 = sim.time;
    SimInput in;
    in.moveY = 1;
    run(sim, 120, in);
    CHECK(sim.time == t0 + 120);
    CHECK((sim.P.x != x0 || sim.P.y != y0));
    // A long idle stretch with spawning, night and autosave ticking over
    run(sim, 4000);
    CHECK(sim.mode == Mode::Play);
    CHECK(sim.zonesFound.count("vale") == 1);
    CHECK(sim.waypoints.count("greenhollow") == 1);
}

TEST_CASE("day and night") {
    Sim sim = freshGame();
    CHECK(sim.hourOfDay() == doctest::Approx(sim.db.tuning.startHour).epsilon(0.01));
    CHECK_FALSE(sim.isNight());
    sim.time = sim.db.tuning.dayLength * 17 / 24;   // 23:00
    CHECK(sim.isNight());
    CHECK(sim.darkness() == doctest::Approx(1.0));
}

TEST_CASE("save and load round-trip") {
    Sim sim = freshGame();
    sim.addItem({"wood", 7});
    sim.addItem(sim.makeGear("iron_sword", 3));
    sim.addItem({"gold", 55});
    sim.gainXp(120);
    sim.quests["first_blade"] = {2, {1}};
    sim.quests["thin_herd"] = {1, {4}};
    sim.bossesDefeated.insert("cellar");
    Map& m = sim.map();
    int opened = 0;
    for (auto& o : m.objects)
        if (o.kind == ObjKind::Chest && !o.mimic && opened < 2) {
            o.opened = true;
            o.openAt = 1;
            o.loot.clear();
            opened++;
        }
    run(sim, 30);
    const json saved = sim.saveGame();
    const std::string text = saved.dump();

    Sim other(database(), 99);
    REQUIRE(other.loadGame(json::parse(text)));
    CHECK(other.P.name == "Tester");
    CHECK(other.P.level == sim.P.level);
    CHECK(other.P.xp == sim.P.xp);
    CHECK(other.countItem("wood") == 7);
    CHECK(other.countItem("gold") == 55);
    REQUIRE(other.equipped(EquipSlot::Weapon) != nullptr);
    CHECK(other.equipped(EquipSlot::Weapon)->rarity == 3);
    CHECK(other.equipped(EquipSlot::Weapon)->affixes.size() == 3);
    CHECK(other.weaponDamage() == sim.weaponDamage());
    CHECK(other.quests["first_blade"].state == 2);
    CHECK(other.quests["thin_herd"].counts[0] == 4);
    CHECK(other.bossesDefeated.count("cellar") == 1);
    CHECK(other.time == sim.time);
    int openedAfter = 0;
    for (const auto& o : other.map().objects)
        if (o.kind == ObjKind::Chest && o.opened) openedAfter++;
    CHECK(openedAfter == opened);
    CHECK(other.P.x == doctest::Approx(sim.P.x));
    CHECK(other.map().explored == sim.map().explored);

    Sim bad(database(), 1);
    CHECK_FALSE(bad.loadGame(json::parse("{\"v\": 1}")));
    CHECK_FALSE(bad.loadGame(json("nonsense")));
}

// A bot mashes the controls in every map: it must never crash, leave the map or corrupt a save.
TEST_CASE("soak: a random bot plays every map") {
    Sim sim = freshGame(21);
    for (const char* id : {"iron_sword", "wood_shield", "bow", "torch", "pickaxe", "mount_boar"}) sim.addItem({id, 1});
    sim.addItem({"arrows", 500});
    sim.addItem({"potion", 20});
    sim.gainXp(2000);
    std::vector<std::string> ids;
    for (const auto& [id, m] : sim.maps) ids.push_back(id);
    REQUIRE(ids.size() >= 2);
    Rng bot(99);
    const char* actions[] = {"attack", "shoot", "dodge", "interact", "heal", "quick1", "quick2", "quick3", "skill", "mount", "recall"};
    for (const std::string& id : ids) {
        const Map& target = sim.maps.at(id);
        sim.enterMap(id, target.start.c * TILE + 8, target.start.r * TILE + 10);
        SimInput in;
        for (int t = 0; t < 2500; t++) {
            if (t % 20 == 0) {
                const double a = bot.next() * 6.283185307;
                in.moveX = std::cos(a);
                in.moveY = std::sin(a);
                in.holdAttack = bot.next() < 0.4;
                in.holdShoot = bot.next() < 0.2;
            }
            if (bot.next() < 0.05) sim.action(actions[bot.range(11)], false);
            // Menus and the death screen stop the world: get back into play the way the app would
            if (sim.mode == Mode::Dead) sim.respawn();
            else if (sim.mode != Mode::Play) sim.mode = Mode::Play;
            sim.tick(in);
        }
        const Map& m = sim.map();
        CHECK(sim.P.x >= 0);
        CHECK(sim.P.y >= 0);
        CHECK(sim.P.x <= m.w * TILE);
        CHECK(sim.P.y <= m.h * TILE);
        CHECK(sim.P.hp >= 0);
        CHECK(sim.P.hp <= sim.maxHp());
        Sim copy(database(), 5);
        CHECK(copy.loadGame(json::parse(sim.saveGame().dump())));
        CHECK(copy.mapId == sim.mapId);
    }
}

TEST_CASE("bag: worn gear leaves the bag, things can be thrown away and picked up again") {
    Sim sim = freshGame();
    const int hp = sim.maxHp();
    CHECK(sim.addItem({"leather_cap", 1}));   // auto-equips into the empty head slot
    REQUIRE(sim.equipped(EquipSlot::Helm) != nullptr);
    CHECK(sim.maxHp() == hp + 1);
    CHECK(sim.bagUsed() == 0);
    CHECK(sim.addItem({"leather_boots", 1}));
    CHECK(sim.speedMul() > 1.0);
    CHECK(sim.bagView().empty());

    // A worn piece goes back to the bag, and only fits its own slot
    REQUIRE(sim.unequip(EquipSlot::Helm));
    CHECK(sim.bagUsed() == 1);
    const int cap = sim.bagView().front();
    CHECK_FALSE(sim.equipTo(cap, EquipSlot::Weapon));
    CHECK(sim.equipTo(cap, EquipSlot::Helm));

    // Throwing away: the item lands in front of the hero and is not sucked straight back in
    sim.addItem({"wood", 5});
    const int wood = sim.bagView().front();
    REQUIRE(sim.discardItem(wood, 2));
    CHECK(sim.countItem("wood") == 3);
    REQUIRE(sim.map().drops.size() == 1);
    CHECK(sim.map().drops.front().tossed);
    run(sim, 60);
    CHECK(sim.countItem("wood") == 3);
    // Walk off and come back: now it is loot again
    const double x0 = sim.P.x, y0 = sim.P.y;
    sim.P.x += 48;
    run(sim, 2);
    sim.P.x = x0;
    sim.P.y = y0;
    run(sim, 90);
    CHECK(sim.countItem("wood") == 5);

    // Some things never leave the bag
    sim.addItem({"gold", 10});
    sim.addItem({"key", 1});
    for (int i = 0; i < static_cast<int>(sim.P.inv.size()); i++)
        if (sim.P.inv[i].id == "gold" || sim.P.inv[i].id == "key") CHECK_FALSE(sim.discardItem(i, 1));
    CHECK(sim.countItem("gold") == 10);
    CHECK(sim.hasItem("key"));

    // A full bag refuses new things, and throwing one away makes room
    while (!sim.bagFull()) sim.P.inv.push_back({"torch", 1, 0, 0, {}});
    CHECK_FALSE(sim.addItem({"stone", 1}, true));
    CHECK(sim.discardItem(sim.bagView().back(), 1));
    CHECK(sim.addItem({"stone", 1}, true));
}

TEST_CASE("max health: levels give skill points, not hearts; enough shards make a heart") {
    Sim sim = freshGame();
    const int hp = sim.maxHp();
    sim.gainXp(sim.db.xpForLevel(1) + sim.db.xpForLevel(2));
    CHECK(sim.P.level == 3);
    CHECK(sim.maxHp() == hp);
    CHECK(sim.skillPoints() == 2 / sim.db.tuning.skillPointEvery * sim.db.tuning.skillPointsPerLevel);
    for (int i = 1; i < sim.db.tuning.heartShards; i++) sim.addItem({"heart_shard", 1});
    CHECK(sim.P.heartCrystals == 0);
    CHECK(sim.maxHp() == hp);
    sim.addItem({"heart_shard", 1});
    CHECK(sim.P.heartCrystals == 1);
    CHECK(sim.P.heartShards == 0);
    CHECK(sim.maxHp() == hp + 2);
}

TEST_CASE("skill tree: requirements, points and effects") {
    Sim sim = freshGame();
    CHECK_FALSE(sim.learnSkill("strength"));   // no points at level 1
    int need = 0;
    for (int l = 1; l < 7; l++) need += sim.db.xpForLevel(l);
    sim.gainXp(need);
    REQUIRE(sim.P.level == 7);
    REQUIRE(sim.skillPoints() == 3);   // one every two levels
    CHECK_FALSE(sim.whirlwindUnlocked());
    CHECK_FALSE(sim.learnSkill("whirlwind"));   // needs Strength first
    const int dmg = sim.weaponDamage();
    CHECK(sim.learnSkill("strength"));
    CHECK(sim.weaponDamage() == dmg + 1);
    CHECK(sim.learnSkill("whirlwind"));
    CHECK(sim.whirlwindUnlocked());
    const int hp = sim.maxHp();
    CHECK(sim.learnSkill("vitality"));
    CHECK(sim.maxHp() == hp + 1);   // half a heart a rank
    CHECK(sim.skillPoints() == 0);
    CHECK_FALSE(sim.learnSkill("vitality"));
    CHECK_FALSE(sim.learnSkill("cyclone"));   // level 5

    // Skills survive a save
    Sim other(sim.db, 3);
    REQUIRE(other.loadGame(sim.saveGame()));
    CHECK(other.skillRank("strength") == 1);
    CHECK(other.whirlwindUnlocked());
    CHECK(other.maxHp() == sim.maxHp());
}

TEST_CASE("dungeons: the dead stay dead until the hero goes back outside") {
    Sim sim = freshGame();
    auto alive = [&] {
        int n = 0;
        for (const auto& e : sim.map().enemies) n += e.alive;
        return n;
    };
    const Map& cellar = sim.maps.at("cellar");
    sim.enterMap("cellar", cellar.start.c * TILE + 8, cellar.start.r * TILE + 10);
    const int full = alive();
    REQUIRE(full > 2);
    for (auto& e : sim.map().enemies)
        if (!e.def->isBoss()) {
            sim.damageEnemy(e, 999, 0, 0);
            break;
        }
    CHECK(alive() == full - 1);
    sim.enterMap("cellar", cellar.start.c * TILE + 8, cellar.start.r * TILE + 10);   // back in from somewhere else underground
    CHECK(alive() == full - 1);
    sim.enterMap("over", sim.overworld().start.c * TILE + 8, sim.overworld().start.r * TILE + 10);
    sim.enterMap("cellar", cellar.start.c * TILE + 8, cellar.start.r * TILE + 10);
    CHECK(alive() >= full - 1);
    for (const auto& e : sim.map().enemies) CHECK(e.alive);
}

TEST_CASE("no resting at a fire during a fight or with monsters nearby") {
    Sim sim = freshGame();
    sim.P.combatT = 100000;
    CHECK(sim.restBlocked().empty());
    Map& m = sim.map();
    m.enemies.push_back(sim.makeEnemy(sim.db.enemy("wolf"), sim.P.x + 60, sim.P.y, false));
    CHECK_FALSE(sim.restBlocked().empty());
    m.enemies.back().x = sim.P.x + 400;
    CHECK(sim.restBlocked().empty());
    sim.P.combatT = 0;
    CHECK_FALSE(sim.restBlocked().empty());
}

TEST_CASE("towns sell different things") {
    const Database& db = database();
    std::set<std::string> seen;
    for (const auto& t : db.towns)
        for (const auto& n : t.npcs)
            if (n.role == "merchant") CHECK(seen.insert(n.shop).second);   // no two traders share a stock
    CHECK(seen.size() >= 4);
    const ShopDef* vale = db.shop("vale_stable");
    const ShopDef* peak = db.shop("peak_stable");
    REQUIRE(vale);
    REQUIRE(peak);
    bool roc = false;
    for (const auto& e : peak->buy) roc = roc || e.item == "mount_roc";
    for (const auto& e : vale->buy) CHECK(e.item != "mount_roc");
    CHECK(roc);
}

TEST_CASE("flying mount: crosses water, cannot be hurt, lands only on open ground") {
    Sim sim = freshGame();
    sim.addItem({"mount_roc", 1}, true);
    sim.P.mount = "mount_roc";
    REQUIRE(sim.flying());
    Map& m = sim.map();
    // A lake shore: walkable ground with water to the east
    int wc = -1, wr = -1;
    for (int r = 10; r < m.h - 10 && wc < 0; r++)
        for (int c = 10; c < m.w - 20 && wc < 0; c++)
            if (m.get(c, r) == WATER && m.get(c + 1, r) == WATER && m.get(c + 2, r) == WATER && m.tileFree(c - 1, r, false)) {
                wc = c;
                wr = r;
            }
    REQUIRE(wc >= 0);
    sim.enterMap("over", (wc - 1) * TILE + 8, wr * TILE + 10);
    SimInput in;
    in.moveX = 1;
    run(sim, 20, in);
    CHECK(sim.P.x > (wc + 1) * TILE);   // out over the water
    const int hp = sim.P.hp;
    sim.hurtPlayer(4, sim.P.x - 10, sim.P.y);
    CHECK(sim.P.hp == hp);
    sim.action("mount", false);
    run(sim, 2);
    if (!sim.flying()) CHECK(sim.map().boxFree(sim.P.x, sim.P.y, 4, 3, false));   // never set down in the lake
}

TEST_CASE("buildings: every one has a door, an inside you can walk around, and its people within reach") {
    Sim sim = freshGame();
    const Map& over = sim.overworld();
    for (const auto& b : sim.db.buildings) {
        CAPTURE(b.id);
        REQUIRE(over.doors.count(b.id) == 1);
        const TilePos door = over.doors.at(b.id);
        CHECK(over.get(door.c, door.r) == STAIRS_DOWN);
        CHECK(over.tileFree(door.c, door.r + 1, false));   // you can stand in front of it
        REQUIRE(sim.maps.count("b_" + b.id) == 1);
        const Map& in = sim.maps.at("b_" + b.id);
        CHECK(in.interior);
        const auto reach = reachMask(in, in.start.c, in.start.r);
        auto near = [&](int c, int r) {
            for (int dr = -1; dr <= 1; dr++)
                for (int dc = -1; dc <= 1; dc++)
                    if (in.inside(c + dc, r + dr) && reach[in.idx(c + dc, r + dr)]) return true;
            return false;
        };
        int people = 0;
        for (const auto& o : in.objects)
            if (o.kind == ObjKind::Npc || o.kind == ObjKind::Campfire) {
                CAPTURE(o.id);
                CHECK(near(o.c, o.r));
                people += o.kind == ObjKind::Npc;
            }
        CHECK(people == static_cast<int>(b.npcs.size()));
        // Floor tiles that cannot be reached would be wasted space hidden behind furniture: allow a few
        int floor = 0, reached = 0;
        for (int r = 0; r < in.h; r++)
            for (int c = 0; c < in.w; c++)
                if (in.get(c, r) == PLANKS && !in.block[in.idx(c, r)]) {
                    floor++;
                    reached += reach[in.idx(c, r)];
                }
        CHECK(reached * 10 >= floor * 8);
    }
    // In and out again
    const BuildingDef& b = sim.db.buildings.front();
    const TilePos door = over.doors.at(b.id);
    sim.enterMap("over", door.c * TILE + 8, (door.r + 1) * TILE + 10);
    SimInput up;
    up.moveY = -1;
    run(sim, 60, up);
    CHECK(sim.mapId == "b_" + b.id);
    CHECK(sim.townId == b.town);
    SimInput down;
    down.moveY = 1;
    run(sim, 120, down);
    CHECK(sim.mapId == "over");
}

TEST_CASE("everyone talks, and quests can be accepted, declined and handed in for a chosen reward") {
    Sim sim = freshGame();
    for (const auto& [id, m] : sim.maps)
        for (const auto& o : m.objects)
            if (o.kind == ObjKind::Npc) {
                const NpcDef* n = sim.db.npc(o.id);
                REQUIRE(n);
                CHECK_FALSE(n->lines.empty());
            }
    // Talking opens the conversation and does not accept anything by itself
    Map& m = sim.map();
    Object* warden = nullptr;
    for (auto& o : m.objects)
        if (o.kind == ObjKind::Npc && o.id == "warden") warden = &o;
    REQUIRE(warden);
    sim.talkTo(*warden);
    CHECK(sim.mode == Mode::Talk);
    CHECK(sim.talkNpc == "warden");
    CHECK_FALSE(sim.talkLine.empty());
    REQUIRE_FALSE(sim.questsOffered("warden").empty());
    CHECK(sim.quests.count("first_blade") == 0);   // declining = simply not accepting
    CHECK(sim.acceptQuest("first_blade"));
    CHECK(sim.quests.at("first_blade").state == 1);
    CHECK(sim.questsUnderway("warden").size() == 1);
    // Abandon puts it back on offer
    CHECK(sim.abandonQuest("first_blade"));
    CHECK(sim.questAvailable(*sim.db.quest("first_blade")));

    // A quest with a reward to choose: the chosen item is the one you get
    const QuestDef* joss = sim.db.quest("joss_hares");
    REQUIRE(joss);
    REQUIRE(joss->choice.size() == 2);
    CHECK(sim.acceptQuest("joss_hares"));
    for (int i = 0; i < 6; i++) sim.questEvent("kill", "hare");
    CHECK(sim.questReady(*joss));
    CHECK(sim.questsToHandIn("joss").size() == 1);
    const std::string picked = joss->choice[1].first;
    const int before = sim.countItem(picked);
    CHECK(sim.turnInQuest("joss_hares", 1));
    CHECK(sim.countItem(picked) == before + 1);
    CHECK(sim.countItem(joss->choice[0].first) == 0);
    CHECK(sim.quests.at("joss_hares").state == 2);
}

TEST_CASE("high ground: plateaus are ringed by cliffs and every one has a way up") {
    const Database& db = database();
    Map over = generateOverworld(db);
    REQUIRE(over.height.size() == over.t.size());
    for (auto& o : over.objects)
        if (o.kind == ObjKind::Gate && o.id == "zone_gate") over.setBlock(o, false);
    const auto mask = reachMask(over, over.start.c, over.start.r);
    int high = 0, ramps = 0, unreachable = 0;
    for (int r = 1; r < over.h - 1; r++)
        for (int c = 1; c < over.w - 1; c++) {
            const uint8_t t = over.get(c, r);
            if (t == RAMP) ramps++;
            const int h = over.heightAt(c, r);
            if (h == 0 || !walkable(t) || t == RAMP) continue;
            high++;
            // you never step straight off high ground onto lower ground: a cliff, a lip or a ramp is in between
            static const int DC[4] = {1, -1, 0, 0}, DR[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; k++) {
                const int nc = c + DC[k], nr = r + DR[k];
                const uint8_t n = over.get(nc, nr);
                if (walkable(n) && n != RAMP) CHECK_MESSAGE(over.heightAt(nc, nr) >= h, "open drop at " << c << "," << r);
            }
            if (!mask[over.idx(c, r)] && over.tileFree(c, r, false)) unreachable++;
        }
    CHECK(high > 300);   // there is real high ground in the world
    CHECK(ramps > 5);
    CHECK(unreachable < high / 10);   // nearly all of it can be walked to (odd pockets behind trees aside)
}

TEST_CASE("sword: the blade winds up, sweeps through the arc and follows through") {
    CHECK(Sim::swingOffset(0) <= -Sim::SWING_SPAN);   // it starts drawn back
    double last = Sim::swingOffset(2);
    for (int f = 3; f <= 14; f++) {   // then it only ever moves forward
        const double o = Sim::swingOffset(f);
        CHECK(o >= last);
        last = o;
    }
    CHECK(Sim::swingOffset(8) >= Sim::SWING_SPAN - 1e-9);
}

TEST_CASE("armour never blocks more than half of a hit") {
    Sim sim = freshGame();
    for (const char* id : {"champion_shield", "great_helm"}) sim.addItem({id, 1});
    sim.P.skills["bulwark"] = 1;
    REQUIRE(sim.shieldDef() >= 4);
    sim.P.hp = sim.maxHp();
    const int full = sim.P.hp;
    sim.hurtPlayer(6, 0, 0);
    CHECK(sim.P.hp == full - 3);
}

TEST_CASE("levels run to 99, slowly, and open new active skills") {
    Sim sim = freshGame();
    CHECK(sim.db.tuning.maxLevel == 99);
    // each level costs more than the last, and the final stretch far more
    for (int l = 2; l < 99; l++) CHECK(sim.db.xpForLevel(l) >= sim.db.xpForLevel(l - 1));
    CHECK(sim.db.xpForLevel(95) > 20 * sim.db.xpForLevel(40));
    // War Cry needs level 17
    sim.P.skills["strength"] = 1;
    CHECK_FALSE(sim.learnSkill("warcry"));
    int need = 0;
    for (int l = 1; l < 17; l++) need += sim.db.xpForLevel(l);
    sim.gainXp(need);
    REQUIRE(sim.P.level == 17);
    CHECK(sim.learnSkill("warcry"));
    sim.useAbility("warcry");
    CHECK(sim.hasBuff("frenzy"));
    CHECK(sim.abilityCooldown("warcry") > 0);
    // monsters far beneath you are worth little
    sim.P.level = 60;
    const int before = sim.P.xp;
    Map& m = sim.map();
    m.enemies.push_back(sim.makeEnemy(sim.db.enemy("boar"), sim.P.x + 200, sim.P.y, false));
    sim.damageEnemy(m.enemies.back(), 99, 0, 0);
    CHECK(sim.P.xp - before <= sim.db.enemy("boar").xp / 10);
}

TEST_CASE("regions: walled off, and their gates open as the bosses fall") {
    const Database& db = database();
    const Map over = generateOverworld(db);
    int gates = 0;
    for (const auto& o : over.objects) gates += o.kind == ObjKind::Gate && o.id == "zone_gate";
    CHECK(gates > 0);
    // with every gate shut, the hero cannot leave the regions open from the start
    const auto mask = reachMask(over, over.start.c, over.start.r);
    int leaks = 0;
    for (int i = 0; i < over.w * over.h; i++)
        if (mask[i] && !db.zones[over.zone[i]].gate.empty()) leaks++;
    CHECK(leaks == 0);
    // the first boss opens the way to the next regions
    Sim sim = freshGame();
    Map& m = sim.maps.at("over");
    int closed = 0;
    for (const auto& o : m.objects) closed += o.kind == ObjKind::Gate && o.id == "zone_gate" && !o.gone && o.res == "cellar";
    REQUIRE(closed > 0);
    sim.bossesDefeated.insert("cellar");
    sim.openZoneGates(false);
    for (const auto& o : m.objects)
        if (o.kind == ObjKind::Gate && o.id == "zone_gate" && o.res == "cellar") CHECK(o.gone);
    // and the opening survives a save
    Sim other(sim.db, 3);
    REQUIRE(other.loadGame(sim.saveGame()));
    for (const auto& o : other.maps.at("over").objects)
        if (o.kind == ObjKind::Gate && o.id == "zone_gate" && o.res == "cellar") CHECK(o.gone);
}

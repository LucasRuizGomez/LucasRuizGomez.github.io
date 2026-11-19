#include "game/defs.h"

#include <cmath>
#include <stdexcept>

namespace hc {

namespace {

ItemType parseItemType(const std::string& s) {
    static const std::map<std::string, ItemType> m = {
        {"mat", ItemType::Mat},       {"heal", ItemType::Heal},       {"weapon", ItemType::Weapon}, {"shield", ItemType::Shield},
        {"armor", ItemType::Armor},   {"trinket", ItemType::Trinket}, {"bow", ItemType::Bow},       {"ammo", ItemType::Ammo},
        {"tool", ItemType::Tool},     {"light", ItemType::Light},     {"key", ItemType::Key},       {"special", ItemType::Special},
        {"trophy", ItemType::Trophy}, {"mount", ItemType::Mount},     {"helm", ItemType::Helm},     {"boots", ItemType::Boots}};
    auto it = m.find(s);
    if (it == m.end()) throw std::runtime_error("Unknown item type: " + s);
    return it->second;
}

Ai parseAi(const std::string& s) {
    static const std::map<std::string, Ai> m = {{"charger", Ai::Charger}, {"chaser", Ai::Chaser}, {"flee", Ai::Flee}, {"flier", Ai::Flier},
                                                {"guard", Ai::Guard},     {"archer", Ai::Archer}, {"boss", Ai::Boss}};
    auto it = m.find(s);
    if (it == m.end()) throw std::runtime_error("Unknown enemy ai: " + s);
    return it->second;
}

Look parseLook(const std::string& s) {
    if (s == "animal") return Look::Animal;
    if (s == "human") return Look::Human;
    if (s == "treant") return Look::Treant;
    if (s == "mimic") return Look::Mimic;
    if (s == "static") return Look::Static;
    throw std::runtime_error("Unknown enemy look: " + s);
}

std::vector<std::pair<std::string, int>> parsePairs(const json& j) {
    std::vector<std::pair<std::string, int>> out;
    for (const auto& e : j) out.emplace_back(e.at(0).get<std::string>(), e.at(1).get<int>());
    return out;
}

std::vector<std::pair<std::string, double>> parseWeights(const json& j) {
    std::vector<std::pair<std::string, double>> out;
    if (j.is_array())
        for (const auto& e : j) out.emplace_back(e.at(0).get<std::string>(), e.at(1).get<double>());
    else
        for (auto it = j.begin(); it != j.end(); ++it) out.emplace_back(it.key(), it.value().get<double>());
    return out;
}

TilePos parsePos(const json& j) { return {j.at(0).get<int>(), j.at(1).get<int>()}; }

}   // namespace

Database Database::load(const std::string& dir) {
    Database db;

    // ── game.json ──
    {
        const json g = loadJson(dir + "game.json");
        Tuning& t = db.tuning;
        t.saveSlots = g.value("saveSlots", t.saveSlots);
        const json& w = g.at("world");
        t.worldW = w.at("width");
        t.worldH = w.at("height");
        t.worldSeed = w.at("seed");
        const json& p = g.at("player");
        t.playerSpeed = p.at("speed");
        t.rollTicks = p.at("rollTicks");
        t.rollCooldown = p.at("rollCooldown");
        t.attackTicks = p.at("attackTicks");
        t.inputBuffer = p.at("inputBuffer");
        t.baseHp = p.at("baseHp");
        t.invSlots = p.at("invSlots");
        t.hitInvuln = p.at("hitInvulnTicks");
        t.recallTicks = p.at("recallTicks");
        t.mountCastTicks = p.at("mountCastTicks");
        t.combatLockTicks = p.at("combatLockTicks");
        const json& f = g.at("fists");
        t.fistDmg = f.at("dmg");
        t.fistReach = f.at("reach");
        t.fistCd = f.at("cd");
        const json& l = g.at("leveling");
        t.maxLevel = l.at("maxLevel");
        t.xpBase = l.at("xpBase");
        t.xpGrowth = l.at("xpGrowth");
        t.heartEvery = l.value("heartEveryLevels", 0);
        t.skillPointsPerLevel = l.value("skillPointsPerLevel", 1);
        t.xpKillsBase = l.value("xpKillsBase", 0.0);
        t.xpKillsPerLevel = l.value("xpKillsPerLevel", 0.0);
        t.xpLateFrom = l.value("xpLateFrom", 9999);
        t.xpLateDiv = std::max(1.0, l.value("xpLateDiv", 100.0));
        t.skillPointEvery = std::max(1, l.value("skillPointEvery", 1));
        t.heartShards = std::max(1, l.value("heartShardsPerHeart", 3));
        t.damageEvery = l.at("damageEveryLevels");
        t.discoverZoneXp = l.at("discoverZoneXp");
        const json& wh = g.at("skills").at("whirlwind");
        t.whirlLevel = wh.value("level", 1);
        t.whirlCooldown = wh.at("cooldown");
        t.whirlRadius = wh.at("radius");
        t.whirlMul = wh.at("damageMul");
        t.dayLength = g.at("day").at("lengthTicks");
        t.startHour = g.at("day").at("startHour");
        const json& s = g.at("spawns");
        t.spawnActive = s.at("activeRadius");
        t.spawnDespawn = s.at("despawnRadius");
        t.respawnTicks = s.at("respawnTicks");
        t.eliteChance = s.at("eliteChance");
        t.globeChance = s.at("globeChance");
        t.shrineBuff = g.at("shrines").at("buffTicks");
        t.shrineRecharge = g.at("shrines").at("rechargeTicks");
        t.treeChops = g.at("trees").at("chops");
        t.stumpTicks = g.at("trees").at("stumpTicks");
        t.saplingTicks = g.at("trees").at("saplingTicks");
        t.goldLost = g.at("death").at("goldLostFraction");
        if (g.contains("rest")) t.restEnemyRadius = g.at("rest").value("enemyRadius", t.restEnemyRadius);
        t.autosaveTicks = g.at("autosaveTicks");
    }

    // ── items.json ──
    {
        const json j = loadJson(dir + "items.json");
        for (const auto& e : j.at("items")) {
            ItemDef d;
            d.id = e.at("id");
            d.name = e.at("name");
            d.desc = e.value("desc", "");
            d.type = parseItemType(e.at("type"));
            d.heal = e.value("heal", 0);
            d.dmg = e.value("dmg", 0);
            d.reach = e.value("reach", 0);
            d.cd = e.value("cd", 0);
            d.def = e.value("def", 0);
            d.hp = e.value("hp", 0);
            d.speed = e.value("speed", 0);
            d.tier = e.value("tier", 0.0);
            d.level = e.value("level", 1);
            d.value = e.value("value", 0);
            d.unique = e.value("unique", false);
            d.mountSprite = e.value("mountSprite", "");
            d.mountSpeed = e.value("mountSpeed", 1.0);
            d.mountTint = e.value("mountTint", "");
            d.doll = e.value("doll", "");
            d.mountFly = e.value("mountFly", false);
            db.itemIndex_[d.id] = db.items.size();
            db.items.push_back(std::move(d));
        }
        for (const auto& e : j.at("rarities")) {
            RarityDef r;
            r.id = e.at("id");
            r.name = e.at("name");
            r.color = hex(e.at("color"));
            r.glow = hex(e.at("glow"));
            r.affixes = e.at("affixes");
            r.weight = e.at("weight");
            r.valueMul = e.at("valueMul");
            db.rarities.push_back(std::move(r));
        }
        for (const auto& e : j.at("affixes")) {
            AffixDef a;
            a.id = e.at("id");
            a.label = e.at("label");
            a.stat = e.at("stat");
            a.text = e.at("text");
            a.min = e.at("min");
            a.max = e.at("max");
            for (const auto& s : e.at("slots")) a.slots.push_back(parseItemType(s));
            db.affixes.push_back(std::move(a));
        }
    }

    // ── recipes.json ──
    {
        const json j = loadJson(dir + "recipes.json");
        for (const auto& e : j.at("categories")) db.categories.push_back({e.at("id"), e.at("label")});
        for (const auto& e : j.at("recipes")) {
            RecipeDef r;
            r.out = e.at("out").at(0);
            r.outQty = e.at("out").at(1);
            r.in = parsePairs(e.at("in"));
            r.cat = e.at("cat");
            r.fire = e.value("station", "") == "fire";
            db.item(r.out);
            for (const auto& [id, q] : r.in) db.item(id);
            db.recipes.push_back(std::move(r));
        }
    }

    // ── enemies.json ──
    {
        const json j = loadJson(dir + "enemies.json");
        std::map<std::string, json> raw;
        for (const auto& e : j.at("enemies")) {
            json merged = e;
            if (e.contains("base")) {
                auto it = raw.find(e.at("base").get<std::string>());
                if (it == raw.end()) throw std::runtime_error("Enemy base not defined before use: " + e.at("base").get<std::string>());
                merged = it->second;
                merged.update(e);
            }
            raw[merged.at("id").get<std::string>()] = merged;
            EnemyDef d;
            d.id = merged.at("id");
            d.name = merged.at("name");
            d.sprite = merged.at("sprite");
            d.tag = merged.value("tag", d.id);
            d.tint = merged.value("tint", "");
            d.look = parseLook(merged.at("look"));
            d.ai = parseAi(merged.at("ai"));
            d.scale = merged.value("scale", 1.0);
            d.level = merged.value("level", 1);
            d.hp = merged.at("hp");
            d.dmg = merged.at("dmg");
            d.xp = merged.value("xp", 1);
            d.speed = merged.value("speed", 0.5);
            d.aggro = merged.value("aggro", 120.0);
            d.stalk = merged.value("stalk", 0.0);
            d.r = merged.at("r").get<double>() * (d.scale > 1 && !merged.contains("boss") ? (1 + (d.scale - 1) * 0.5) : 1.0);
            d.cy = merged.at("cy");
            d.hw = merged.at("hw");
            d.hh = merged.at("hh");
            d.fly = merged.value("fly", false);
            d.passive = merged.value("passive", false);
            if (merged.contains("drops"))
                for (const auto& dr : merged.at("drops")) d.drops.push_back({dr.at(0), dr.at(1), dr.at(2)});
            for (const auto& dr : d.drops) db.item(dr.item);
            if (merged.contains("gold")) {
                d.goldMin = merged.at("gold").at(0);
                d.goldMax = merged.at("gold").at(1);
                d.goldChance = merged.at("gold").at(2);
            }
            if (merged.contains("charge")) {
                const json& c = merged.at("charge");
                d.charge.range = c.at("range");
                d.charge.every = c.at("every");
                d.charge.wind = c.at("wind");
                d.charge.speed = c.at("speed");
            }
            if (merged.contains("boss")) {
                const json& b = merged.at("boss");
                d.boss.speed = b.value("speed", 0.55);
                d.boss.slam = b.value("slam", 4);
                d.boss.slamRadius = b.value("slamRadius", 42.0);
                d.boss.volley = b.value("volley", 12);
                d.boss.summon = b.value("summon", "");
                d.boss.roomTitle = b.value("roomTitle", "");
                d.boss.final = b.value("final", false);
            }
            db.enemyIndex_[d.id] = db.enemies.size();
            db.enemies.push_back(std::move(d));
        }
        for (const auto& e : j.at("elites")) {
            EliteDef d;
            d.id = e.at("id");
            d.label = e.at("label");
            d.glow = hex(e.at("glow"));
            d.speedMul = e.value("speedMul", 1.0);
            d.dmgAdd = e.value("dmgAdd", 0);
            d.healOnHit = e.value("healOnHit", 0);
            d.armor = e.value("armor", 0);
            db.elites.push_back(std::move(d));
        }
    }

    // ── world.json ──
    {
        const json j = loadJson(dir + "world.json");
        for (const auto& e : j.at("zones")) {
            ZoneDef z;
            z.id = e.at("id");
            z.name = e.at("name");
            z.index = static_cast<int>(db.zones.size());
            z.levelMin = e.at("levels").at(0);
            z.levelMax = e.at("levels").at(1);
            for (const auto& c : e.at("centers")) z.centers.push_back(parsePos(c));
            z.water = e.at("water");
            z.trees = e.at("trees");
            z.rocks = e.at("rocks");
            z.flowers = e.at("flowers");
            z.tree = e.at("tree");
            z.gate = e.value("gate", "");
            z.sandy = e.value("sandy", false);
            z.snow = e.value("snow", false);
            z.ground = hex(e.at("ground"));
            z.foliage = hex(e.at("foliage"));
            z.music = e.at("music");
            z.density = e.at("density");
            z.spawns = parseWeights(e.at("spawns"));
            z.nodes = parseWeights(e.at("nodes"));
            for (const auto& [id, w] : z.spawns) db.enemy(id);
            db.zones.push_back(std::move(z));
        }
        for (auto it = j.at("pois").begin(); it != j.at("pois").end(); ++it) db.pois[it.key()] = it.value();
        for (auto it = j.at("props").begin(); it != j.at("props").end(); ++it) {
            PropDef p;
            p.id = it.key();
            p.sprite = it.value().at("sprite");
            p.w = it.value().value("w", 1);
            p.h = it.value().value("h", 1);
            p.frames = it.value().value("frames", 1);
            p.solid = it.value().value("solid", true);
            p.flat = it.value().value("flat", false);
            p.ox = it.value().value("ox", 0);
            p.oy = it.value().value("oy", 0);
            p.arch = it.value().value("arch", false);
            db.props[p.id] = std::move(p);
        }
        for (auto it = j.at("shops").begin(); it != j.at("shops").end(); ++it) {
            ShopDef s;
            s.id = it.key();
            s.title = it.value().at("title");
            for (const auto& b : it.value().at("buy")) {
                s.buy.push_back({b.at(0), b.at(1), b.at(2)});
                db.item(s.buy.back().item);
            }
            for (auto se = it.value().at("sell").begin(); se != it.value().at("sell").end(); ++se) s.sell[se.key()] = se.value();
            db.shops[s.id] = std::move(s);
        }
        auto readNpc = [&](const json& n, const std::string& town) {
            NpcDef d;
            d.id = n.at("id");
            d.name = n.at("name");
            d.sprite = n.at("sprite");
            d.tint = n.value("tint", "");
            d.role = n.value("role", "villager");
            d.shop = n.value("shop", "");
            d.text = n.value("text", "");
            d.town = town;
            if (n.contains("lines"))
                for (const auto& l : n.at("lines")) d.lines.push_back(l);
            if (d.text.empty() && !d.lines.empty()) d.text = d.lines.front();
            if (n.contains("at")) d.at = parsePos(n.at("at"));
            if (!d.shop.empty() && !db.shops.count(d.shop)) throw std::runtime_error("NPC " + d.id + " uses unknown shop " + d.shop);
            if (db.npcs_.count(d.id)) throw std::runtime_error("Two NPCs share the id " + d.id);
            return d;
        };
        for (const auto& e : j.at("towns")) {
            TownDef t;
            t.id = e.at("id");
            t.name = e.at("name");
            t.at = parsePos(e.at("at"));
            t.radius = e.at("radius");
            t.houses = e.value("houses", 2);
            t.stalls = e.value("stalls", 0);
            t.zone = e.at("zone");
            t.music = e.at("music");
            for (const auto& f : e.at("features")) t.features.push_back(f);
            for (const auto& n : e.at("npcs")) {
                NpcDef d = readNpc(n, t.id);
                db.npcs_[d.id] = d;
                t.npcs.push_back(std::move(d));
            }
            db.towns.push_back(std::move(t));
        }

        // ── buildings.json: what is inside the houses, and who lives there ──
        const json b = loadJson(dir + "buildings.json");
        auto places = [](const json& arr) {
            std::vector<PropPlace> out;
            for (const auto& p : arr) out.push_back({p.at(0).get<std::string>(), p.at(1).get<int>(), p.at(2).get<int>()});
            return out;
        };
        for (auto it = b.at("kinds").begin(); it != b.at("kinds").end(); ++it) {
            InteriorDef k;
            k.id = it.key();
            const json& e = it.value();
            k.label = e.value("label", k.id);
            k.w = e.at("size").at(0);
            k.h = e.at("size").at(1);
            k.floor = e.value("floor", k.floor);
            k.wall = e.value("wall", k.wall);
            k.props = places(e.at("props"));
            if (e.contains("hearth")) k.hearth = parsePos(e.at("hearth"));
            k.hearthSprite = e.value("hearthSprite", k.hearthSprite);
            for (const auto& p : k.props)
                if (!db.props.count(p.prop)) throw std::runtime_error("Interior " + k.id + " uses unknown prop " + p.prop);
            db.interiors[k.id] = std::move(k);
        }
        for (const auto& e : b.at("buildings")) {
            BuildingDef d;
            d.id = e.at("id");
            d.town = e.at("town");
            d.kind = e.at("kind");
            d.name = e.at("name");
            d.sign = e.value("sign", "");
            d.floor = e.value("floor", "");
            d.wall = e.value("wall", "");
            if (e.contains("props")) d.props = places(e.at("props"));
            if (e.contains("remove"))
                for (const auto& r : e.at("remove")) d.remove.push_back(r);
            if (!db.interiors.count(d.kind)) throw std::runtime_error("Building " + d.id + " has unknown kind " + d.kind);
            for (const auto& p : d.props)
                if (!db.props.count(p.prop)) throw std::runtime_error("Building " + d.id + " uses unknown prop " + p.prop);
            TownDef* town = nullptr;
            for (auto& t : db.towns)
                if (t.id == d.town) town = &t;
            if (!town) throw std::runtime_error("Building " + d.id + " is in unknown town " + d.town);
            if (e.contains("npcs"))
                for (const auto& n : e.at("npcs")) {
                    NpcDef nd = readNpc(n, d.town);
                    nd.building = d.id;
                    d.npcs.push_back(nd.id);
                    db.npcs_[nd.id] = nd;
                }
            town->buildings.push_back(d.id);
            db.buildings.push_back(std::move(d));
        }
        for (auto& t : db.towns) t.houses = static_cast<int>(t.buildings.size());
        for (const auto& e : j.at("dungeons")) {
            DungeonDef d;
            d.id = e.at("id");
            d.name = e.at("name");
            d.kind = e.at("kind");
            d.under = e.value("under", "");
            d.entrance = e.value("entrance", "door");
            if (e.contains("at")) d.at = parsePos(e.at("at"));
            d.level = e.at("level");
            d.w = e.at("size").at(0);
            d.h = e.at("size").at(1);
            d.rooms = e.value("rooms", 8);
            d.seed = e.value("seed", 1u);
            d.tint = e.value("tint", "");
            d.music = e.at("music");
            d.boss = e.value("boss", "");
            d.traps = e.value("traps", 0);
            if (e.contains("enemies"))
                for (const auto& en : e.at("enemies")) d.enemies.push_back(en);
            if (e.contains("reward")) d.reward = parsePairs(e.at("reward"));
            for (const auto& en : d.enemies) db.enemy(en);
            if (!d.boss.empty()) db.enemy(d.boss);
            for (const auto& [id, q] : d.reward) db.item(id);
            db.dungeons.push_back(std::move(d));
        }
    }

    // ── quests.json ──
    {
        const json j = loadJson(dir + "quests.json");
        for (const auto& e : j.at("quests")) {
            QuestDef q;
            q.id = e.at("id");
            q.title = e.at("title");
            q.giver = e.at("giver");
            q.turnIn = e.value("turnIn", q.giver);
            q.offer = e.at("offer");
            q.progress = e.value("progress", "");
            q.done = e.value("done", "");
            q.main = e.value("main", false);
            if (e.contains("requires"))
                for (const auto& r : e.at("requires")) q.requires_.push_back(r);
            for (const auto& o : e.at("objectives")) {
                QuestObjective ob;
                ob.type = o.at("type");
                ob.item = o.value("item", "");
                ob.target = o.value("target", "");
                ob.label = o.value("label", "");
                ob.count = o.value("count", 1);
                ob.anyWeapon = o.value("anyWeapon", false);
                if (!ob.item.empty()) db.item(ob.item);
                q.objectives.push_back(std::move(ob));
            }
            const json& r = e.at("rewards");
            q.xp = r.value("xp", 0);
            q.gold = r.value("gold", 0);
            q.rewardRarity = r.value("rarity", 0);
            if (r.contains("items")) q.items = parsePairs(r.at("items"));
            if (r.contains("choice")) q.choice = parsePairs(r.at("choice"));
            for (const auto& [id, n] : q.items) db.item(id);
            for (const auto& [id, n] : q.choice) db.item(id);
            q.objectiveText = e.value("summary", "");
            q.level = e.value("level", 0);
            if (!q.level)   // by default, the lowest level of the region the quest giver lives in
                if (const NpcDef* g = db.npc(q.giver))
                    if (const TownDef* t = db.town(g->town))
                        if (const ZoneDef* z = db.zone(t->zone)) q.level = z->levelMin;
            q.level = std::max(1, q.level);
            if (!db.npc(q.giver)) throw std::runtime_error("Quest " + q.id + " has unknown giver " + q.giver);
            if (!db.npc(q.turnIn)) throw std::runtime_error("Quest " + q.id + " has unknown turn-in NPC " + q.turnIn);
            db.questIndex_[q.id] = db.quests.size();
            db.quests.push_back(std::move(q));
        }
        for (const auto& q : db.quests)
            for (const auto& r : q.requires_)
                if (!db.quest(r)) throw std::runtime_error("Quest " + q.id + " requires unknown quest " + r);
    }

    // ── controls.json ──
    {
        const json j = loadJson(dir + "controls.json");
        for (const auto& e : j.at("actions")) {
            ActionDef a;
            a.id = e.at("id");
            a.label = e.at("label");
            for (const auto& k : e.at("keys")) a.keys.push_back(k);
            db.actions.push_back(std::move(a));
        }
        for (auto it = j.at("gamepad").begin(); it != j.at("gamepad").end(); ++it) db.gamepad[it.key()] = it.value();
        db.defaultSfx = j.at("defaults").at("sfx");
        db.defaultMusic = j.at("defaults").at("music");
        db.defaultUi = j.at("defaults").at("ui");
    }

    // ── skills.json ──
    {
        const json j = loadJson(dir + "skills.json");
        for (const auto& e : j.at("branches")) db.branches.push_back({e.at("id"), e.at("label"), e.value("desc", ""), hex(e.value("color", "#c8a060"))});
        for (const auto& e : j.at("skills")) {
            SkillDef s;
            s.id = e.at("id");
            s.name = e.at("name");
            s.branch = e.at("branch");
            s.desc = e.value("desc", "");
            s.maxRank = std::max(1, e.value("ranks", 1));
            s.level = e.value("level", 1);
            s.col = e.at("pos").at(0);
            s.row = e.at("pos").at(1);
            if (e.contains("requires")) {
                s.requires_ = e.at("requires").at(0);
                s.requiresRank = e.at("requires").at(1);
            }
            s.effects = parseWeights(e.at("effects"));
            s.icon = e.value("icon", "");
            bool known = false;
            for (const auto& b : db.branches) known = known || b.id == s.branch;
            if (!known) throw std::runtime_error("Skill " + s.id + " is in unknown branch " + s.branch);
            db.skills.push_back(std::move(s));
        }
        for (const auto& s : db.skills)
            if (!s.requires_.empty() && !db.skill(s.requires_)) throw std::runtime_error("Skill " + s.id + " requires unknown skill " + s.requires_);
    }

    db.audio = loadJson(dir + "audio.json");
    db.sprites = loadJson(dir + "sprites.json");
    return db;
}

const ItemDef* Database::findItem(const std::string& id) const {
    auto it = itemIndex_.find(id);
    return it == itemIndex_.end() ? nullptr : &items[it->second];
}
const ItemDef& Database::item(const std::string& id) const {
    if (const ItemDef* d = findItem(id)) return *d;
    throw std::runtime_error("Unknown item: " + id);
}
const EnemyDef& Database::enemy(const std::string& id) const {
    auto it = enemyIndex_.find(id);
    if (it == enemyIndex_.end()) throw std::runtime_error("Unknown enemy: " + id);
    return enemies[it->second];
}
const ZoneDef* Database::zone(const std::string& id) const {
    for (const auto& z : zones)
        if (z.id == id) return &z;
    return nullptr;
}
const TownDef* Database::town(const std::string& id) const {
    for (const auto& t : towns)
        if (t.id == id) return &t;
    return nullptr;
}
const DungeonDef* Database::dungeon(const std::string& id) const {
    for (const auto& d : dungeons)
        if (d.id == id) return &d;
    return nullptr;
}
const QuestDef* Database::quest(const std::string& id) const {
    auto it = questIndex_.find(id);
    return it == questIndex_.end() ? nullptr : &quests[it->second];
}
const ShopDef* Database::shop(const std::string& id) const {
    auto it = shops.find(id);
    return it == shops.end() ? nullptr : &it->second;
}
const NpcDef* Database::npc(const std::string& id) const {
    auto it = npcs_.find(id);
    return it == npcs_.end() ? nullptr : &it->second;
}
const PropDef* Database::prop(const std::string& id) const {
    auto it = props.find(id);
    return it == props.end() ? nullptr : &it->second;
}
const AffixDef* Database::affix(const std::string& id) const {
    for (const auto& a : affixes)
        if (a.id == id) return &a;
    return nullptr;
}
const BuildingDef* Database::building(const std::string& id) const {
    for (const auto& b : buildings)
        if (b.id == id) return &b;
    return nullptr;
}
const InteriorDef* Database::interior(const std::string& kind) const {
    auto it = interiors.find(kind);
    return it == interiors.end() ? nullptr : &it->second;
}
const SkillDef* Database::skill(const std::string& id) const {
    for (const auto& s : skills)
        if (s.id == id) return &s;
    return nullptr;
}
int Database::xpForLevel(int level) const {
    const double late = std::max(0, level - tuning.xpLateFrom), lateMul = 1 + late * late / tuning.xpLateDiv;
    if (tuning.xpKillsBase > 0)   // kills of monsters your own level, each worth 45 + 5 * level
        return static_cast<int>(std::floor((45 + 5.0 * level) * (tuning.xpKillsBase + tuning.xpKillsPerLevel * level) * lateMul));
    return static_cast<int>(std::floor(tuning.xpBase * std::pow(tuning.xpGrowth, level - 1) * lateMul));
}

}   // namespace hc

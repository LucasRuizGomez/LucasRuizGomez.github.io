// Save games. The world itself is rebuilt from its seed; a save only stores what the player changed:
// the hero, quests, opened chests, felled trees, mined rocks, broken pots, explored ground.
#include "game/sim.h"

namespace hc {

namespace {

const int SAVE_VERSION = 3;

json itemToJson(const Item& it) {
    json j = {{"id", it.id}, {"qty", it.qty}};
    if (it.uid) j["uid"] = it.uid;
    if (it.rarity) j["rarity"] = it.rarity;
    if (!it.affixes.empty()) {
        j["affixes"] = json::array();
        for (const auto& [id, v] : it.affixes) j["affixes"].push_back({id, v});
    }
    return j;
}

bool itemFromJson(const Database& db, const json& j, Item& out) {
    out = Item();
    out.id = j.value("id", "");
    if (!db.findItem(out.id)) return false;   // item removed from the game since this save was made
    out.qty = j.value("qty", 1);
    out.uid = j.value("uid", 0);
    out.rarity = std::clamp(j.value("rarity", 0), 0, static_cast<int>(db.rarities.size()) - 1);
    if (j.contains("affixes"))
        for (const auto& a : j.at("affixes"))
            if (db.affix(a.at(0).get<std::string>())) out.affixes.emplace_back(a.at(0).get<std::string>(), a.at(1).get<int>());
    return true;
}

// Run-length encoding of a 0/1 grid: alternating run lengths, starting with zeros.
json rle(const std::vector<uint8_t>& v) {
    json out = json::array();
    uint8_t cur = 0;
    int run = 0;
    for (uint8_t b : v) {
        const uint8_t bit = b ? 1 : 0;
        if (bit == cur) run++;
        else {
            out.push_back(run);
            cur = bit;
            run = 1;
        }
    }
    out.push_back(run);
    return out;
}

void unrle(const json& j, std::vector<uint8_t>& v) {
    size_t i = 0;
    uint8_t cur = 0;
    for (const auto& run : j) {
        for (int k = run.get<int>(); k > 0 && i < v.size(); k--) v[i++] = cur;
        cur = cur ? 0 : 1;
    }
}

}   // namespace

void Sim::prepareTitle() {
    if (!worldReady) buildWorld();
    mapId = "over";
    mode = Mode::Title;
    fade.reset();
}

json Sim::saveSummary() const {
    std::string place = map().name;
    if (map().outdoor) {
        const TownDef* t = db.town(townId);
        const ZoneDef* z = db.zone(zoneId);
        place = t ? t->name : z ? z->name : place;
    }
    return {{"name", P.name}, {"level", P.level}, {"place", place}, {"time", time}, {"day", time / db.tuning.dayLength + 1}, {"deaths", deaths}, {"victory", victorySeen}};
}

json Sim::saveGame() const {
    json s;
    s["v"] = SAVE_VERSION;
    s["summary"] = saveSummary();
    s["map"] = mapId;
    json& p = s["player"];
    p = {{"name", P.name}, {"x", P.x}, {"y", P.y}, {"hp", P.hp}, {"level", P.level}, {"xp", P.xp}, {"hearts", P.heartCrystals}, {"lastTown", P.lastTown}, {"mount", P.mount}};
    p["equip"] = {P.equip[0], P.equip[1], P.equip[2], P.equip[3]};
    p["inv"] = json::array();
    for (const auto& it : P.inv) p["inv"].push_back(itemToJson(it));
    p["buffs"] = json::array();
    for (const auto& b : P.buffs) p["buffs"].push_back({b.id, b.t});
    s["time"] = time;
    s["deaths"] = deaths;
    s["doorOpen"] = doorOpen;
    s["victory"] = victorySeen;
    s["bosses"] = bossesDefeated;
    s["zones"] = zonesFound;
    s["waypoints"] = waypoints;
    s["shrines"] = shrinesFound;
    json& q = s["quests"];
    q = json::object();
    for (const auto& [id, st] : quests) q[id] = {{"s", st.state}, {"c", st.counts}};

    json& w = s["world"];
    w = json::object();
    for (const auto& [id, m] : maps) {
        json jm = json::object();
        json chests = json::object(), stumps = json::array(), nodes = json::array(), pots = json::array(), shrines = json::array();
        int nodeIndex = 0;
        std::set<std::pair<int, int>> potsAlive;
        for (const auto& o : m.objects) {
            if (o.kind == ObjKind::Chest && (o.opened || o.openAt >= 0)) {
                json loot = json::array();
                for (const auto& it : o.loot) loot.push_back(itemToJson(it));
                chests[o.id] = {{"opened", o.opened}, {"loot", loot}};
            } else if (o.kind == ObjKind::Stump && !o.gone) stumps.push_back({o.c, o.r, o.stage, std::max(0, o.at - tickCount)});
            else if (o.kind == ObjKind::Node) {
                if (o.left < o.max) nodes.push_back({nodeIndex, o.left, std::max(0, o.respawn - tickCount)});
                nodeIndex++;
            } else if (o.kind == ObjKind::Shrine && o.at > tickCount) shrines.push_back({o.c, o.r, o.at - tickCount});
            else if (o.kind == ObjKind::Pot && o.gone) pots.push_back({o.c, o.r});
        }
        for (const auto& p : brokenPots_)
            if (p.first == id) pots.push_back({p.second.c, p.second.r});
        if (!chests.empty()) jm["chests"] = chests;
        if (!stumps.empty()) jm["stumps"] = stumps;
        if (!nodes.empty()) jm["nodes"] = nodes;
        if (!pots.empty()) jm["pots"] = pots;
        if (!shrines.empty()) jm["shrines"] = shrines;
        if (m.outdoor) jm["explored"] = rle(m.explored);
        if (!jm.empty()) w[id] = jm;
    }
    return s;
}

bool Sim::loadGame(const json& s) {
    if (!s.is_object() || s.value("v", 0) != SAVE_VERSION || !s.contains("player")) return false;
    const std::string wantMap = s.value("map", "over");
    buildWorld();
    if (!maps.count(wantMap)) return false;

    P = Player();
    const json& p = s.at("player");
    P.name = p.value("name", "Wanderer");
    P.level = std::clamp(p.value("level", 1), 1, db.tuning.maxLevel);
    P.xp = p.value("xp", 0);
    P.heartCrystals = p.value("hearts", 0);
    P.lastTown = p.value("lastTown", "greenhollow");
    if (!db.town(P.lastTown)) P.lastTown = db.towns.front().id;
    nextUid_ = 1;
    for (const auto& ji : p.at("inv")) {
        Item it;
        if (itemFromJson(db, ji, it)) {
            if (!db.item(it.id).stacks() && !it.uid) it.uid = nextUid_;
            nextUid_ = std::max(nextUid_, it.uid + 1);
            P.inv.push_back(it);
        }
    }
    for (int i = 0; i < 4; i++) {
        P.equip[i] = p.at("equip").at(i).get<int>();
        if (!findUid(P.equip[i])) P.equip[i] = 0;
    }
    if (p.contains("buffs"))
        for (const auto& b : p.at("buffs")) P.buffs.push_back({b.at(0).get<std::string>(), b.at(1).get<int>()});
    const std::string mount = p.value("mount", "");
    P.hp = std::clamp(p.value("hp", 2), 2, maxHp());

    tickCount = 0;
    time = s.value("time", 0);
    deaths = s.value("deaths", 0);
    doorOpen = s.value("doorOpen", false);
    victorySeen = s.value("victory", false);
    bossesDefeated = s.value("bosses", std::set<std::string>{});
    zonesFound = s.value("zones", std::set<std::string>{});
    waypoints = s.value("waypoints", std::set<std::string>{});
    shrinesFound = s.value("shrines", std::set<std::string>{});
    quests.clear();
    if (s.contains("quests"))
        for (auto it = s.at("quests").begin(); it != s.at("quests").end(); ++it) {
            const QuestDef* q = db.quest(it.key());
            if (!q) continue;
            QuestState st;
            st.state = it.value().value("s", 0);
            st.counts = it.value().value("c", std::vector<int>{});
            st.counts.resize(q->objectives.size(), 0);
            quests[it.key()] = st;
        }
    brokenPots_.clear();
    nightPack = false;
    victoryAt = hitstop = 0;
    gateAt = -999;
    toasts.clear();
    floaters.clear();
    particles.clear();
    banner.reset();
    applyWorldFlags();

    if (s.contains("world"))
        for (auto wi = s.at("world").begin(); wi != s.at("world").end(); ++wi) {
            auto mi = maps.find(wi.key());
            if (mi == maps.end()) continue;
            Map& m = mi->second;
            const json& jm = wi.value();
            if (jm.contains("chests"))
                for (auto& o : m.objects) {
                    if (o.kind != ObjKind::Chest || !jm.at("chests").contains(o.id)) continue;
                    const json& jc = jm.at("chests").at(o.id);
                    o.opened = jc.value("opened", true);
                    o.openAt = 0;
                    o.loot.clear();
                    for (const auto& ji : jc.at("loot")) {
                        Item it;
                        if (itemFromJson(db, ji, it)) o.loot.push_back(it);
                    }
                    if (o.mimic && o.opened) m.setBlock(o, false);
                }
            if (jm.contains("stumps"))
                for (const auto& st : jm.at("stumps")) {
                    const int c = st.at(0), r = st.at(1);
                    if (m.get(c, r) != TREE) continue;
                    m.set(c, r, GRASS);
                    Object o;
                    o.kind = ObjKind::Stump;
                    o.c = c;
                    o.r = r;
                    o.stage = st.at(2);
                    o.at = st.at(3).get<int>();
                    if (o.stage) m.block[m.idx(c, r)] = 1;
                    m.objects.push_back(o);
                }
            if (jm.contains("nodes")) {
                std::vector<Object*> nodes;
                for (auto& o : m.objects)
                    if (o.kind == ObjKind::Node) nodes.push_back(&o);
                for (const auto& n : jm.at("nodes")) {
                    const size_t i = n.at(0).get<size_t>();
                    if (i >= nodes.size()) continue;
                    nodes[i]->left = n.at(1);
                    if (nodes[i]->left <= 0) {
                        nodes[i]->respawn = n.at(2).get<int>();
                        m.setBlock(*nodes[i], false);
                    }
                }
            }
            if (jm.contains("pots"))
                for (const auto& jp : jm.at("pots")) {
                    const int c = jp.at(0), r = jp.at(1);
                    for (auto& o : m.objects)
                        if (o.kind == ObjKind::Pot && o.c == c && o.r == r) {
                            o.gone = true;
                            m.setBlock(o, false);
                        }
                    brokenPots_.push_back({m.id, {c, r}});
                }
            std::erase_if(m.objects, [](const Object& o) { return o.gone; });
            if (jm.contains("shrines"))
                for (const auto& js : jm.at("shrines"))
                    for (auto& o : m.objects)
                        if (o.kind == ObjKind::Shrine && o.c == js.at(0).get<int>() && o.r == js.at(1).get<int>()) o.at = js.at(2).get<int>();
            if (jm.contains("explored") && !m.explored.empty()) unrle(jm.at("explored"), m.explored);
            m.exploredDirty = true;
        }

    enterMap(wantMap, p.value("x", 0.0), p.value("y", 0.0));
    if (!map().boxFree(P.x, P.y, 4, 3, false)) {   // the world changed under an old save: fall back to the map's entrance
        const TilePos st = map().start;
        P.x = st.c * TILE + 8;
        P.y = st.r * TILE + 10;
        updateCamera(true);
    }
    if (!mount.empty() && db.findItem(mount) && hasItem(mount) && map().outdoor) P.mount = mount;
    mode = Mode::Play;
    return true;
}

}   // namespace hc

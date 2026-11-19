// Random gear: rarity rolls, affixes, and drops on the ground.
#include "game/sim.h"

#include <cmath>

namespace hc {

Item Sim::makeGear(const std::string& id, int minRarity, double luck) {
    const ItemDef& def = db.item(id);
    Item it{id, 1};
    if (!def.isGear()) return it;
    if (hasBuff("fortune")) luck *= 2;
    double total = 0;
    for (size_t i = 0; i < db.rarities.size(); i++) total += db.rarities[i].weight * (i ? luck : 1);
    double roll = rng.next() * total;
    int rarity = 0;
    for (size_t i = 0; i < db.rarities.size(); i++) {
        const double w = db.rarities[i].weight * (i ? luck : 1);
        if (roll < w) {
            rarity = static_cast<int>(i);
            break;
        }
        roll -= w;
    }
    it.rarity = std::clamp(std::max(rarity, minRarity), 0, static_cast<int>(db.rarities.size()) - 1);
    std::vector<const AffixDef*> pool;
    for (const auto& a : db.affixes)
        if (std::find(a.slots.begin(), a.slots.end(), def.type) != a.slots.end()) pool.push_back(&a);
    for (int n = db.rarities[it.rarity].affixes; n > 0 && !pool.empty(); n--) {
        const size_t k = static_cast<size_t>(rng.range(static_cast<int>(pool.size())));
        it.affixes.emplace_back(pool[k]->id, rng.between(pool[k]->min, pool[k]->max));
        pool.erase(pool.begin() + k);
    }
    return it;
}

// The map you are on (or the one a dungeon opens onto): its own gear only drops there
std::string Sim::currentRealm() const {
    const Map& m = map();
    if (m.outdoor) return m.id;
    const DungeonDef* d = m.dungeon;
    while (d && !d->under.empty()) d = db.dungeon(d->under);
    return d ? d->realm : "over";
}

Item Sim::randomGear(int level, int minRarity) {
    const std::string realm = currentRealm();
    std::vector<const ItemDef*> pool;
    auto allowed = [&](const ItemDef& d) { return d.isGear() && (d.realm.empty() || d.realm == realm); };
    for (const auto& d : db.items)
        if (allowed(d) && d.level <= level + 8 && d.level >= level - 32 && d.id != "kings_edge") {
            pool.push_back(&d);
            if (!d.realm.empty()) pool.push_back(&d);   // a map's own gear turns up twice as often there
        }
    if (pool.empty())
        for (const auto& d : db.items)
            if (allowed(d) && d.level <= level + 8) pool.push_back(&d);
    if (pool.empty()) return {"bandage", 1};
    // a little more luck deeper in the world, but fine and rare gear stays rare
    return makeGear(pool[rng.range(static_cast<int>(pool.size()))]->id, minRarity, 1.0 + std::min(level, 99) * 0.012);
}

void Sim::dropItem(Map& m, Item item, double x, double y) {
    const double a = rng.next() * PI * 2;
    Drop d;
    d.item = std::move(item);
    d.x = x;
    d.y = y;
    d.vx = std::cos(a) * 0.9;
    d.vy = std::sin(a) * 0.9;
    d.vz = 2.2;
    m.drops.push_back(std::move(d));
}

}   // namespace hc

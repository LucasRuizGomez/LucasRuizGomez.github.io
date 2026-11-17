// Bag, equipment, derived stats, crafting and trading.
#include "game/sim.h"

#include <cmath>

namespace hc {

namespace {

std::string hearts(int halves) { return halves % 2 ? std::to_string(halves / 2) + ".5" : std::to_string(halves / 2); }

std::string fill(std::string text, int v) {
    for (const char* key : {"{v}", "{h}"}) {
        const size_t p = text.find(key);
        if (p != std::string::npos) text.replace(p, 3, key[1] == 'h' ? hearts(v) : std::to_string(v));
    }
    return text;
}

}   // namespace

EquipSlot Sim::slotFor(ItemType t) {
    switch (t) {
        case ItemType::Shield: return EquipSlot::Shield;
        case ItemType::Armor: return EquipSlot::Armor;
        case ItemType::Trinket: return EquipSlot::Trinket;
        default: return EquipSlot::Weapon;
    }
}

Item* Sim::findUid(int uid) {
    if (!uid) return nullptr;
    for (auto& it : P.inv)
        if (it.uid == uid) return &it;
    return nullptr;
}
const Item* Sim::findUid(int uid) const { return const_cast<Sim*>(this)->findUid(uid); }

int Sim::countItem(const std::string& id) const {
    int n = 0;
    for (const auto& it : P.inv)
        if (it.id == id) n += it.qty;
    return n;
}

const Item* Sim::equipped(EquipSlot s) const { return findUid(P.equip[static_cast<int>(s)]); }

bool Sim::isEquipped(const Item& it) const {
    if (!it.uid) return false;
    for (int uid : P.equip)
        if (uid == it.uid) return true;
    return false;
}

bool Sim::addItem(Item item, bool quiet) {
    const ItemDef& def = db.item(item.id);
    if (def.type == ItemType::Special && item.id == "heart") {
        P.heartCrystals++;
        P.hp = maxHp();
        if (!quiet) {
            showBanner("heart", "Heart Crystal", "Maximum health +1 heart");
            play("heal");
        }
        return true;
    }
    if (def.stacks()) {
        for (auto& it : P.inv)
            if (it.id == item.id) {
                it.qty += item.qty;
                return true;
            }
    }
    if (static_cast<int>(P.inv.size()) >= db.tuning.invSlots) {
        if (!quiet) toast("Your bag is full.");
        return false;
    }
    if (!def.stacks()) {
        item.qty = 1;
        if (!item.uid) item.uid = nextUid_++;
        nextUid_ = std::max(nextUid_, item.uid + 1);
    }
    P.inv.push_back(item);
    if (!quiet) autoEquip(P.inv.back());
    return true;
}

bool Sim::removeItem(const std::string& id, int qty) {
    if (countItem(id) < qty) return false;
    for (size_t i = 0; i < P.inv.size() && qty > 0;) {
        Item& it = P.inv[i];
        if (it.id != id) {
            i++;
            continue;
        }
        const int take = std::min(qty, it.qty);
        it.qty -= take;
        qty -= take;
        if (it.qty <= 0) {
            for (int& uid : P.equip)
                if (uid && uid == it.uid) uid = 0;
            P.inv.erase(P.inv.begin() + i);
        } else i++;
    }
    P.hp = std::min(P.hp, maxHp());
    return true;
}

void Sim::autoEquip(const Item& it) {
    const ItemDef& def = db.item(it.id);
    auto score = [&](const Item& x) {
        const ItemDef& d = db.item(x.id);
        double s = d.tier + x.rarity * 0.15;
        for (const auto& [id, v] : x.affixes) s += v * 0.02;
        return s;
    };
    if (def.isGear()) {
        const EquipSlot slot = slotFor(def.type);
        const Item* cur = equipped(slot);
        if (cur && score(*cur) >= score(it)) return;
        P.equip[static_cast<int>(slot)] = it.uid;
        const Color col = db.rarities[std::clamp(it.rarity, 0, static_cast<int>(db.rarities.size()) - 1)].color;
        const auto lines = itemLines(it);
        showBanner(it.id, itemName(it), "Equipped" + (lines.empty() ? std::string() : " · " + lines.front()), it.rarity ? col : Color(138, 90, 12));
        play("equip");
        return;
    }
    if (countItem(it.id) != 1) return;
    switch (def.type) {
        case ItemType::Bow: showBanner(it.id, def.name, "Fire bolts with the crossbow key, or right click"); break;
        case ItemType::Tool: showBanner(it.id, def.name, "You can now mine iron and coal"); break;
        case ItemType::Light: showBanner(it.id, def.name, "You see further in the dark"); break;
        case ItemType::Key: showBanner(it.id, def.name, "Opens the sealed gate"); break;
        case ItemType::Mount: showBanner(it.id, def.name, "Press the mount key outdoors to ride"); break;
        default: break;
    }
}

void Sim::useOrEquip(int bagIndex) {
    if (bagIndex < 0 || bagIndex >= static_cast<int>(P.inv.size())) return;
    const Item it = P.inv[bagIndex];
    const ItemDef& def = db.item(it.id);
    if (def.isGear()) {
        int& slot = P.equip[static_cast<int>(slotFor(def.type))];
        const bool on = slot != it.uid;
        slot = on ? it.uid : 0;
        P.hp = std::min(P.hp, maxHp());
        play("equip");
        toast((on ? "Equipped " : "Unequipped ") + itemName(it) + ".");
    } else if (def.type == ItemType::Heal) useHeal(it.id);
    else if (def.type == ItemType::Mount) {
        mode = Mode::Play;
        if (P.mount == it.id) dismount();
        else {
            P.mount.clear();
            wantMount_ = true;
        }
    } else {
        play("deny");
        toast(def.type == ItemType::Mat ? "Used for crafting and trading." : def.desc);
    }
}

bool Sim::useHeal(const std::string& id) {
    if (P.hp >= maxHp()) {
        play("deny");
        toast("Your health is already full.");
        return false;
    }
    if (!hasItem(id)) return false;
    const int h = std::min(db.item(id).heal, maxHp() - P.hp);
    removeItem(id, 1);
    P.hp += h;
    play("heal");
    floater(P.x, P.y - 20, "+" + hearts(h), Color(123, 224, 138));
    burst(P.x, P.y - 8, Color(123, 224, 138), 10, 0.8, -0.02);
    return true;
}

void Sim::quickHeal() {
    const int missing = maxHp() - P.hp;
    if (missing <= 0) {
        toast("Your health is already full.");
        play("deny");
        return;
    }
    // Use the smallest thing that covers the wound, so potions are not wasted on scratches
    static const char* small[4] = {"meat", "bandage", "roast", "potion"};
    static const char* medium[4] = {"bandage", "roast", "meat", "potion"};
    static const char* large[4] = {"roast", "potion", "bandage", "meat"};
    static const char* huge[4] = {"potion", "roast", "bandage", "meat"};
    const char** order = missing >= 10 ? huge : missing >= 5 ? large : missing >= 2 ? medium : small;
    for (int i = 0; i < 4; i++)
        if (hasItem(order[i])) {
            useHeal(order[i]);
            return;
        }
    toast("Nothing to heal with. Craft bandages from herbs or roast some meat.");
    play("deny");
}

void Sim::quickUse(const std::string& id) {
    if (!hasItem(id)) {
        toast("No " + db.item(id).name + " in your bag.");
        play("deny");
        return;
    }
    useHeal(id);
}

std::string Sim::itemName(const Item& it) const {
    const ItemDef& def = db.item(it.id);
    std::string name = def.name;
    if (it.rarity > 0 && it.rarity < static_cast<int>(db.rarities.size()) && !db.rarities[it.rarity].name.empty()) name = db.rarities[it.rarity].name + " " + name;
    if (!it.affixes.empty())
        if (const AffixDef* a = db.affix(it.affixes.front().first)) name += " " + a->label;
    return name;
}

std::vector<std::string> Sim::itemLines(const Item& it) const {
    const ItemDef& def = db.item(it.id);
    std::vector<std::string> lines;
    switch (def.type) {
        case ItemType::Weapon: lines.push_back("Damage " + std::to_string(def.dmg) + (def.reach >= 20 ? " · long reach" : "")); break;
        case ItemType::Shield: lines.push_back("Blocks " + std::to_string(def.def) + " from strong hits"); break;
        case ItemType::Armor: lines.push_back("+" + hearts(def.hp) + " hearts"); break;
        case ItemType::Heal: lines.push_back("Restores " + hearts(def.heal) + (def.heal == 2 ? " heart" : " hearts")); break;
        case ItemType::Mount: lines.push_back("Speed x" + std::to_string(def.mountSpeed).substr(0, 3)); break;
        default: break;
    }
    for (const auto& [id, v] : it.affixes)
        if (const AffixDef* a = db.affix(id)) lines.push_back(fill(a->text, v));
    return lines;
}

int Sim::itemValue(const Item& it) const {
    const ItemDef& def = db.item(it.id);
    const double mul = it.rarity >= 0 && it.rarity < static_cast<int>(db.rarities.size()) ? db.rarities[it.rarity].valueMul : 1;
    return static_cast<int>(std::lround(def.value * mul));
}

int Sim::statBonus(const std::string& stat) const {
    int total = 0;
    for (int uid : P.equip)
        if (const Item* it = findUid(uid))
            for (const auto& [id, v] : it->affixes)
                if (const AffixDef* a = db.affix(id); a && a->stat == stat) total += v;
    return total;
}

int Sim::maxHp() const {
    const Tuning& t = db.tuning;
    int hp = t.baseHp + 2 * (P.level / t.heartEvery) + 2 * P.heartCrystals + statBonus("hp");
    if (const Item* a = equipped(EquipSlot::Armor)) hp += db.item(a->id).hp;
    return hp;
}

int Sim::weaponDamage() const {
    const Item* w = equipped(EquipSlot::Weapon);
    return (w ? db.item(w->id).dmg : db.tuning.fistDmg) + P.level / db.tuning.damageEvery + statBonus("dmg");
}
int Sim::weaponReach() const {
    const Item* w = equipped(EquipSlot::Weapon);
    return w ? db.item(w->id).reach : db.tuning.fistReach;
}
int Sim::weaponCooldown() const {
    const Item* w = equipped(EquipSlot::Weapon);
    return w ? db.item(w->id).cd : db.tuning.fistCd;
}
int Sim::shieldDef() const {
    const Item* s = equipped(EquipSlot::Shield);
    return (s ? db.item(s->id).def : 0) + statBonus("ward");
}
double Sim::speedMul() const { return 1.0 + statBonus("speed") / 100.0 + (hasBuff("swift") ? 0.25 : 0.0); }

bool Sim::hasBuff(const std::string& id) const {
    for (const auto& b : P.buffs)
        if (b.id == id && b.t > 0) return true;
    return false;
}

bool Sim::canCraft(const RecipeDef& r) const {
    for (const auto& [id, q] : r.in)
        if (countItem(id) < q) return false;
    return true;
}

std::string Sim::recipeBlocked(const RecipeDef& r) const {
    if (db.item(r.out).unique && hasItem(r.out)) return "You already have one.";
    if (r.fire && !nearFire()) return "Needs a camp fire.";
    return "";
}

std::vector<const RecipeDef*> Sim::recipes(const std::string& cat) const {
    std::vector<const RecipeDef*> out;
    for (const auto& r : db.recipes)
        if (cat == "all" || r.cat == cat) out.push_back(&r);
    return out;
}

int Sim::craft(const RecipeDef& r, int times) {
    const std::string blocked = recipeBlocked(r);
    if (!blocked.empty()) {
        toast(blocked);
        play("deny");
        return 0;
    }
    const ItemDef& out = db.item(r.out);
    int n = 0;
    for (int i = 0; i < times; i++) {
        if (!canCraft(r) || !recipeBlocked(r).empty()) break;
        if (out.stacks() ? false : static_cast<int>(P.inv.size()) >= db.tuning.invSlots) {
            toast("Your bag is full.");
            break;
        }
        for (const auto& [id, q] : r.in) removeItem(id, q);
        addItem(out.isGear() ? makeGear(r.out, 0, 0.6) : Item{r.out, r.outQty});
        n++;
        if (out.unique || out.isGear()) break;
    }
    if (n) {
        play("craft");
        toast("Crafted " + (n > 1 ? std::to_string(n) + "x " : std::string()) + out.name + ".", Color(123, 224, 138));
        craftFlash = 20;
        questEvent("have", "");
    } else {
        play("deny");
        toast("Not enough materials.");
    }
    return n;
}

bool Sim::nearFire() const {
    for (const auto& o : map().objects)
        if (o.kind == ObjKind::Campfire && hypot2(o.c * TILE + 8 - P.x, o.r * TILE + 8 - P.y) < 52) return true;
    return false;
}

std::vector<ShopRow> Sim::shopRows(bool buying) const {
    std::vector<ShopRow> rows;
    const ShopDef* shop = db.shop(shopId);
    if (!shop) return rows;
    if (buying) {
        for (const auto& e : shop->buy) rows.push_back({e.item, e.qty, e.price, -1});
        return rows;
    }
    for (size_t i = 0; i < P.inv.size(); i++) {
        const Item& it = P.inv[i];
        const ItemDef& def = db.item(it.id);
        if (def.isGear()) {
            if (!isEquipped(it)) rows.push_back({it.id, 1, std::max(1, static_cast<int>(itemValue(it) * 0.4)), static_cast<int>(i)});
        } else if (auto s = shop->sell.find(it.id); s != shop->sell.end()) rows.push_back({it.id, 1, s->second, static_cast<int>(i)});
    }
    return rows;
}

int Sim::shopTrade(bool buying, int row, int times) {
    int n = 0;
    for (int i = 0; i < times; i++) {
        const auto rows = shopRows(buying);
        if (row < 0 || row >= static_cast<int>(rows.size())) break;
        const ShopRow& it = rows[row];
        const ItemDef& def = db.item(it.id);
        if (buying) {
            std::string why;
            if (def.unique && hasItem(it.id)) why = "You already have one.";
            else if ((def.isGear() || def.type == ItemType::Mount) && def.level > P.level) why = "Requires level " + std::to_string(def.level) + ".";
            else if (countItem("gold") < it.price) why = "Not enough gold.";
            if (!why.empty()) {
                if (!n) {
                    toast(why);
                    play("deny");
                }
                break;
            }
            if (!addItem(def.isGear() ? makeGear(it.id, 0, 0) : Item{it.id, it.qty})) break;
            removeItem("gold", it.price);
            n++;
            if (def.isGear() || def.unique) break;
        } else {
            if (def.isGear()) P.inv.erase(P.inv.begin() + it.bagIndex);
            else if (!removeItem(it.id, 1)) break;
            addItem({"gold", it.price});
            n++;
            if (def.isGear()) break;
        }
    }
    if (n) play("coin");
    return n;
}

}   // namespace hc

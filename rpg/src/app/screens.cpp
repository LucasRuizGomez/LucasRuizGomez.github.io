// In-game screens drawn over the world: bag, crafting, shop, dialogue, quest log, world map, fast travel.
#include "app/app.h"
#include "app/palette.h"

#include <cmath>
#include <cstdio>

namespace hc {

namespace {
std::string playTime(int ticks) {
    char buf[32];
    const int mins = ticks / 3600;
    std::snprintf(buf, sizeof buf, mins >= 60 ? "%dh %02dm" : "%d min", mins >= 60 ? mins / 60 : mins, mins % 60);
    return buf;
}
std::string hearts(int halves) { return halves % 2 ? std::to_string(halves / 2) + ".5" : std::to_string(halves / 2); }
}   // namespace

void App::drawInventory() {
    Sim& sim = *sim_;
    Player& P = sim.P;
    dim(0.6);
    const int COLS = 6, slotsN = db_.tuning.invSlots, ROWS = (slotsN + COLS - 1) / COLS;
    const double S = 58, GAP = 7, gw = COLS * S + (COLS - 1) * GAP, gh = ROWS * S + (ROWS - 1) * GAP;
    const double W = gw + 370, H = gh + 120, X = std::round((uiW() - W) / 2), Y = std::round((uiH() - H) / 2);
    panel(X, Y, W, H);
    text("Bag", X + 24, Y + 38, pal::text, 22, 1);
    text(std::to_string(P.inv.size()) + " / " + std::to_string(slotsN), X + 24 + gw, Y + 38, pal::faint, 13, 1, Align::Right);
    drawSpr(spr_.icon("gold"), X + 90, Y + 18, 24);
    text(std::to_string(sim.countItem("gold")) + " gold", X + 118, Y + 37, pal::gold, 14, 1);
    const double gx = X + 24, gy = Y + 56;
    invSel_ = std::clamp(invSel_, 0, slotsN - 1);
    for (int i = 0; i < slotsN; i++) {
        const double sx = gx + (i % COLS) * (S + GAP), sy = gy + (i / COLS) * (S + GAP);
        const bool has = i < static_cast<int>(P.inv.size());
        slotBox(sx, sy, S, S, !has);
        if (has && P.inv[i].rarity > 0) box(sx + 2, sy + 2, S - 4, S - 4, Color(0, 0, 0, 0), db_.rarities[P.inv[i].rarity].glow, 2);
        if (i == invSel_) box(sx - 2, sy - 2, S + 4, S + 4, Color(0, 0, 0, 0), pal::sel, 3);
        hit(sx, sy, S, S, [this, i] {
            if (invSel_ == i) sim_->useOrEquip(i);
            else invSel_ = i;
        });
        if (!has) continue;
        const Item& it = P.inv[i];
        drawSpr(spr_.icon(it.id), sx + 7, sy + 7, S - 14);
        if (it.qty > 1) {
            text(std::to_string(it.qty), sx + S - 5, sy + S - 5, pal::ltText, 13, 2, Align::Right);
            text(std::to_string(it.qty), sx + S - 6, sy + S - 6, pal::text, 13, 2, Align::Right);
        }
        if (sim.isEquipped(it)) {
            box(sx + 4, sy + 4, 16, 16, pal::sel, pal::btnInk);
            text("E", sx + 12, sy + 12, pal::ltText, 11, 2, Align::Center, Baseline::Middle);
        }
    }
    // Details of the selected item, then the hero's stats
    const double dx = gx + gw + 26, dw = W - (dx - X) - 24;
    box(dx, gy, dw, gh, pal::well, pal::line);
    double ty = gy + 16;
    if (invSel_ < static_cast<int>(P.inv.size())) {
        const Item& it = P.inv[invSel_];
        const ItemDef& d = db_.item(it.id);
        drawSpr(spr_.icon(it.id), dx + 14, gy + 14, 60);
        const auto nameLines = wrap(sim.itemName(it), dw - 100, 16, 1);
        for (size_t i = 0; i < nameLines.size(); i++) text(nameLines[i], dx + 86, gy + 34 + i * 19.0, rarityColor(it), 16, 1);
        const double subY = gy + 34 + nameLines.size() * 19.0;
        text(d.isGear() ? (it.rarity ? db_.rarities[it.rarity].id : std::string("common")) + " " + (d.type == ItemType::Weapon ? "weapon" : d.type == ItemType::Shield ? "shield" : d.type == ItemType::Armor ? "armor" : "trinket")
                        : "x " + std::to_string(it.qty),
             dx + 86, subY, pal::muted, 12, 1);
        ty = std::max(gy + 92, subY + 22);
        const auto statLines = sim.itemLines(it);
        for (size_t i = 0; i < statLines.size(); i++) {
            text(statLines[i], dx + 16, ty, i == 0 && d.type != ItemType::Trinket ? pal::text : pal::ok, 13, 1);
            ty += 18;
        }
        for (const auto& l : wrap(d.desc, dw - 32, 13)) {
            text(l, dx + 16, ty + 4, pal::muted, 13);
            ty += 17;
        }
        if (d.value > 0) {
            text("Worth " + std::to_string(sim.itemValue(it)) + " gold", dx + 16, ty + 8, pal::faint, 12);
            ty += 18;
        }
        const std::string action = d.isGear() ? (sim.isEquipped(it) ? "Unequip" : "Equip") : d.type == ItemType::Heal ? "Use" : d.type == ItemType::Mount ? "Ride" : "";
        if (!action.empty()) {
            const double kw = keycap("Enter", dx + 16, ty + 14, 20);
            text(action, dx + 24 + kw, ty + 24, pal::text, 13, 1, Align::Left, Baseline::Middle);
        }
    } else text("Empty slot", dx + dw / 2, gy + 50, pal::faint, 14, 0, Align::Center, Baseline::Middle);

    // Hero
    const double sy = gy + gh - 132;
    g_.fillRect(dx + 12, sy - 8, dw - 24, 1, pal::line);
    text(P.name, dx + 16, sy + 12, pal::text, 15, 1);
    text("Level " + std::to_string(P.level), dx + dw - 16, sy + 12, pal::gold, 13, 1, Align::Right);
    const std::pair<std::string, std::string> stats[] = {
        {"Health", hearts(P.hp) + " / " + hearts(sim.maxHp()) + " hearts"},
        {"Damage", std::to_string(sim.weaponDamage())},
        {"Block", sim.shieldDef() ? std::to_string(sim.shieldDef()) + " per strong hit" : "none"},
        {"Speed", std::to_string(static_cast<int>(std::lround(sim.speedMul() * 100))) + "%"},
        {"Critical", std::to_string(sim.statBonus("crit")) + "%"},
    };
    for (size_t i = 0; i < 5; i++) {
        text(stats[i].first, dx + 16, sy + 32 + i * 17.0, pal::muted, 12);
        text(stats[i].second, dx + dw - 16, sy + 32 + i * 17.0, pal::text, 12, 1, Align::Right);
    }
    static const char* SLOT_NAMES[4] = {"Weapon", "Shield", "Armor", "Trinket"};
    std::string worn;
    for (int i = 0; i < 4; i++)
        if (const Item* e = sim.equipped(static_cast<EquipSlot>(i))) worn += (worn.empty() ? "" : "  ·  ") + std::string(SLOT_NAMES[i]) + ": " + db_.item(e->id).name;
    const double fy = Y + H - 30;
    double fx = X + 24;
    fx += keycap("Arrows", fx, fy - 11, 22) + 8;
    text("Select", fx, fy, pal::muted, 13, 0, Align::Left, Baseline::Middle);
    fx += 58;
    fx += keycap(bindLabel("craft"), fx, fy - 11, 22) + 8;
    text("Crafting", fx, fy, pal::muted, 13, 0, Align::Left, Baseline::Middle);
    fx += 70;
    fx += keycap(bindLabel("bag"), fx, fy - 11, 22) + 8;
    text("Close", fx, fy, pal::muted, 13, 0, Align::Left, Baseline::Middle);
    if (textW(worn, 12) < W - (fx - X) - 90) text(worn, X + W - 24, fy, pal::muted, 12, 0, Align::Right, Baseline::Middle);
    closeButton(X + W - 40, Y + 14);
}

void App::setCraftCat(int step) {
    int i = 0;
    for (size_t k = 0; k < db_.categories.size(); k++)
        if (db_.categories[k].id == craftCat_) i = static_cast<int>(k);
    const int n = static_cast<int>(db_.categories.size());
    craftCat_ = db_.categories[(i + step + n) % n].id;
    craftSel_ = craftScroll_ = 0;
}

void App::craftSelected(int times) {
    const auto list = sim_->recipes(craftCat_);
    if (craftSel_ >= 0 && craftSel_ < static_cast<int>(list.size())) sim_->craft(*list[craftSel_], times);
}

void App::drawCrafting() {
    Sim& sim = *sim_;
    dim(0.6);
    const double W = std::min(900.0, uiW() - 40), H = std::min(560.0, uiH() - 40), X = std::round((uiW() - W) / 2), Y = std::round((uiH() - H) / 2);
    panel(X, Y, W, H);
    text("Crafting", X + 24, Y + 38, pal::text, 22, 1);
    const bool fire = sim.nearFire();
    text(fire ? "At a fire: cooking and forging available" : "Cooking and iron gear need a camp fire", X + W - 56, Y + 38, fire ? pal::ok : pal::faint, 13, 1, Align::Right);
    double tx = X + 24;
    for (const auto& c : db_.categories) {
        const double w = textW(c.label, 14, 1) + 32;
        button(tx, Y + 56, w, 30, c.label, c.id == craftCat_, 14);
        const std::string id = c.id;
        hit(tx, Y + 56, w, 30, [this, id] {
            craftCat_ = id;
            craftSel_ = craftScroll_ = 0;
        });
        tx += w + 6;
    }
    const auto list = sim.recipes(craftCat_);
    const double LW = 320, rowH = 44, ly = Y + 100, lh = H - 100 - 54;
    const int rows = static_cast<int>(lh / rowH);
    craftSel_ = std::clamp(craftSel_, 0, std::max(0, static_cast<int>(list.size()) - 1));
    if (craftSel_ < craftScroll_) craftScroll_ = craftSel_;
    if (craftSel_ >= craftScroll_ + rows) craftScroll_ = craftSel_ - rows + 1;
    for (int i = craftScroll_; i < std::min(static_cast<int>(list.size()), craftScroll_ + rows); i++) {
        const RecipeDef& r = *list[i];
        const double y = ly + (i - craftScroll_) * rowH;
        const bool ok = sim.canCraft(r) && sim.recipeBlocked(r).empty();
        row(X + 24, y, LW, rowH - 6, i == craftSel_);
        hit(X + 24, y, LW, rowH - 6, [this, i] {
            if (craftSel_ == i) craftSelected(in_.shift() ? 5 : 1);
            else craftSel_ = i;
        });
        drawSpr(spr_.icon(r.out), X + 32, y + 3, 32);
        text(db_.item(r.out).name + (r.outQty > 1 ? " x" + std::to_string(r.outQty) : ""), X + 74, y + 19, ok ? pal::text : pal::faint, 14, 1, Align::Left, Baseline::Middle);
        if (ok) box(X + 24 + LW - 18, y + 15, 8, 8, pal::ok);
    }
    if (static_cast<int>(list.size()) > rows)
        text(std::to_string(craftScroll_ + 1) + " to " + std::to_string(std::min(static_cast<int>(list.size()), craftScroll_ + rows)) + " of " + std::to_string(list.size()), X + 24 + LW, ly + rows * rowH + 8, pal::faint, 12, 0, Align::Right);

    const double dx = X + 24 + LW + 20, dw = W - (dx - X) - 24;
    box(dx, ly, dw, lh, sim.craftFlash ? pal::flash : pal::well, pal::line);
    if (!list.empty()) {
        const RecipeDef& r = *list[craftSel_];
        const ItemDef& it = db_.item(r.out);
        const std::string blocked = sim.recipeBlocked(r);
        const bool ok = sim.canCraft(r) && blocked.empty();
        drawSpr(spr_.icon(r.out), dx + 16, ly + 16, 72);
        text(it.name, dx + 100, ly + 42, pal::gold, 18, 1);
        text("Makes " + std::to_string(r.outQty), dx + 100, ly + 64, pal::muted, 13, 1);
        Item sample{r.out, 1};
        double ty = ly + 112;
        for (const auto& l : sim.itemLines(sample)) {
            text(l, dx + 16, ty, pal::text, 13, 1);
            ty += 18;
        }
        for (const auto& l : wrap(it.desc, dw - 32, 13)) {
            text(l, dx + 16, ty, pal::muted, 13);
            ty += 17;
        }
        ty += 14;
        text("Needs", dx + 16, ty, pal::muted, 13, 1);
        for (size_t i = 0; i < r.in.size(); i++) {
            const int have = sim.countItem(r.in[i].first), q = r.in[i].second;
            const double y = ty + 12 + i * 36.0;
            drawSpr(spr_.icon(r.in[i].first), dx + 16, y, 30);
            text(db_.item(r.in[i].first).name, dx + 56, y + 15, pal::text, 14, 1, Align::Left, Baseline::Middle);
            text(std::to_string(have) + " / " + std::to_string(q), dx + dw - 16, y + 15, have >= q ? pal::ok : pal::bad, 14, 1, Align::Right, Baseline::Middle);
        }
        if (r.fire) text(fire ? "Camp fire: nearby" : "Camp fire: go to a town or camp", dx + 16, ty + 22 + r.in.size() * 36.0, fire ? pal::ok : pal::bad, 13, 1);
        text(!blocked.empty() ? blocked : ok ? "Ready to craft" : "Missing materials", dx + 16, ly + lh - 56, ok ? pal::ok : pal::bad, 14, 1);
        double fx = dx + 16;
        const double cbx = fx;
        fx += keycap("Enter", fx, ly + lh - 38, 22) + 8;
        text("Craft", fx, ly + lh - 27, pal::text, 13, 1, Align::Left, Baseline::Middle);
        fx += 50;
        hit(cbx, ly + lh - 40, fx - cbx - 8, 26, [this] { craftSelected(1); });
        if (it.stacks() && !it.unique) {
            fx += keycap("Shift + Enter", fx, ly + lh - 38, 22) + 8;
            text("Craft 5", fx, ly + lh - 27, pal::text, 13, 1, Align::Left, Baseline::Middle);
        }
    }
    const double fy = Y + H - 26;
    double fx = X + 24;
    fx += keycap("Up / Down", fx, fy - 11, 22) + 8;
    text("Recipe", fx, fy, pal::muted, 13, 0, Align::Left, Baseline::Middle);
    fx += 62;
    fx += keycap("Left / Right", fx, fy - 11, 22) + 8;
    text("Category", fx, fy, pal::muted, 13, 0, Align::Left, Baseline::Middle);
    fx += 78;
    fx += keycap(bindLabel("craft"), fx, fy - 11, 22) + 8;
    text("Close", fx, fy, pal::muted, 13, 0, Align::Left, Baseline::Middle);
    closeButton(X + W - 40, Y + 14);
}

void App::drawShop() {
    Sim& sim = *sim_;
    dim(0.6);
    const ShopDef* shop = db_.shop(sim.shopId);
    const double W = std::min(760.0, uiW() - 40), H = std::min(540.0, uiH() - 40), X = std::round((uiW() - W) / 2), Y = std::round((uiH() - H) / 2);
    panel(X, Y, W, H);
    text(sim.signTitle.empty() ? (shop ? shop->title : "Trader") : sim.signTitle, X + 24, Y + 38, pal::text, 22, 1);
    drawSpr(spr_.icon("gold"), X + W - 160, Y + 18, 26);
    text(std::to_string(sim.countItem("gold")) + " gold", X + W - 128, Y + 37, pal::gold, 15, 1);
    closeButton(X + W - 40, Y + 14);
    double tx = X + 24;
    for (int t = 0; t < 2; t++) {
        const bool buy = t == 0;
        button(tx, Y + 56, 90, 30, buy ? "Buy" : "Sell", shopBuying_ == buy, 14);
        hit(tx, Y + 56, 90, 30, [this, buy] {
            shopBuying_ = buy;
            shopSel_ = 0;
        });
        tx += 96;
    }
    const auto L = sim.shopRows(shopBuying_);
    const double rowH = 42, ly = Y + 100;
    const int rows = static_cast<int>((H - 170) / rowH);
    shopSel_ = std::clamp(shopSel_, 0, std::max(0, static_cast<int>(L.size()) - 1));
    const int scroll = std::max(0, shopSel_ - rows + 1);
    if (L.empty()) text(shopBuying_ ? "Nothing for sale." : "You carry nothing this trader wants.", X + W / 2, ly + 40, pal::faint, 14, 0, Align::Center);
    for (int i = scroll; i < std::min(static_cast<int>(L.size()), scroll + rows); i++) {
        const ShopRow& it = L[i];
        const ItemDef& def = db_.item(it.id);
        const double y = ly + (i - scroll) * rowH;
        const bool locked = shopBuying_ && (def.isGear() || def.type == ItemType::Mount) && def.level > sim.P.level;
        const bool can = !shopBuying_ || (sim.countItem("gold") >= it.price && !(def.unique && sim.hasItem(it.id)) && !locked);
        row(X + 24, y, W - 48, rowH - 6, i == shopSel_);
        hit(X + 24, y, W - 48, rowH - 6, [this, i] {
            if (shopSel_ == i) sim_->shopTrade(shopBuying_, i, in_.shift() ? 5 : 1);
            else shopSel_ = i;
        });
        drawSpr(spr_.icon(it.id), X + 32, y + 2, 32);
        std::string name = def.name + (it.qty > 1 ? " x" + std::to_string(it.qty) : "");
        Color nameCol = can ? pal::text : pal::faint;
        if (!shopBuying_ && it.bagIndex >= 0 && it.bagIndex < static_cast<int>(sim.P.inv.size()) && def.isGear()) {
            name = sim.itemName(sim.P.inv[it.bagIndex]);
            nameCol = rarityColor(sim.P.inv[it.bagIndex]);
        }
        text(name, X + 74, y + 18, nameCol, 14, 1, Align::Left, Baseline::Middle);
        text(locked ? "needs level " + std::to_string(def.level) : "you have " + std::to_string(sim.countItem(it.id)), X + W - 170, y + 18, locked ? pal::bad : pal::faint, 12, 0, Align::Right, Baseline::Middle);
        text((shopBuying_ ? "" : "+") + std::to_string(it.price) + " gold", X + W - 40, y + 18, shopBuying_ ? (can ? pal::gold : pal::bad) : pal::ok, 14, 1, Align::Right, Baseline::Middle);
    }
    if (!L.empty()) {
        const ShopRow& it = L[shopSel_];
        Item sample{it.id, 1};
        if (!shopBuying_ && it.bagIndex >= 0 && it.bagIndex < static_cast<int>(sim.P.inv.size())) sample = sim.P.inv[it.bagIndex];
        std::string info;
        for (const auto& l : sim.itemLines(sample)) info += (info.empty() ? "" : "  ·  ") + l;
        if (info.empty()) info = db_.item(it.id).desc;
        const auto lines = wrap(info, W - 48, 13);
        if (!lines.empty()) text(lines.front(), X + 24, Y + H - 48, pal::muted, 13);
    }
    text("Up / Down choose · Left / Right buy or sell · Enter trade (Shift x5) · Esc leave", X + W / 2, Y + H - 18, pal::muted, 12, 0, Align::Center);
}

void App::drawSign() {
    Sim& sim = *sim_;
    const double W = std::min(680.0, uiW() - 40);
    const auto lines = wrap(sim.signText, W - 48, 16);
    const double H = 92 + lines.size() * 23.0, X = std::round((uiW() - W) / 2), Y = std::round(uiH() - H - 70);
    panel(X, Y, W, H);
    text(sim.signTitle, X + 24, Y + 36, pal::gold, 16, 1);
    for (size_t i = 0; i < lines.size(); i++) text(lines[i], X + 24, Y + 64 + i * 23.0, pal::text, 16);
    const std::string key = btnLabel("interact").empty() ? "E" : btnLabel("interact");
    const double kw = keycap(key, X + W - 90, Y + H - 32, 20);
    text("Close", X + W - 90 + kw + 8, Y + H - 21, pal::muted, 13, 0, Align::Left, Baseline::Middle);
}

void App::drawQuests() {
    Sim& sim = *sim_;
    dim(0.6);
    const double W = std::min(900.0, uiW() - 40), H = std::min(560.0, uiH() - 40), X = std::round((uiW() - W) / 2), Y = std::round((uiH() - H) / 2);
    panel(X, Y, W, H);
    text("Quest log", X + 24, Y + 38, pal::text, 22, 1);
    int doneCount = 0;
    for (const auto& [id, st] : sim.quests) doneCount += st.state == 2;
    text(std::to_string(doneCount) + " of " + std::to_string(db_.quests.size()) + " quests finished", X + W - 56, Y + 38, pal::faint, 13, 1, Align::Right);
    closeButton(X + W - 40, Y + 14);
    const auto list = sim.activeQuests();
    const double LW = 300, rowH = 44, ly = Y + 60, lh = H - 60 - 46;
    questSel_ = std::clamp(questSel_, 0, std::max(0, static_cast<int>(list.size()) - 1));
    for (size_t i = 0; i < list.size() && i < static_cast<size_t>(lh / rowH); i++) {
        const QuestDef& q = *list[i];
        const double y = ly + i * rowH;
        row(X + 24, y, LW, rowH - 6, static_cast<int>(i) == questSel_);
        hit(X + 24, y, LW, rowH - 6, [this, i] { questSel_ = static_cast<int>(i); });
        if (q.main) box(X + 30, y + 6, 4, rowH - 18, pal::gold);
        text(q.title, X + 42, y + 14, pal::text, 14, 1, Align::Left, Baseline::Middle);
        text(sim.questReady(q) ? "Ready to hand in" : q.main ? "Main quest" : "Side quest", X + 42, y + 29, sim.questReady(q) ? pal::ok : pal::faint, 11, 1, Align::Left, Baseline::Middle);
    }
    const double dx = X + 24 + LW + 20, dw = W - (dx - X) - 24;
    box(dx, ly, dw, lh, pal::well, pal::line);
    if (list.empty()) {
        text("No quests yet.", dx + dw / 2, ly + 60, pal::muted, 15, 1, Align::Center);
        text("People with a yellow mark over their heads have work for you.", dx + dw / 2, ly + 86, pal::faint, 13, 0, Align::Center);
    } else {
        const QuestDef& q = *list[questSel_];
        const NpcDef* giver = db_.npc(q.giver);
        const NpcDef* turnIn = db_.npc(q.turnIn);
        const TownDef* town = turnIn ? db_.town(turnIn->town) : nullptr;
        text(q.title, dx + 18, ly + 34, pal::gold, 19, 1);
        text("From " + (giver ? giver->name : std::string("?")), dx + 18, ly + 54, pal::muted, 12, 1);
        double ty = ly + 84;
        for (const auto& l : wrap(q.offer, dw - 36, 14)) {
            text(l, dx + 18, ty, pal::text, 14);
            ty += 19;
        }
        ty += 14;
        text("Objectives", dx + 18, ty, pal::muted, 13, 1);
        ty += 22;
        for (size_t i = 0; i < q.objectives.size(); i++) {
            const QuestObjective& o = q.objectives[i];
            const int have = sim.objectiveProgress(q, i);
            const bool done = have >= o.count;
            box(dx + 18, ty - 11, 12, 12, done ? pal::ok : pal::well, pal::line);
            text(o.label, dx + 38, ty, done ? pal::ok : pal::text, 14, 1);
            if (o.count > 1) text(std::to_string(have) + " / " + std::to_string(o.count), dx + dw - 18, ty, done ? pal::ok : pal::bad, 14, 1, Align::Right);
            ty += 22;
        }
        text(std::string("Hand in to ") + (turnIn ? turnIn->name : "?") + (town ? " in " + town->name : ""), dx + 18, ty + 8, pal::muted, 13);
        std::string reward = std::to_string(q.xp) + " xp";
        if (q.gold) reward += "  ·  " + std::to_string(q.gold) + " gold";
        for (const auto& [id, n] : q.items) reward += "  ·  " + (n > 1 ? std::to_string(n) + " " : std::string()) + db_.item(id).name;
        text("Reward", dx + 18, ly + lh - 40, pal::muted, 13, 1);
        text(reward, dx + 18, ly + lh - 20, pal::text, 13, 1);
    }
    text("Up / Down choose · " + bindLabel("map") + " world map · " + bindLabel("quests") + " or Esc close", X + W / 2, Y + H - 18, pal::muted, 12, 0, Align::Center);
}

// The whole realm, with fog over what you have not walked yet and marks for where your quests point.
void App::drawWorldMap() {
    Sim& sim = *sim_;
    dim(0.7);
    Map& over = sim.overworld();
    const Sprite& mini = world_->minimap(sim, over);
    const double maxW = uiW() - 80, maxH = uiH() - 130;
    const int k = std::max(1, static_cast<int>(std::floor(std::min(maxW * ui_ / over.w, maxH * ui_ / over.h))));
    const double w = over.w * k / ui_, h = over.h * k / ui_, X = std::round((uiW() - w) / 2), Y = std::round((uiH() - h) / 2) + 14;
    panel(X - 16, Y - 54, w + 32, h + 92);
    text("The Realm", X, Y - 20, pal::text, 22, 1);
    text(std::to_string(sim.zonesFound.size()) + " of " + std::to_string(db_.zones.size()) + " regions discovered", X + w, Y - 20, pal::faint, 13, 1, Align::Right);
    const int PX = static_cast<int>(std::lround(X * ui_)), PY = static_cast<int>(std::lround(Y * ui_));
    g_.setTransform(1);
    g_.fillRect(PX, PY, over.w * k, over.h * k, Color(24, 20, 30));
    g_.draw(mini, PX, PY, over.w * k, over.h * k);
    g_.setTransform(ui_);
    auto seen = [&](int c, int r) { return over.inside(c, r) && over.explored[over.idx(c, r)]; };
    auto pos = [&](int c, int r) { return Vec2{X + c * k / ui_, Y + r * k / ui_}; };

    for (const auto& z : db_.zones) {
        if (!sim.zonesFound.count(z.id)) continue;
        for (size_t i = 0; i < z.centers.size() && i < 1; i++) {
            const Vec2 p = pos(z.centers[i].c, z.centers[i].r);
            g_.setAlpha(0.75);
            shadowText(z.name, p.x, p.y - 34, pal::ltText, 15, 2, Align::Center);
            shadowText("Levels " + std::to_string(z.levelMin) + " to " + std::to_string(z.levelMax), p.x, p.y - 20, pal::ltMuted, 11, 1, Align::Center);
            g_.setAlpha(1);
        }
    }
    for (const auto& t : db_.towns) {
        if (!seen(t.at.c, t.at.r)) continue;
        const Vec2 p = pos(t.at.c, t.at.r);
        box(p.x - 5, p.y - 5, 10, 10, sim.waypoints.count(t.id) ? Color(120, 200, 255) : Color(214, 210, 200), pal::ink, 2);
        shadowText(t.name, p.x, p.y + 20, pal::ltGold, 13, 1, Align::Center);
    }
    for (const auto& d : db_.dungeons) {
        if (!d.under.empty() || !seen(d.at.c, d.at.r)) continue;
        const Vec2 p = pos(d.at.c, d.at.r);
        box(p.x - 4, p.y - 4, 8, 8, sim.bossesDefeated.count(d.id) ? Color(120, 120, 130) : Color(220, 130, 255), pal::ink, 2);
        shadowText(d.name, p.x, p.y - 8, Color(232, 200, 255), 12, 1, Align::Center);
    }
    // Where active quests want you to go
    for (const QuestDef* q : sim.activeQuests()) {
        TilePos target{-1, -1};
        if (sim.questReady(*q)) {
            if (const NpcDef* n = db_.npc(q->turnIn))
                if (const TownDef* t = db_.town(n->town)) target = t->at;
        } else
            for (const auto& o : q->objectives) {
                if (o.type == "visit")
                    if (const TownDef* t = db_.town(o.target)) target = t->at;
                if (o.type == "boss")
                    if (const DungeonDef* d = db_.dungeon(o.target)) target = d->at;
            }
        if (target.c < 0) continue;
        const Vec2 p = pos(target.c, target.r);
        const double pulse = 9 + 3 * std::sin(sim.tickCount * 0.1 + target.c);
        box(p.x - pulse, p.y - pulse, pulse * 2, pulse * 2, Color(0, 0, 0, 0), Color(255, 226, 60), 2);
        shadowText(q->title, p.x, p.y - pulse - 6, Color(255, 226, 60), 12, 1, Align::Center);
    }
    if (sim.map().outdoor) {
        const Vec2 p = pos(ifloor(sim.P.x / TILE), ifloor(sim.P.y / TILE));
        box(p.x - 5, p.y - 5, 10, 10, Color(224, 80, 60), Color(255, 255, 255), 2);
        shadowText("You", p.x, p.y - 10, pal::ltText, 12, 1, Align::Center);
    } else if (sim.map().dungeon) {
        const DungeonDef* d = sim.map().dungeon;
        while (d && !d->under.empty()) d = db_.dungeon(d->under);
        if (d) {
            const Vec2 p = pos(d->at.c, d->at.r);
            box(p.x - 5, p.y - 5, 10, 10, Color(224, 80, 60), Color(255, 255, 255), 2);
        }
    }
    text("Blue: waypoint town · Purple: dungeon · Yellow ring: quest · " + bindLabel("map") + " or Esc close", X + w / 2, Y + h + 22, pal::muted, 12, 0, Align::Center);
    hit(0, 0, uiW(), uiH(), [this] { setMode(Mode::Play); });
}

std::vector<const TownDef*> App::travelList() const {
    std::vector<const TownDef*> out;
    for (const auto& t : db_.towns)
        if (sim_->waypoints.count(t.id)) out.push_back(&t);
    return out;
}

void App::drawTravel() {
    Sim& sim = *sim_;
    dim(0.6);
    const auto list = travelList();
    const double W = std::min(480.0, uiW() - 40), H = 130 + list.size() * 46.0, X = std::round((uiW() - W) / 2), Y = std::round((uiH() - H) / 2);
    panel(X, Y, W, H);
    text("Waypoint", X + 24, Y + 38, pal::text, 22, 1);
    text("Travel to a town you have visited", X + 24, Y + 60, pal::muted, 13);
    closeButton(X + W - 40, Y + 14);
    travelSel_ = std::clamp(travelSel_, 0, std::max(0, static_cast<int>(list.size()) - 1));
    for (size_t i = 0; i < list.size(); i++) {
        const TownDef& t = *list[i];
        const double y = Y + 78 + i * 46.0;
        const bool here = t.id == sim.townId;
        row(X + 24, y, W - 48, 40, static_cast<int>(i) == travelSel_);
        hit(X + 24, y, W - 48, 40, [this, i] {
            if (travelSel_ == static_cast<int>(i)) handleKey("Enter", false);
            else travelSel_ = static_cast<int>(i);
        });
        text(t.name, X + 40, y + 20, here ? pal::faint : pal::text, 15, 1, Align::Left, Baseline::Middle);
        const ZoneDef* z = db_.zone(t.zone);
        text(here ? "You are here" : z ? z->name : "", X + W - 40, y + 20, pal::muted, 12, 0, Align::Right, Baseline::Middle);
    }
    text("Up / Down choose · Enter travel · Esc close", X + W / 2, Y + H - 18, pal::muted, 12, 0, Align::Center);
}

void App::drawDead() {
    const TownDef* t = db_.town(sim_->P.lastTown);
    centerCard("You fell", "You wake by the fire in " + (t ? t->name : std::string("town")) + ". You keep your gear.", {{"Deaths so far", std::to_string(sim_->deaths)}, {"Gold lost", std::to_string(static_cast<int>(sim_->countItem("gold") * db_.tuning.goldLost))}},
               "Enter to wake up");
}

void App::drawVictory() {
    const Item* w = sim_->equipped(EquipSlot::Weapon);
    centerCard("The Hollow King has fallen", "A stairway has opened beneath his throne...",
               {{"Time played", playTime(sim_->time)}, {"Level", std::to_string(sim_->P.level)}, {"Deaths", std::to_string(sim_->deaths)}, {"Weapon", w ? sim_->itemName(*w) : "Fists"}}, "Enter to keep exploring");
}

}   // namespace hc

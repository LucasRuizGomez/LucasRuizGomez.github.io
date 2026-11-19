// The bag with the hero's paper doll (drag gear onto the body, drag anything out of the bag to throw it away)
// and the skill tree.
#include "app/app.h"
#include "app/palette.h"

#include <algorithm>
#include <cmath>

namespace hc {

namespace {

std::string hearts(int halves) { return halves % 2 ? std::to_string(halves / 2) + ".5" : std::to_string(halves / 2); }

// Paper doll: slot k (EquipSlot 0..5, 6 = mount) sits at column / row of a 3 x 4 grid around the hero
const int DOLL_COL[App::DOLL_SLOTS] = {0, 2, 0, 2, 1, 1, 0};
const int DOLL_ROW[App::DOLL_SLOTS] = {1, 1, 2, 2, 0, 3, 3};
const char* DOLL_NAME[App::DOLL_SLOTS] = {"Weapon", "Shield", "Body", "Ring", "Head", "Feet", "Mount"};

const int BAG_COLS = 6;

}   // namespace

// ───────────────────────────── bag ─────────────────────────────

const Item* App::dollItem(int k) const {
    if (k < EQUIP_SLOTS) return sim_->equipped(static_cast<EquipSlot>(k));
    const std::string id = sim_->P.mountPick;
    if (id.empty()) return nullptr;
    for (const auto& it : sim_->P.inv)
        if (it.id == id) return &it;
    return nullptr;
}

int App::invIndexOf(const Item* it) const {
    if (!it) return -1;
    for (size_t i = 0; i < sim_->P.inv.size(); i++)
        if (&sim_->P.inv[i] == it) return static_cast<int>(i);
    return -1;
}

// The bag entry (index into P.inv) the selection points at, or -1
int App::selectedInvIndex() const {
    if (invSel_ >= 0) {
        const auto view = sim_->bagView();
        return invSel_ < static_cast<int>(view.size()) ? view[invSel_] : -1;
    }
    return invIndexOf(dollItem(-1 - invSel_));
}

void App::invActivate() {
    Sim& sim = *sim_;
    if (invSel_ >= 0) {
        const int i = selectedInvIndex();
        if (i >= 0) sim.useOrEquip(i);
        return;
    }
    const int k = -1 - invSel_;
    if (!dollItem(k)) return;
    if (k < EQUIP_SLOTS) {
        const std::string name = sim.itemName(*dollItem(k));
        if (sim.unequip(static_cast<EquipSlot>(k))) sim.toast("Unequipped " + name + ".");
    } else sim.setMountPick("");
}

void App::invDiscard(bool all) {
    const int i = selectedInvIndex();
    if (i < 0) return;
    sim_->discardItem(i, all ? sim_->P.inv[i].qty : 1);
}

// Arrow keys: the bag grid and the doll are one surface; walking left off the grid lands on the doll
void App::invMove(const std::string& dir) {
    const int n = db_.tuning.invSlots, rows = (n + BAG_COLS - 1) / BAG_COLS;
    if (invSel_ >= 0) {
        const int c = invSel_ % BAG_COLS, r = invSel_ / BAG_COLS;
        if (dir == "right") invSel_ = std::min(n - 1, invSel_ + 1);
        else if (dir == "down") invSel_ = std::min(n - 1, invSel_ + BAG_COLS);
        else if (dir == "up") invSel_ = std::max(0, invSel_ - BAG_COLS);
        else if (dir == "left") {
            if (c > 0) invSel_--;
            else {   // onto the right-hand column of the doll, at about the same height
                const double want = rows > 1 ? r * 3.0 / (rows - 1) : 0;
                int best = 1;
                double bd = 1e9;
                for (int k = 0; k < DOLL_SLOTS; k++)
                    if (DOLL_COL[k] == 2 && std::abs(DOLL_ROW[k] - want) < bd) {
                        bd = std::abs(DOLL_ROW[k] - want);
                        best = k;
                    }
                invSel_ = -1 - best;
            }
        }
        return;
    }
    const int k = -1 - invSel_, dc = dir == "right" ? 1 : dir == "left" ? -1 : 0, dr = dir == "down" ? 1 : dir == "up" ? -1 : 0;
    int best = -1;
    double bd = 1e9;
    for (int j = 0; j < DOLL_SLOTS; j++) {
        const int ddc = DOLL_COL[j] - DOLL_COL[k], ddr = DOLL_ROW[j] - DOLL_ROW[k];
        if (j == k || ddc * dc + ddr * dr <= 0) continue;
        const double d = dc ? std::abs(ddc) + std::abs(ddr) * 3.0 : std::abs(ddr) + std::abs(ddc) * 3.0;
        if (d < bd) {
            bd = d;
            best = j;
        }
    }
    if (best >= 0) invSel_ = -1 - best;
    else if (dir == "right") {   // off the doll into the bag
        const int r = static_cast<int>(std::lround(DOLL_ROW[k] * (rows - 1) / 3.0));
        invSel_ = std::clamp(r, 0, rows - 1) * BAG_COLS;
    }
}

void App::invPress(int kind, int index) {
    drag_ = Drag();
    drag_.kind = kind;
    drag_.index = index;
    drag_.x0 = in_.mouseX / ui_;
    drag_.y0 = in_.mouseY / ui_;
    const int sel = kind == 0 ? index : -1 - index;
    drag_.wasSel = invSel_ == sel;
    invSel_ = sel;
    // Only something that exists can be dragged
    if (kind == 0) {
        const auto view = sim_->bagView();
        if (index >= static_cast<int>(view.size())) drag_.kind = -1;
    } else if (!dollItem(index)) drag_.kind = -1;
}

void App::invRelease(double ux, double uy) {
    Sim& sim = *sim_;
    const Drag d = drag_;
    drag_ = Drag();
    if (d.kind < 0) return;
    if (!d.moved) {   // a plain click: the second click on the same slot uses or equips it
        if (d.wasSel) invActivate();
        return;
    }
    const auto view = sim.bagView();
    const int from = d.kind == 0 ? (d.index < static_cast<int>(view.size()) ? view[d.index] : -1) : invIndexOf(dollItem(d.index));
    if (from < 0) return;
    const Item item = sim.P.inv[from];
    const ItemDef& def = db_.item(item.id);
    const Zone* z = nullptr;
    for (const auto& zz : zones_)
        if (ux >= zz.x && ux < zz.x + zz.w && uy >= zz.y && uy < zz.y + zz.h) z = &zz;
    const bool outside = ux < invRect_[0] || uy < invRect_[1] || ux >= invRect_[0] + invRect_[2] || uy >= invRect_[1] + invRect_[3];
    if (!z) {
        if (outside) {   // dropped off the panel: thrown on the ground
            if (sim.discardItem(from, item.qty) && d.kind == 0) invSel_ = std::min(invSel_, std::max(0, static_cast<int>(sim.bagView().size()) - 1));
        }
        return;
    }
    if (z->kind == 1) {   // onto a doll slot
        const int k = z->index;
        if (d.kind == 1 && k == d.index) return;
        if (k < EQUIP_SLOTS) {
            if (!Sim::slotAccepts(static_cast<EquipSlot>(k), def.type)) {
                sim.toast(def.isGear() ? "That goes in the " + std::string(DOLL_NAME[static_cast<int>(Sim::slotFor(def.type))]) + " slot." : "Only gear can be worn.");
                sim.play("deny");
                return;
            }
            if (sim.equipTo(from, static_cast<EquipSlot>(k))) sim.toast("Equipped " + sim.itemName(item) + ".");
        } else if (def.type == ItemType::Mount) {
            if (sim.setMountPick(item.id)) sim.toast(def.name + " will answer your call.");
        } else {
            sim.toast("Only a mount goes there.");
            sim.play("deny");
            return;
        }
        invSel_ = -1 - k;
        return;
    }
    // onto a bag cell
    const int cell = z->index;
    if (d.kind == 1) {
        if (d.index < EQUIP_SLOTS) {
            if (sim.unequip(static_cast<EquipSlot>(d.index))) sim.toast("Unequipped " + sim.itemName(item) + ".");
        } else sim.setMountPick("");
        invSel_ = std::min(cell, std::max(0, static_cast<int>(sim.bagView().size()) - 1));
        return;
    }
    sim.moveItem(from, cell < static_cast<int>(view.size()) ? view[cell] : -1);
    invSel_ = std::min(cell, std::max(0, static_cast<int>(sim.bagView().size()) - 1));
    sim.play("ui");
}

void App::drawInventory() {
    Sim& sim = *sim_;
    Player& P = sim.P;
    dim(0.6);
    zones_.clear();
    const int slotsN = db_.tuning.invSlots, ROWS = (slotsN + BAG_COLS - 1) / BAG_COLS;
    const double S = 52, GAP = 6, gw = BAG_COLS * S + (BAG_COLS - 1) * GAP, gh = ROWS * S + (ROWS - 1) * GAP;
    const double DW = 232, DETW = 300, CH = std::max(gh, 340.0);   // CH: height of the three wells
    const double W = 24 + DW + 20 + gw + 20 + DETW + 24, H = CH + 132, X = std::round((uiW() - W) / 2), Y = std::round((uiH() - H) / 2);
    invRect_[0] = X;
    invRect_[1] = Y;
    invRect_[2] = W;
    invRect_[3] = H;
    panel(X, Y, W, H);
    const double ux = in_.mouseX / ui_, uy = in_.mouseY / ui_;
    if (drag_.kind >= 0 && !in_.mouseLeft) drag_ = Drag();   // released somewhere we did not hear about
    if (drag_.kind >= 0 && std::hypot(ux - drag_.x0, uy - drag_.y0) > 6) drag_.moved = true;
    const auto view = sim.bagView();
    const int bagN = static_cast<int>(view.size());
    if (invSel_ >= slotsN) invSel_ = slotsN - 1;
    if (invSel_ < -DOLL_SLOTS) invSel_ = -1;

    // What is being dragged, if anything
    const Item* dragged = nullptr;
    if (drag_.kind >= 0 && drag_.moved) dragged = drag_.kind == 0 ? (drag_.index < bagN ? &P.inv[view[drag_.index]] : nullptr) : dollItem(drag_.index);

    // ── the hero and what they wear ──
    const double dx0 = X + 24, gy = Y + 56;
    text(P.name, dx0, Y + 38, pal::text, 20, 1);
    text("Level " + std::to_string(P.level), dx0 + DW, Y + 38, pal::gold, 13, 1, Align::Right);
    box(dx0, gy, DW, CH, pal::well, pal::line);
    {
        const AnimalSet& K = spr_.knight(world_->heroLayers(sim));
        if (K.anims.count("idle")) {
            const Frames& idle = K.get("idle")[0];
            if (!idle.empty()) drawSpr(idle[(sim.tickCount / 30) % idle.size()], dx0 + DW / 2 - 70, gy + CH / 2 - 84, 140);
        }
    }
    const double colX[3] = {dx0 + 10, dx0 + DW / 2 - S / 2, dx0 + DW - 10 - S}, rowY[4] = {gy + 10, gy + CH * 0.27, gy + CH * 0.52, gy + CH - 10 - S};
    for (int k = 0; k < DOLL_SLOTS; k++) {
        const double sx = colX[DOLL_COL[k]], sy = rowY[DOLL_ROW[k]];
        const Item* it = dollItem(k);
        const bool hiding = it && dragged == it;
        slotBox(sx, sy, S, S, !it || hiding);
        if (dragged) {   // where the dragged piece can go
            const ItemDef& dd = db_.item(dragged->id);
            const bool fits = k < EQUIP_SLOTS ? Sim::slotAccepts(static_cast<EquipSlot>(k), dd.type) : dd.type == ItemType::Mount;
            if (fits) box(sx - 2, sy - 2, S + 4, S + 4, Color(0, 0, 0, 0), pal::ok, 2 + (sim.tickCount / 10) % 2);
        }
        if (it && !hiding) {
            if (it->rarity > 0) box(sx + 2, sy + 2, S - 4, S - 4, Color(0, 0, 0, 0), db_.rarities[it->rarity].glow, 2);
            drawSpr(spr_.icon(it->id), sx + 7, sy + 7, S - 14);
        } else text(DOLL_NAME[k], sx + S / 2, sy + S / 2, pal::faint, 11, 1, Align::Center, Baseline::Middle);
        if (invSel_ == -1 - k) box(sx - 2, sy - 2, S + 4, S + 4, Color(0, 0, 0, 0), pal::sel, 3);
        hit(sx, sy, S, S, [this, k] { invPress(1, k); });
        zones_.push_back({sx, sy, S, S, 1, k});
    }

    // ── the bag ──
    const double gx = dx0 + DW + 20;
    text("Bag", gx, Y + 38, pal::text, 20, 1);
    drawSpr(spr_.icon("gold"), gx + 46, Y + 20, 22);
    text(std::to_string(sim.countItem("gold")), gx + 72, Y + 37, pal::gold, 14, 1);
    text(std::to_string(bagN) + " / " + std::to_string(slotsN), gx + gw, Y + 38, bagN >= slotsN ? pal::bad : pal::faint, 13, 1, Align::Right);
    for (int i = 0; i < slotsN; i++) {
        const double sx = gx + (i % BAG_COLS) * (S + GAP), sy = gy + (i / BAG_COLS) * (S + GAP);
        const bool has = i < bagN;
        const Item* it = has ? &P.inv[view[i]] : nullptr;
        const bool hiding = it && dragged == it;
        slotBox(sx, sy, S, S, !has || hiding);
        if (i == invSel_) box(sx - 2, sy - 2, S + 4, S + 4, Color(0, 0, 0, 0), pal::sel, 3);
        hit(sx, sy, S, S, [this, i] { invPress(0, i); });
        zones_.push_back({sx, sy, S, S, 0, i});
        if (!it || hiding) continue;
        if (it->rarity > 0) box(sx + 2, sy + 2, S - 4, S - 4, Color(0, 0, 0, 0), db_.rarities[it->rarity].glow, 2);
        drawSpr(spr_.icon(it->id), sx + 7, sy + 7, S - 14);
        if (it->qty > 1) {
            text(std::to_string(it->qty), sx + S - 5, sy + S - 5, pal::ltText, 13, 2, Align::Right);
            text(std::to_string(it->qty), sx + S - 6, sy + S - 6, pal::text, 13, 2, Align::Right);
        }
    }

    // ── details of the selection, then the hero's numbers ──
    const double tx = gx + gw + 20, dw = DETW;
    box(tx, gy, dw, CH, pal::well, pal::line);
    const double sy = gy + CH - 96, by = sy - 46;   // hero numbers at the bottom, buttons above them
    const int sel = selectedInvIndex();
    double ty = gy + 16;
    if (sel >= 0) {
        const Item& it = P.inv[sel];
        const ItemDef& d = db_.item(it.id);
        drawSpr(spr_.icon(it.id), tx + 12, gy + 12, 52);
        const auto nameLines = wrap(sim.itemName(it), dw - 84, 15, 1);
        for (size_t i = 0; i < nameLines.size() && i < 2; i++) text(nameLines[i], tx + 74, gy + 30 + i * 18.0, rarityColor(it), 15, 1);
        const double subY = gy + 30 + std::min<size_t>(2, nameLines.size()) * 18.0;
        static const std::map<ItemType, const char*> KIND = {{ItemType::Weapon, "weapon"}, {ItemType::Shield, "shield"}, {ItemType::Armor, "armor"},
                                                             {ItemType::Trinket, "ring"},   {ItemType::Helm, "helm"},     {ItemType::Boots, "boots"}};
        std::string sub = "x " + std::to_string(it.qty);
        if (d.isGear()) sub = (it.rarity ? db_.rarities[it.rarity].id : std::string("common")) + " " + KIND.at(d.type) + (sim.isEquipped(it) ? " · worn" : "");
        else if (d.type == ItemType::Mount) sub = it.id == P.mountPick ? "your mount" : "mount";
        text(sub, tx + 74, subY, pal::muted, 12, 1);
        ty = std::max(gy + 80, subY + 20);
        for (const auto& l : sim.itemLines(it)) {
            text(l, tx + 14, ty, d.isGear() ? pal::ok : pal::text, 13, 1);
            ty += 17;
        }
        for (const auto& l : wrap(d.desc, dw - 28, 12)) {
            if (ty > by - 30) break;
            text(l, tx + 14, ty + 3, pal::muted, 12);
            ty += 15;
        }
        if (d.value > 0 && ty + 8 < by - 8) text("Worth " + std::to_string(sim.itemValue(it)) + " gold", tx + 14, ty + 8, pal::faint, 12);
        // Buttons: use / equip, and throw away
        const std::string action = d.isGear() ? (sim.isEquipped(it) ? "Unequip" : "Equip")
                                   : d.type == ItemType::Heal  ? "Use"
                                   : d.type == ItemType::Mount ? (P.mount == it.id ? "Dismount" : "Ride")
                                                               : "";
        double bx = tx + 14;
        if (!action.empty()) {
            button(bx, by, 86, 26, action, false, 13);
            hit(bx, by, 86, 26, [this] { invActivate(); });
            bx += 92;
        }
        if (sim.discardBlocked(it).empty()) {
            button(bx, by, 72, 26, "Drop", false, 13);
            hit(bx, by, 72, 26, [this] { invDiscard(false); });
            bx += 78;
            if (it.qty > 1) {
                button(bx, by, 86, 26, "Drop all", false, 13);
                hit(bx, by, 86, 26, [this] { invDiscard(true); });
            }
        }
    } else text(invSel_ < 0 ? std::string("Nothing in the ") + DOLL_NAME[-1 - invSel_] + " slot" : "Empty slot", tx + dw / 2, gy + 40, pal::faint, 13, 0, Align::Center, Baseline::Middle);

    g_.fillRect(tx + 10, sy - 8, dw - 20, 1, pal::line);
    const std::pair<std::string, std::string> stats[] = {
        {"Health", hearts(P.hp) + " / " + hearts(sim.maxHp()) + " hearts"},
        {"Damage", std::to_string(sim.weaponDamage()) + "   bolts " + std::to_string(sim.bowDamage())},
        {"Block", sim.shieldDef() ? std::to_string(sim.shieldDef()) + " per strong hit" : "none"},
        {"Speed · Critical", std::to_string(static_cast<int>(std::lround(sim.speedMul() * 100))) + "% · " + std::to_string(sim.statBonus("crit")) + "%"},
        {"Heart shards", std::to_string(P.heartShards) + " / " + std::to_string(db_.tuning.heartShards)},
    };
    for (size_t i = 0; i < 5; i++) {
        text(stats[i].first, tx + 14, sy + 8 + i * 17.0, pal::muted, 12);
        text(stats[i].second, tx + dw - 14, sy + 8 + i * 17.0, pal::text, 12, 1, Align::Right);
    }

    // ── footer ──
    const double fy = Y + H - 44;
    double fx = X + 24;
    fx += keycap("Enter", fx, fy - 11, 22) + 6;
    text("Use / equip", fx, fy, pal::muted, 13, 0, Align::Left, Baseline::Middle);
    fx += 84;
    fx += keycap("X", fx, fy - 11, 22) + 6;
    text("Drop (Shift: all)", fx, fy, pal::muted, 13, 0, Align::Left, Baseline::Middle);
    fx += 118;
    fx += keycap(bindLabel("skills"), fx, fy - 11, 22) + 6;
    const int pts = sim.skillPoints();
    text(pts > 0 ? "Skill tree (" + std::to_string(pts) + " points)" : "Skill tree", fx, fy, pts > 0 ? pal::gold : pal::muted, 13, pts > 0 ? 1 : 0, Align::Left, Baseline::Middle);
    fx += textW("Skill tree (00 points)", 13, 1) + 14;
    fx += keycap(bindLabel("craft"), fx, fy - 11, 22) + 6;
    text("Crafting", fx, fy, pal::muted, 13, 0, Align::Left, Baseline::Middle);
    fx += 66;
    fx += keycap(bindLabel("bag"), fx, fy - 11, 22) + 6;
    text("Close", fx, fy, pal::muted, 13, 0, Align::Left, Baseline::Middle);
    text("Drag gear onto the hero to wear it. Drag anything out of this window to throw it on the ground.", X + 24, Y + H - 16, pal::faint, 12);
    closeButton(X + W - 40, Y + 14);

    // The piece in hand follows the cursor
    if (dragged) {
        g_.setAlpha(0.9);
        drawSpr(spr_.icon(dragged->id), ux - 20, uy - 20, 40);
        g_.setAlpha(1);
        const bool outside = ux < X || uy < Y || ux >= X + W || uy >= Y + H;
        if (outside && sim.discardBlocked(*dragged).empty()) shadowText("Drop on the ground", ux, uy + 34, pal::ltGold, 13, 1, Align::Center);
    }
}

// ───────────────────────────── skill tree ─────────────────────────────

void App::skillMove(const std::string& dir) {
    const int n = static_cast<int>(db_.skills.size());
    if (n == 0 || skillPos_.size() != static_cast<size_t>(n)) return;
    skillSel_ = std::clamp(skillSel_, 0, n - 1);
    const double vx = dir == "right" ? 1 : dir == "left" ? -1 : 0, vy = dir == "down" ? 1 : dir == "up" ? -1 : 0;
    const Vec2 from = skillPos_[skillSel_];
    int best = -1;
    double bd = 1e18;
    for (int i = 0; i < n; i++) {
        if (i == skillSel_) continue;
        const double ex = skillPos_[i].x - from.x, ey = skillPos_[i].y - from.y, along = ex * vx + ey * vy;
        if (along <= 1) continue;
        const double side = std::abs(ex * vy - ey * vx), d = along + side * 2.5;
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    if (best >= 0) {
        skillSel_ = best;
        audio_.play("uiMove");
    }
}

void App::drawSkills() {
    Sim& sim = *sim_;
    dim(0.65);
    const double W = std::min(1060.0, uiW() - 40), H = std::min(680.0, uiH() - 30), X = std::round((uiW() - W) / 2), Y = std::round((uiH() - H) / 2);
    panel(X, Y, W, H);
    text("Skill tree", X + 24, Y + 38, pal::text, 22, 1);
    const int pts = sim.skillPoints();
    text(pts > 0 ? std::to_string(pts) + (pts == 1 ? " point to spend" : " points to spend") : "No points to spend · one more every " + std::to_string(db_.tuning.skillPointEvery) + " levels", X + W - 56, Y + 38, pts > 0 ? pal::gold : pal::faint, 14, 1, Align::Right);
    closeButton(X + W - 40, Y + 14);

    const int nb = std::max<int>(1, static_cast<int>(db_.branches.size()));
    int rows = 1;
    for (const auto& s : db_.skills) rows = std::max(rows, static_cast<int>(s.row) + 1);
    const double colW = (W - 48 - (nb - 1) * 12.0) / nb, top = Y + 58, rowH = 58, nodeW = std::min(104.0, colW / 3 - 6), nodeH = 46;
    skillPos_.assign(db_.skills.size(), Vec2{0, 0});
    std::map<std::string, Vec2> centre;
    skillSel_ = std::clamp(skillSel_, 0, std::max(0, static_cast<int>(db_.skills.size()) - 1));
    for (size_t i = 0; i < db_.skills.size(); i++) {
        const SkillDef& s = db_.skills[i];
        int b = 0;
        for (int k = 0; k < nb; k++)
            if (db_.branches[k].id == s.branch) b = k;
        const double bx = X + 24 + b * (colW + 12);
        skillPos_[i] = {bx + colW / 2 + (s.col - 1) * (colW / 3), top + 52 + s.row * rowH + nodeH / 2};
        centre[s.id] = skillPos_[i];
    }
    // Branch backgrounds and titles
    for (int k = 0; k < nb; k++) {
        const BranchDef& br = db_.branches[k];
        const double bx = X + 24 + k * (colW + 12);
        box(bx, top, colW, 52 + rows * rowH, pal::well, pal::line);
        box(bx, top, colW, 4, br.color);
        int spent = 0;
        for (const auto& s : db_.skills)
            if (s.branch == br.id) spent += sim.skillRank(s.id);
        text(br.label, bx + 12, top + 24, br.color.alpha(1), 17, 2);
        text(std::to_string(spent) + " spent", bx + colW - 12, top + 24, pal::faint, 12, 1, Align::Right);
        text(br.desc, bx + 12, top + 42, pal::muted, 12);
    }
    // Connectors: down from the requirement, across, and down into the skill
    for (const auto& s : db_.skills) {
        if (s.requires_.empty() || !centre.count(s.requires_)) continue;
        const Vec2 a = centre[s.requires_], b = centre[s.id];
        const bool met = sim.skillRank(s.requires_) >= s.requiresRank;
        const Color c = met ? pal::gold : pal::line;
        const double y0 = a.y + nodeH / 2, y1 = b.y - nodeH / 2, ym = std::round((y0 + y1) / 2);
        g_.fillRect(a.x - 1.5, y0, 3, ym - y0, c);
        g_.fillRect(std::min(a.x, b.x) - 1.5, ym - 1.5, std::abs(b.x - a.x) + 3, 3, c);
        g_.fillRect(b.x - 1.5, ym, 3, y1 - ym, c);
    }
    // Nodes
    for (size_t i = 0; i < db_.skills.size(); i++) {
        const SkillDef& s = db_.skills[i];
        const BranchDef* br = nullptr;
        for (const auto& b : db_.branches)
            if (b.id == s.branch) br = &b;
        const Vec2 c = skillPos_[i];
        const double nx = c.x - nodeW / 2, ny = c.y - nodeH / 2;
        const int rank = sim.skillRank(s.id);
        const bool open = sim.skillBlocked(s).empty();
        const bool sel = static_cast<int>(i) == skillSel_;
        const Color fill = rank >= s.maxRank ? (br ? br->color.alpha(0.55) : pal::slotSel) : rank > 0 ? (br ? br->color.alpha(0.3) : pal::slotSel) : open ? pal::slot : pal::well;
        box(nx, ny, nodeW, nodeH, fill, sel ? pal::sel : open ? pal::gold : pal::line, sel ? 3 : open ? 2 : 1);
        if (open && !sel) box(nx - 2, ny - 2, nodeW + 4, nodeH + 4, Color(0, 0, 0, 0), Color(255, 210, 90, static_cast<uint8_t>(90 + 80 * std::sin(sim.tickCount * 0.12))), 1);
        // Icon (OwlishMedia) in an octagonal frame (Keiffer), greyed out until it can be learned; name and rank pips beside it
        // the Dungeon Crawl icon in a brass-rimmed well; dark until it can be learned
        box(nx + 5, ny + 5, 36, 36, Color(10, 8, 7), rank ? (br ? br->color : pal::gold) : Color(110, 88, 54), rank ? 2 : 1);
        if (!rank && !open) g_.setAlpha(0.3);
        else if (!rank) g_.setAlpha(0.7);
        if (!s.icon.empty()) drawSpr(spr_.get(s.icon), nx + 7, ny + 7, 32);
        g_.setAlpha(1);
        const auto lines = wrap(s.name, nodeW - 48, 11, 1);
        for (size_t l = 0; l < lines.size() && l < 2; l++) text(lines[l], nx + 45, ny + 15 + l * 12.0, rank || open ? pal::text : pal::faint, 11, 1);
        for (int r = 0; r < s.maxRank; r++) {   // one pip per rank
            const double px = nx + 45 + r * 10;
            box(px, ny + nodeH - 11, 7, 7, r < rank ? pal::gold : Color(0, 0, 0, 0), r < rank ? pal::text : pal::faint, 1);
        }
        if (s.level > 1 && sim.P.level < s.level) text(std::to_string(s.level), nx + nodeW - 4, ny + nodeH - 4, pal::bad, 10, 1, Align::Right);
        if (std::any_of(s.effects.begin(), s.effects.end(), [](const auto& e) { return e.first == "whirl" || e.first == "warcry" || e.first == "leap" || e.first == "stoneskin" || e.first == "volley"; }))
            g_.fillRect(nx + nodeW - 9, ny + 3, 6, 6, Color(236, 196, 110));   // a gold corner marks an active skill
        const int idx = static_cast<int>(i);
        hit(nx, ny, nodeW, nodeH, [this, idx] {
            if (skillSel_ == idx) sim_->learnSkill(db_.skills[idx].id);
            else skillSel_ = idx;
        }, [this, idx] { skillSel_ = idx; });
    }
    // Details of the selected skill
    const double dy = top + 52 + rows * rowH + 10, dh = Y + H - 40 - dy;
    box(X + 24, dy, W - 48, dh, pal::well, pal::line);
    if (!db_.skills.empty()) {
        const SkillDef& s = db_.skills[skillSel_];
        const int rank = sim.skillRank(s.id);
        const std::string why = sim.skillBlocked(s);
        text(s.name, X + 40, dy + 26, pal::gold, 18, 1);
        text("Rank " + std::to_string(rank) + " of " + std::to_string(s.maxRank), X + 40 + textW(s.name, 18, 1) + 14, dy + 26, pal::muted, 13, 1);
        text(s.desc + (s.maxRank > 1 ? " (per rank)" : ""), X + 40, dy + 50, pal::text, 14);
        if (why.empty()) {
            const double kw = keycap("Enter", X + 40, dy + dh - 30, 20);
            text("Learn" + std::string(rank ? " the next rank" : "") + " for 1 point (or click it again)", X + 48 + kw, dy + dh - 19, pal::ok, 13, 1, Align::Left, Baseline::Middle);
        } else text(why, X + 40, dy + dh - 16, rank >= s.maxRank ? pal::ok : pal::bad, 13, 1);
    }
    text("Arrows choose · Enter learn · " + bindLabel("bag") + " bag · " + bindLabel("skills") + " or Esc close", X + W / 2, Y + H - 16, pal::muted, 12, 0, Align::Center);
}

}   // namespace hc

// Heads-up display: hearts, level, equipment and quick slots, minimap with quest tracker,
// boss bar, interaction prompt, floating numbers, toasts and banners.
#include "app/app.h"
#include "app/palette.h"

#include <cmath>

namespace hc {

void App::drawHud() {
    Sim& sim = *sim_;
    const Player& P = sim.P;
    const Map& m = sim.map();
    const int tick = sim.tickCount;
    auto toScreen = [&](double wx, double wy) { return Vec2{(wx * zoom_ - world_->offX) / ui_, (wy * zoom_ - world_->offY) / ui_}; };

    // Hearts (ten to a row)
    const int hearts = (sim.maxHp() + 1) / 2;
    for (int i = 0; i < hearts; i++) {
        const int v = std::clamp(P.hp - i * 2, 0, 2);
        const bool low = P.hp <= 2 && v > 0 && (tick >> 4) % 2;
        drawSpr(spr_.heart[v], 16 + (i % 10) * 27 - (low ? 1 : 0), 12 + (i / 10) * 26 - (low ? 1 : 0), 26 + (low ? 2 : 0));
    }
    double y = 14 + ((hearts + 9) / 10) * 26;

    // Level and experience
    {
        const bool capped = P.level >= db_.tuning.maxLevel;
        const std::string lv = "Level " + std::to_string(P.level);
        shadowText(lv, 16, y + 13, pal::ltGold, 14, 1);
        const double bx = 22 + textW(lv, 14, 1), bw = 150;
        bar(bx, y + 4, bw, 7, capped ? 1 : static_cast<double>(P.xp) / sim.xpToNext(), pal::xp, Color(40, 30, 60));
        if (!capped) shadowText(std::to_string(P.xp) + " / " + std::to_string(sim.xpToNext()), bx + bw + 8, y + 12, pal::ltMuted, 11);
        y += 26;
    }

    // Equipment row: weapon, shield, crossbow, healing, whirlwind, mount
    struct Slot {
        std::string key, icon, empty, count;
        double cool = 0;
        bool locked = false;
    };
    std::vector<Slot> slots;
    const Item* weapon = sim.equipped(EquipSlot::Weapon);
    const Item* shield = sim.equipped(EquipSlot::Shield);
    slots.push_back({btnLabel("attack"), weapon ? weapon->id : "", "Fists", ""});
    slots.push_back({"", shield ? shield->id : "", "", ""});
    slots.push_back({btnLabel("shoot"), sim.hasItem("bow") ? "bow" : "", "", sim.hasItem("bow") ? std::to_string(sim.countItem("arrows")) : ""});
    {
        std::string healIcon;
        for (const char* id : {"potion", "roast", "bandage", "meat"})
            if (sim.hasItem(id)) {
                healIcon = id;
                break;
            }
        const int n = sim.countItem("potion") + sim.countItem("roast") + sim.countItem("bandage") + sim.countItem("meat");
        slots.push_back({btnLabel("heal"), healIcon, "", healIcon.empty() ? "" : std::to_string(n)});
    }
    slots.push_back({btnLabel("skill"), weapon ? weapon->id : "wood_sword", "", "", static_cast<double>(P.whirlCd) / db_.tuning.whirlCooldown, !sim.whirlwindUnlocked()});
    {
        std::string mount;
        double best = 0;
        for (const auto& it : P.inv)
            if (const ItemDef& d = db_.item(it.id); d.type == ItemType::Mount && d.mountSpeed > best) {
                best = d.mountSpeed;
                mount = it.id;
            }
        if (!mount.empty()) slots.push_back({btnLabel("mount"), mount, "", ""});
    }
    for (size_t i = 0; i < slots.size(); i++) {
        const Slot& s = slots[i];
        const double x = 16 + i * 50.0, sy = y + 8;
        slotBox(x, sy, 44, 44, s.locked);
        if (!s.icon.empty()) {
            if (s.locked) g_.setAlpha(0.3);
            drawSpr(spr_.icon(s.icon), x + 6, sy + 6, 32);
            g_.setAlpha(1);
        } else if (!s.empty.empty()) text(s.empty, x + 22, sy + 24, pal::faint, 10, 1, Align::Center, Baseline::Middle);
        if (i == 4) {   // whirlwind: a ring mark, the unlock level, and the cooldown sweeping down
            if (s.locked) text("Lv " + std::to_string(db_.tuning.whirlLevel), x + 22, sy + 24, pal::muted, 11, 1, Align::Center, Baseline::Middle);
            else if (s.cool > 0) g_.fillRect(x + 3, sy + 3, 38, std::round(38 * s.cool), Color(20, 14, 10, 150));
            else box(x + 3, sy + 3, 38, 38, Color(0, 0, 0, 0), Color(255, 226, 138, static_cast<uint8_t>(120 + 80 * std::sin(tick * 0.1))), 2);
        }
        if (!s.key.empty()) keycap(s.key, x - 4, sy - 6, 16);
        if (!s.count.empty()) text(s.count, x + 40, sy + 40, pal::text, 12, 1, Align::Right);
    }
    // Quick items 1-3
    static const char* QUICK[3][2] = {{"bandage", "quick1"}, {"roast", "quick2"}, {"potion", "quick3"}};
    const double qx0 = 16 + slots.size() * 50.0 + 8;
    for (int i = 0; i < 3; i++) {
        const double x = qx0 + i * 40, sy = y + 14;
        const int n = sim.countItem(QUICK[i][0]);
        slotBox(x, sy, 34, 34, n == 0);
        g_.setAlpha(n ? 1 : 0.35);
        drawSpr(spr_.icon(QUICK[i][0]), x + 5, sy + 5, 24);
        g_.setAlpha(1);
        if (scheme_ != Scheme::Pad) keycap(bindLabel(QUICK[i][1]), x - 3, sy - 6, 14);
        if (n) text(std::to_string(n), x + 31, sy + 31, pal::text, 11, 1, Align::Right);
    }
    y += 62;
    // Blessings from shrines
    for (const auto& b : P.buffs) {
        if (b.t <= 0) continue;
        const char* name = b.id == "frenzy" ? "Fury" : b.id == "protection" ? "Stone" : b.id == "swift" ? "Wind" : "Fortune";
        const std::string label = std::string(name) + " " + std::to_string(b.t / 60 + 1) + "s";
        const double w = textW(label, 12, 1) + 16;
        box(16, y, w, 20, pal::ltBox, Color(150, 110, 230), 1);
        text(label, 16 + w / 2, y + 10, Color(220, 200, 255), 12, 1, Align::Center, Baseline::Middle);
        y += 24;
    }

    drawMinimap();

    // Boss bar
    const Enemy* boss = sim.activeBoss();
    if (boss) {
        const double w = std::min(520.0, uiW() - 80), x = (uiW() - w) / 2, by = uiH() - 70;
        shadowText(boss->def->name, uiW() / 2, by - 8, pal::ltText, 15, 1, Align::Center);
        box(x - 2, by - 2, w + 4, 16, pal::ink);
        box(x, by, w, 12, Color(58, 26, 36));
        const double fw = std::round(w * boss->hp / boss->maxHp);
        box(x, by, fw, 12, boss->hp <= boss->maxHp / 2 ? Color(255, 90, 74) : Color(200, 56, 74));
        box(x, by, fw, 2, Color(255, 255, 255, 64));
    }

    // Interaction prompt over whatever the hero is facing
    if (sim.focus.kind != Focus::Kind::None && sim.mode == Mode::Play) {
        const Vec2 s = toScreen(sim.focus.x, sim.focus.y);
        const std::string key = btnLabel("interact");
        const double lw = textW(sim.focus.label, 13, 1) + std::max(0.0, textW(key, 10, 1) - 8), w = lw + 42, x = std::round(s.x - w / 2), py = std::round(s.y - 34);
        panel(x - 2, py - 2, w + 4, 30);
        const double kw = keycap(key, x + 6, py + 5, 16);
        text(sim.focus.label, x + 12 + kw, py + 13, pal::text, 13, 1, Align::Left, Baseline::Middle);
    }
    // Names over elites
    for (const auto& e : m.enemies) {
        if (!e.alive || !e.elite) continue;
        const Vec2 s = toScreen(e.x, e.y - 34 * e.def->scale);
        if (s.x < -50 || s.y < -20 || s.x > uiW() + 50 || s.y > uiH()) continue;
        shadowText(e.elite->label + " " + e.def->name, s.x, s.y, e.elite->glow, 12, 1, Align::Center);
    }
    // Floating numbers
    for (const auto& f : sim.floaters) {
        const Vec2 s = toScreen(f.x, f.y);
        g_.setAlpha(std::min(1.0, f.t / 20.0));
        const double fs = std::round(14 + f.pop * 0.6);
        text(f.text, s.x + 1, s.y + 1, pal::ink, fs, 2, Align::Center, Baseline::Middle);
        text(f.text, s.x, s.y, f.col, fs, 2, Align::Center, Baseline::Middle);
        g_.setAlpha(1);
    }
    // Recall cast bar
    if (P.recallT > 0) {
        const double w = 220, x = (uiW() - w) / 2, by = uiH() - 190;
        shadowText("Recalling...", uiW() / 2, by - 8, pal::sky, 13, 1, Align::Center);
        bar(x, by, w, 8, 1 - static_cast<double>(P.recallT) / db_.tuning.recallTicks, pal::sky, Color(20, 40, 60));
    }
    // Toasts
    for (size_t i = 0; i < sim.toasts.size(); i++) {
        const Toast& t = sim.toasts[i];
        const double a = std::min(1.0, t.t / 30.0), ty = uiH() - (boss ? 150 : 120) - (sim.toasts.size() - 1 - i) * 34.0;
        const double w = textW(t.text, 14, 1) + 28;
        g_.setAlpha(a);
        box(uiW() / 2 - w / 2, ty, w, 28, pal::ltBox, pal::ltEdge, 2);
        text(t.text, uiW() / 2, ty + 14, t.col, 14, 1, Align::Center, Baseline::Middle);
        g_.setAlpha(1);
    }
    // Item banner, or the name of the place you just walked into
    if (sim.banner) {
        const Banner& b = *sim.banner;
        const double a = std::min({1.0, b.t / 25.0, (170 - b.t) / 12.0}), w = std::max(360.0, textW(b.title, 18, 1) + 110), x = (uiW() - w) / 2, by = 22 - (1 - a) * 10;
        g_.setAlpha(std::max(0.0, a));
        panel(x, by, w, 76);
        drawSpr(spr_.icon(b.icon), x + 14, by + 14, 48);
        text(b.title, x + 76, by + 34, b.col, 18, 1);
        text(b.sub, x + 76, by + 56, pal::muted, 13);
        g_.setAlpha(1);
    } else if (sim.area) {
        const AreaTitle& ar = *sim.area;
        g_.setAlpha(std::clamp(std::min(ar.t / 40.0, (200 - ar.t) / 20.0), 0.0, 1.0));
        shadowText(ar.text, uiW() / 2, 64, pal::ltText, 30, 2, Align::Center);
        if (!ar.sub.empty()) shadowText(ar.sub, uiW() / 2, 88, pal::ltGold, 15, 1, Align::Center);
        g_.setAlpha(1);
    }
    // Controls hint
    if (sim.mode == Mode::Play) {
        std::string hint;
        if (scheme_ == Scheme::Pad) hint = "Stick move · X attack · RT crossbow · LT whirlwind · B roll · A use · Y heal · LB craft · Back bag · Start menu";
        else {
            const std::string atk = scheme_ == Scheme::Mouse ? "Left click" : bindLabel("attack"), shoot = scheme_ == Scheme::Mouse ? "Right click" : bindLabel("shoot");
            hint = "WASD move · " + atk + " attack · " + shoot + " crossbow · " + bindLabel("dodge") + " roll · " + bindLabel("interact") + " use · " + bindLabel("heal") + " heal · " + bindLabel("bag") +
                   " bag · " + bindLabel("craft") + " craft · " + bindLabel("quests") + " quests · " + bindLabel("map") + " map · Esc menu";
        }
        const double hw = textW(hint, 12) + 24;
        if (hw < uiW() - 20) {
            box(uiW() / 2 - hw / 2, uiH() - 30, hw, 22, pal::ltBox, pal::ltEdge, 1);
            text(hint, uiW() / 2, uiH() - 19, Color(236, 230, 216, 210), 12, 0, Align::Center, Baseline::Middle);
        }
    }
}

// The minimap shows the surroundings on the overworld (it is far too big to fit) and the whole map in dungeons.
void App::drawMinimap() {
    Sim& sim = *sim_;
    Map& m = sim.map();
    const Sprite& mini = world_->minimap(sim, m, !m.outdoor);
    const int pc = ifloor(sim.P.x / TILE), pr = ifloor(sim.P.y / TILE);
    const int vw = m.outdoor ? 64 : m.w, vh = m.outdoor ? 48 : m.h;
    const int c0 = m.outdoor ? std::clamp(pc - vw / 2, 0, m.w - vw) : 0, r0 = m.outdoor ? std::clamp(pr - vh / 2, 0, m.h - vh) : 0;
    const int k = std::max(1, static_cast<int>(std::lround(190 * ui_ / vw)));
    const double w = vw * k / ui_, h = vh * k / ui_, x = uiW() - w - 16, y = 16;
    panel(x - 8, y - 8, w + 16, h + 16);
    const int X = static_cast<int>(std::lround(x * ui_)), Y = static_cast<int>(std::lround(y * ui_));
    g_.setTransform(1);
    g_.fillRect(X, Y, vw * k, vh * k, Color(13, 11, 18));
    g_.drawPart(mini, {c0, r0, vw, vh}, X, Y, vw * k, vh * k);
    const int ms = std::max(2, k * 2);
    auto mark = [&](int c, int r, Color col, int size) {
        if (c < c0 || r < r0 || c >= c0 + vw || r >= r0 + vh) return;
        const int mx = X + (c - c0) * k - size / 2, my = Y + (r - r0) * k - size / 2;
        g_.fillRect(mx - 1, my - 1, size + 2, size + 2, pal::ink);
        g_.fillRect(mx, my, size, size, col);
    };
    for (const auto& o : m.objects) {
        if (o.gone || (!m.outdoor && !m.explored.empty() && !m.explored[m.idx(o.c, o.r)])) continue;
        if (o.kind == ObjKind::Chest && !o.opened) mark(o.c, o.r, Color(242, 210, 122), ms);
        else if (o.kind == ObjKind::Npc) {
            const int q = sim.npcMarker(o.id);
            const NpcDef* n = db_.npc(o.id);
            mark(o.c, o.r, q == 1 || q == 3 ? Color(255, 226, 60) : n && !n->shop.empty() ? Color(123, 224, 138) : Color(143, 200, 255), q == 1 || q == 3 ? ms + 2 : ms);
        } else if (o.kind == ObjKind::Campfire) mark(o.c, o.r, Color(255, 154, 74), ms);
        else if (o.kind == ObjKind::Waypoint) mark(o.c, o.r, Color(120, 200, 255), ms + 1);
        else if (o.kind == ObjKind::Entrance) mark(o.c, o.r, Color(220, 130, 255), ms + 2);
        else if (o.kind == ObjKind::Shrine && sim.tickCount >= o.at) mark(o.c, o.r, Color(196, 160, 255), ms);
    }
    for (const auto& l : m.links)
        if (!m.outdoor && (m.explored.empty() || m.explored[m.idx(l.c, l.r)]) && (m.get(l.c, l.r) == STAIRS_UP || m.get(l.c, l.r) == STAIRS_DOWN)) mark(l.c, l.r, Color(242, 210, 122), ms + 1);
    g_.fillRect(X + (pc - c0) * k - k, Y + (pr - r0) * k - k, k * 3, k * 3, Color(255, 255, 255));
    g_.fillRect(X + (pc - c0) * k, Y + (pr - r0) * k, k, k, Color(224, 80, 60));
    g_.setTransform(ui_);
    drawStatus(x + w, y + h + 30);
}

// Place, clock and the quest tracker, under the minimap
void App::drawStatus(double right, double top) {
    Sim& sim = *sim_;
    const Map& m = sim.map();
    struct Line {
        std::string text;
        Color col;
        int weight;
    };
    std::vector<Line> lines;
    std::string place = m.name;
    if (m.outdoor) {
        const TownDef* t = db_.town(sim.townId);
        const ZoneDef* z = db_.zone(sim.zoneId);
        place = t ? t->name : z ? z->name : place;
    }
    lines.push_back({place, pal::muted, 1});
    lines.push_back({sim.clockText(), sim.isNight() ? pal::night : pal::gold, 1});
    int shown = 0;
    for (const QuestDef* q : sim.activeQuests()) {
        if (++shown > 3) break;
        const bool ready = sim.questReady(*q);
        lines.push_back({q->title, ready ? pal::ok : pal::text, 1});
        if (ready) {
            const NpcDef* n = db_.npc(q->turnIn);
            lines.push_back({"Return to " + (n ? n->name : std::string("the giver")), pal::ok, 0});
        } else
            for (size_t i = 0; i < q->objectives.size(); i++) {
                const QuestObjective& o = q->objectives[i];
                lines.push_back({o.count > 1 ? o.label + ": " + std::to_string(sim.objectiveProgress(*q, i)) + " / " + std::to_string(o.count) : o.label, pal::muted, 0});
            }
    }
    double w = 0;
    for (const auto& l : lines) w = std::max(w, textW(l.text, 12, l.weight));
    w += 22;
    const double h = lines.size() * 17.0 + 10;
    panel(right - w + 2, top - 19, w + 8, h + 8);
    for (size_t i = 0; i < lines.size(); i++) text(lines[i].text, right - 4, top + i * 17.0, lines[i].col, 12, lines[i].weight, Align::Right);
}

}   // namespace hc

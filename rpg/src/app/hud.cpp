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

    // ── The action bar along the bottom, between a globe of health and a globe of experience ──
    struct Slot {
        std::string key, icon, label, count;
        double cool = 0;
        int cdTicks = 0;
        bool locked = false, ready = false, dim = false;
    };
    std::vector<Slot> slots;
    const Item* weapon = sim.equipped(EquipSlot::Weapon);
    slots.push_back({btnLabel("attack"), weapon ? weapon->id : "", "Fists", ""});
    {
        Slot s{btnLabel("shoot"), "bow", "", sim.hasItem("bow") ? std::to_string(sim.countItem("arrows")) : ""};
        s.locked = !sim.hasItem("bow");
        s.dim = !s.locked && sim.countItem("arrows") == 0;
        slots.push_back(s);
    }
    {
        Slot s{btnLabel("skill"), "owl_shuriken", "", ""};
        s.locked = !sim.whirlwindUnlocked();
        s.cool = static_cast<double>(P.whirlCd) / std::max(1, sim.whirlCooldown());
        s.ready = !s.locked && P.whirlCd <= 0;
        s.cdTicks = P.whirlCd;
        slots.push_back(s);
    }
    for (const auto& ab : Sim::abilities()) {   // the other active skills from the tree
        Slot s{btnLabel(ab.id), ab.icon, "", ""};
        s.locked = !sim.abilityKnown(ab.id);
        s.cdTicks = sim.abilityCooldown(ab.id);
        s.cool = static_cast<double>(s.cdTicks) / std::max(1, ab.cooldown);
        s.ready = !s.locked && s.cdTicks <= 0;
        slots.push_back(s);
    }
    {
        std::string healIcon;
        for (const char* id : {"potion", "roast", "bandage", "meat"})
            if (sim.hasItem(id)) {
                healIcon = id;
                break;
            }
        const int n = sim.countItem("potion") + sim.countItem("roast") + sim.countItem("bandage") + sim.countItem("meat");
        Slot s{btnLabel("heal"), healIcon.empty() ? "potion" : healIcon, "", healIcon.empty() ? "" : std::to_string(n)};
        s.dim = healIcon.empty();
        slots.push_back(s);
    }
    static const char* QUICK[3][2] = {{"bandage", "quick1"}, {"roast", "quick2"}, {"potion", "quick3"}};
    for (const auto& qk : QUICK) {
        const int n = sim.countItem(qk[0]);
        Slot s{scheme_ != Scheme::Pad ? bindLabel(qk[1]) : "", qk[0], "", n ? std::to_string(n) : ""};
        s.dim = n == 0;
        slots.push_back(s);
    }
    {
        const std::string mount = sim.chosenMount();
        Slot s{btnLabel("mount"), mount.empty() ? "mount_boar" : mount, "", ""};
        s.locked = mount.empty();
        slots.push_back(s);
    }

    const double SS = 42, GAP = 5, ORB = 96;
    const double barW = slots.size() * (SS + GAP) - GAP + 28, barH = SS + 24;
    const double totalW = barW + ORB * 2 - 24;
    const double bx0 = std::round((uiW() - totalW) / 2) + ORB - 12, by0 = std::round(uiH() - barH - 8);
    // the bar: a dark iron plate with a brass rim, and an experience strip along its top
    panel(bx0, by0, barW, barH);
    const double xpFrac = P.level >= db_.tuning.maxLevel ? 1 : static_cast<double>(P.xp) / std::max(1, sim.xpToNext());
    box(bx0 + 12, by0 + 8, barW - 24, 5, Color(16, 12, 20), Color(8, 6, 6));
    g_.fillRect(bx0 + 13, by0 + 9, std::round((barW - 26) * xpFrac), 3, pal::xp);
    for (int i = 1; i < 10; i++) g_.fillRect(std::round(bx0 + 12 + (barW - 24) * i / 10), by0 + 8, 1, 5, Color(8, 6, 6, 200));
    for (size_t i = 0; i < slots.size(); i++) {
        const Slot& s = slots[i];
        const double x = bx0 + 14 + i * (SS + GAP), sy = by0 + 18;
        slotBox(x, sy, SS, SS, s.locked);
        if (!s.icon.empty()) {
            const bool glyph = s.icon.rfind("owl_", 0) == 0;
            g_.setAlpha(s.locked ? 0.22 : s.dim ? 0.4 : 1);
            if (glyph) drawSpr(spr_.get(s.icon), x + 7, sy + 7, 32, Color(236, 196, 110));
            else drawSpr(spr_.icon(s.icon), x + 7, sy + 7, 32);
            g_.setAlpha(1);
        } else if (!s.label.empty()) text(s.label, x + SS / 2, sy + SS / 2 + 1, pal::faint, 11, 1, Align::Center, Baseline::Middle);
        if (s.cool > 0 && !s.locked) {   // cooldown: a shadow drains away from the top
            g_.fillRect(x + 3, sy + 3, SS - 6, std::round((SS - 6) * s.cool), Color(6, 4, 4, 170));
            text(std::to_string(static_cast<int>(std::ceil(s.cdTicks / 60.0))), x + SS / 2, sy + SS / 2 + 1, pal::text, 15, 2, Align::Center, Baseline::Middle);
        }
        if (s.ready) box(x + 2, sy + 2, SS - 4, SS - 4, Color(0, 0, 0, 0), Color(255, 210, 120, static_cast<uint8_t>(90 + 70 * std::sin(tick * 0.1))), 1);
        if (!s.key.empty()) keycap(s.key, x - 3, sy - 7, 15);
        if (!s.count.empty()) {
            text(s.count, x + SS - 3, sy + SS - 3, pal::ink, 12, 2, Align::Right);
            text(s.count, x + SS - 4, sy + SS - 4, pal::text, 12, 2, Align::Right);
        }
    }

    // globes
    auto globe = [&](double gx, double gy, double frac, const Sprite& liquid, const std::string& big, const std::string& small) {
        const double S = ORB, k = S / 48.0;
        g_.draw(spr_.ui.orbBack, gx, gy, S, S);
        frac = std::clamp(frac, 0.0, 1.0);
        // the surface sways a little
        const int top = std::clamp(static_cast<int>(std::lround(48 * (1 - frac) + std::sin(tick * 0.08) * (frac > 0.02 && frac < 0.98 ? 0.6 : 0))), 0, 48);
        if (top < 48) g_.drawPart(liquid, {0, top, 48, 48 - top}, gx, gy + top * k, S, (48 - top) * k);
        if (top > 2 && top < 46) g_.fillRect(gx + 8 * k, std::round(gy + top * k), S - 16 * k, 1, Color(255, 255, 255, 40));
        g_.draw(spr_.ui.orbGlass, gx, gy, S, S);
        g_.draw(spr_.ui.orbFrame, gx, gy, S, S);
        shadowText(big, gx + S / 2, gy + S / 2 + 4, pal::ltText, 17, 2, Align::Center);
        shadowText(small, gx + S / 2, gy + S / 2 + 20, pal::ltMuted, 11, 1, Align::Center);
    };
    const int maxHp = sim.maxHp();
    auto heartsText = [](int half) { return std::to_string(half / 2) + (half % 2 ? "\xc2\xbd" : ""); };
    const bool low = P.hp <= 2 && (tick >> 4) % 2;
    const double gy = uiH() - ORB - 4;
    globe(bx0 - ORB + 14, gy, static_cast<double>(P.hp) / std::max(1, maxHp), spr_.ui.orbRed, heartsText(P.hp) + " / " + heartsText(maxHp), "hearts");
    if (low) g_.drawTinted(spr_.glow, bx0 - ORB + 4, gy - 10, ORB + 20, ORB + 20, Color(255, 40, 30, 70), true);
    const bool capped = P.level >= db_.tuning.maxLevel;
    globe(bx0 + barW - 14, gy, xpFrac, spr_.ui.orbViolet, "Level " + std::to_string(P.level), capped ? "max level" : std::to_string(P.xp) + " / " + std::to_string(sim.xpToNext()));
    if (const int pts = sim.skillPoints(); pts > 0) {   // unspent skill points nag gently, above the experience globe
        g_.setAlpha(0.75 + 0.25 * std::sin(tick * 0.1));
        shadowText("+" + std::to_string(pts) + (pts == 1 ? " skill point" : " skill points") + " · " + bindLabel("skills"), bx0 + barW - 14 + ORB / 2, gy - 8, pal::ltGold, 12, 1, Align::Center);
        g_.setAlpha(1);
    }

    // Blessings from shrines, top left
    double y = 16;
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
        const double w = std::min(520.0, uiW() - 560), x = (uiW() - w) / 2, by = 48;
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
        const double w = 220, x = (uiW() - w) / 2, by = uiH() - 200;
        shadowText("Recalling...", uiW() / 2, by - 8, pal::sky, 13, 1, Align::Center);
        bar(x, by, w, 8, 1 - static_cast<double>(P.recallT) / db_.tuning.recallTicks, pal::sky, Color(20, 40, 60));
    }
    // Toasts
    for (size_t i = 0; i < sim.toasts.size(); i++) {
        const Toast& t = sim.toasts[i];
        const double a = std::min(1.0, t.t / 30.0), ty = uiH() - 150 - (sim.toasts.size() - 1 - i) * 34.0;
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
    // Controls hint: only for the first half minute of a new game (they are always in the pause menu)
    if (sim.mode == Mode::Play && sim.tickCount < 1800 && P.level == 1) {
        std::string hint;
        if (scheme_ == Scheme::Pad) hint = "Stick move · X attack · RT crossbow · LT whirlwind · B roll · A use · Y heal · LB craft · RB skills · Back bag · Start menu";
        else if (sim.flying()) hint = "WASD fly · " + bindLabel("mount") + " land over open ground · " + bindLabel("recall") + " recall · " + bindLabel("map") + " map · Esc menu";
        else {
            const std::string atk = scheme_ == Scheme::Mouse ? "Left click" : bindLabel("attack"), shoot = scheme_ == Scheme::Mouse ? "Right click" : bindLabel("shoot");
            hint = "WASD move · " + atk + " attack · " + shoot + " crossbow · " + bindLabel("dodge") + " roll · " + bindLabel("interact") + " use · " + bindLabel("heal") + " heal · " + bindLabel("bag") +
                   " bag · " + bindLabel("craft") + " craft · " + bindLabel("quests") + " quests · " + bindLabel("map") + " map · Esc menu";
        }
        const double hw = textW(hint, 12) + 24;
        if (hw < uiW() - 20) {
            box(uiW() / 2 - hw / 2, uiH() - 120, hw, 22, pal::ltBox, pal::ltEdge, 1);
            text(hint, uiW() / 2, uiH() - 109, Color(236, 230, 216, 210), 12, 0, Align::Center, Baseline::Middle);
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
    drawStatus(x + w, y + h + 14);
}

// Place, clock and the quest tracker, under the minimap: a small iron-bound board
void App::drawStatus(double right, double top) {
    Sim& sim = *sim_;
    const Map& m = sim.map();
    std::string place = m.name;
    if (m.outdoor) {
        const TownDef* t = db_.town(sim.townId);
        const ZoneDef* z = db_.zone(sim.zoneId);
        place = t ? t->name : z ? z->name : place;
    }
    const double W = 250, X = right - W + 8, pad = 14, iw = W - pad * 2;

    // Lay the board out first, so the panel fits it exactly
    struct Row {
        enum Kind { Title, Objective, Hint } kind;
        std::string text, right;
        Color col;
        double frac = -1;
        bool done = false, ready = false, main = false;
    };
    std::vector<Row> rows;
    const auto active = sim.activeQuests();
    int shown = 0;
    for (const QuestDef* q : active) {
        if (++shown > 3) break;
        const bool ready = sim.questReady(*q);
        Row t{Row::Title, q->title, "", ready ? pal::ok : q->main ? Color(236, 196, 110) : pal::ltText};
        t.ready = ready;
        t.main = q->main;
        rows.push_back(t);
        if (ready) {
            const NpcDef* n = db_.npc(q->turnIn);
            rows.push_back({Row::Hint, "Return to " + (n ? n->name : std::string("the giver")), "", pal::ok});
            continue;
        }
        for (size_t i = 0; i < q->objectives.size(); i++) {
            const QuestObjective& o = q->objectives[i];
            const int have = std::min(o.count, sim.objectiveProgress(*q, i));
            Row r{Row::Objective, o.label, o.count > 1 ? std::to_string(have) + "/" + std::to_string(o.count) : "", have >= o.count ? pal::ok : pal::ltMuted};
            r.done = have >= o.count;
            if (o.count > 1) r.frac = static_cast<double>(have) / o.count;
            rows.push_back(r);
        }
    }
    if (active.size() > 3) rows.push_back({Row::Hint, "+" + std::to_string(active.size() - 3) + " more · " + bindLabel("quests") + " quest log", "", pal::ltMuted});
    if (active.empty()) rows.push_back({Row::Hint, "No quests. Talk to the townsfolk.", "", pal::ltMuted});

    double h = 58;
    for (const auto& r : rows) h += r.kind == Row::Title ? 22 : r.frac >= 0 ? 22 : 17;
    h += 8;

    panel(X, top, W, h);
    // header: where you are and the hour, with the sun or moon
    const bool night = sim.isNight();
    text(place, X + pad, top + 26, Color(236, 220, 190), 18, 1);
    drawSpr(spr_.get(night ? "owl_rest" : "owl_light"), X + W - pad - 18, top + 10, 18, night ? Color(150, 170, 230) : Color(240, 200, 110));
    text(sim.clockText(), X + W - pad - 28, top + 24, night ? Color(150, 170, 230) : pal::ltMuted, 12, 1, Align::Right);
    ornament(X + W / 2, top + 38, iw);
    double y = top + 60;
    for (const auto& r : rows) {
        switch (r.kind) {
            case Row::Title: {
                // a gold "!" for a quest under way, a "?" when it is ready to hand in
                drawSpr(spr_.get(r.ready ? "owl_question" : "owl_exclamation"), X + pad - 4, y - 14, 16, r.ready ? pal::ok : r.main ? Color(236, 196, 110) : Color(200, 180, 140));
                std::string t = r.text;
                while (t.size() > 3 && textW(t, 13, 1) > iw - 16) t = t.substr(0, t.size() - 4) + "...";
                text(t, X + pad + 14, y, r.col, 13, 1);
                y += 22;
                break;
            }
            case Row::Objective: {
                box(X + pad + 15, y - 8, 7, 7, r.done ? pal::ok : Color(0, 0, 0, 0), r.done ? pal::ok : Color(150, 118, 66));
                std::string t = r.text;
                const double rw = r.right.empty() ? 0 : textW(r.right, 11, 1) + 8;
                while (t.size() > 3 && textW(t, 12) > iw - 30 - rw) t = t.substr(0, t.size() - 4) + "...";
                text(t, X + pad + 28, y, r.col, 12);
                if (!r.right.empty()) text(r.right, X + W - pad, y, r.done ? pal::ok : pal::ltMuted, 11, 1, Align::Right);
                if (r.frac >= 0) {
                    bar(X + pad + 28, y + 5, iw - 28, 3, r.frac, r.done ? pal::ok : Color(196, 150, 70), Color(24, 18, 14));
                    y += 22;
                } else
                    y += 17;
                break;
            }
            case Row::Hint:
                text(r.text, X + pad + 14, y, r.col, 12);
                y += 17;
                break;
        }
    }
}

}   // namespace hc

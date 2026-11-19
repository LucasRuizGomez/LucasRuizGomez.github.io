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
}   // namespace

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

// The map you are on: by default only the region you stand in, drawn large; Tab shows the whole map. Fog over what
// you have not walked yet, marks for towns, dungeons, the ways to other maps and where your quests point.
void App::drawWorldMap() {
    Sim& sim = *sim_;
    dim(0.7);
    // which outdoor map: the one you are on, or the one a dungeon opens onto
    std::string mapId = sim.map().outdoor ? sim.mapId : "over";
    if (const DungeonDef* d = sim.map().dungeon) {
        while (d && !d->under.empty()) d = db_.dungeon(d->under);
        if (d) mapId = d->realm;
    }
    if (!sim.maps.count(mapId)) mapId = "over";
    Map& over = sim.maps.at(mapId);
    const Sprite& mini = world_->minimap(sim, over);
    // the region: its bounding box, a little padded
    int zi = -1;
    if (sim.map().outdoor) zi = over.zone[over.idx(std::clamp(ifloor(sim.P.x / TILE), 0, over.w - 1), std::clamp(ifloor(sim.P.y / TILE), 0, over.h - 1))];
    else if (const ZoneDef* z = db_.zone(sim.zoneId)) zi = z->index;
    int c0 = 0, r0 = 0, c1 = over.w - 1, r1 = over.h - 1;
    if (!mapWhole_ && zi >= 0) {
        c0 = over.w, r0 = over.h, c1 = 0, r1 = 0;
        for (int r = 0; r < over.h; r++)
            for (int c = 0; c < over.w; c++)
                if (over.zone[over.idx(c, r)] == zi) {
                    c0 = std::min(c0, c), c1 = std::max(c1, c);
                    r0 = std::min(r0, r), r1 = std::max(r1, r);
                }
        c0 = std::max(0, c0 - 4), r0 = std::max(0, r0 - 4), c1 = std::min(over.w - 1, c1 + 4), r1 = std::min(over.h - 1, r1 + 4);
        if (c1 < c0) c0 = 0, r0 = 0, c1 = over.w - 1, r1 = over.h - 1;
    }
    const int vw = c1 - c0 + 1, vh = r1 - r0 + 1;
    const double maxW = uiW() - 80, maxH = uiH() - 130;
    const double k = std::max(1.0, std::floor(std::min(maxW * ui_ / vw, maxH * ui_ / vh)));
    const double w = vw * k / ui_, h = vh * k / ui_, X = std::round((uiW() - w) / 2), Y = std::round((uiH() - h) / 2) + 14;
    panel(X - 16, Y - 54, w + 32, h + 92);
    const ZoneDef* here = zi >= 0 && zi < static_cast<int>(db_.zones.size()) ? &db_.zones[zi] : nullptr;
    text(mapWhole_ || !here ? over.name : here->name, X, Y - 20, pal::text, 22, 1);
    if (here && !mapWhole_) text("Levels " + std::to_string(here->levelMin) + " to " + std::to_string(here->levelMax), X + w, Y - 20, pal::muted, 13, 1, Align::Right);
    const int PX = static_cast<int>(std::lround(X * ui_)), PY = static_cast<int>(std::lround(Y * ui_));
    g_.setTransform(1);
    g_.fillRect(PX, PY, vw * k, vh * k, Color(24, 20, 30));
    g_.drawPart(mini, {c0, r0, vw, vh}, PX, PY, vw * k, vh * k);
    if (!mapWhole_ && zi >= 0)   // other regions are shown, but in shadow
        for (int r = r0; r <= r1; r++)
            for (int c = c0; c <= c1; c++)
                if (over.zone[over.idx(c, r)] != zi) g_.fillRect(PX + (c - c0) * k, PY + (r - r0) * k, k, k, Color(8, 6, 10, 170));
    g_.setTransform(ui_);
    auto seen = [&](int c, int r) { return over.inside(c, r) && over.explored[over.idx(c, r)] && c >= c0 && c <= c1 && r >= r0 && r <= r1; };
    auto pos = [&](int c, int r) { return Vec2{X + (c - c0 + 0.5) * k / ui_, Y + (r - r0 + 0.5) * k / ui_}; };

    if (mapWhole_)
        for (const auto& z : db_.zones) {
            if (z.realm != mapId || !sim.zonesFound.count(z.id) || z.centers.empty()) continue;
            const Vec2 p = pos(z.centers[0].c, z.centers[0].r);
            g_.setAlpha(0.75);
            shadowText(z.name, p.x, p.y - 34, pal::ltText, 15, 2, Align::Center);
            shadowText("Levels " + std::to_string(z.levelMin) + " to " + std::to_string(z.levelMax), p.x, p.y - 20, pal::ltMuted, 11, 1, Align::Center);
            g_.setAlpha(1);
        }
    for (const auto& t : db_.towns) {
        if (t.realm != mapId || !seen(t.at.c, t.at.r)) continue;
        const Vec2 p = pos(t.at.c, t.at.r);
        box(p.x - 5, p.y - 5, 10, 10, sim.waypoints.count(t.id) ? Color(120, 200, 255) : Color(214, 210, 200), pal::ink, 2);
        shadowText(t.name, p.x, p.y + 20, pal::ltGold, 13, 1, Align::Center);
    }
    for (const auto& d : db_.dungeons) {
        if (d.realm != mapId || !d.under.empty() || !seen(d.at.c, d.at.r)) continue;
        const Vec2 p = pos(d.at.c, d.at.r);
        box(p.x - 4, p.y - 4, 8, 8, sim.bossesDefeated.count(d.id) ? Color(120, 120, 130) : Color(220, 130, 255), pal::ink, 2);
        shadowText(d.name, p.x, p.y - 8, Color(232, 200, 255), 12, 1, Align::Center);
    }
    for (const auto& o : over.objects) {   // the ways to other maps
        if (o.kind != ObjKind::Travel || !seen(o.c, o.r)) continue;
        const Vec2 p = pos(o.c, o.r);
        const bool open = o.res.empty() || sim.bossesDefeated.count(o.res);
        box(p.x - 4, p.y - 4, 8, 8, open ? Color(255, 170, 90) : Color(110, 90, 80), pal::ink, 2);
        const RealmDef* r = db_.realm(o.id);
        shadowText(r ? r->name : (o.id == "over" ? "The Realm" : o.id), p.x, p.y + 18, open ? Color(255, 200, 140) : pal::ltMuted, 11, 1, Align::Center);
    }
    // Where active quests want you to go (on this map)
    for (const QuestDef* q : sim.activeQuests()) {
        TilePos target{-1, -1};
        std::string where = "over";
        if (sim.questReady(*q)) {
            if (const NpcDef* n = db_.npc(q->turnIn))
                if (const TownDef* t = db_.town(n->town)) target = t->at, where = t->realm;
        } else
            for (const auto& o : q->objectives) {
                if (o.type == "visit")
                    if (const TownDef* t = db_.town(o.target)) target = t->at, where = t->realm;
                if (o.type == "boss")
                    if (const DungeonDef* d = db_.dungeon(o.target)) target = d->at, where = d->realm;
            }
        if (target.c < 0 || where != mapId || target.c < c0 || target.c > c1 || target.r < r0 || target.r > r1) continue;
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
        if (d && d->at.c >= c0 && d->at.c <= c1 && d->at.r >= r0 && d->at.r <= r1) {
            const Vec2 p = pos(d->at.c, d->at.r);
            box(p.x - 5, p.y - 5, 10, 10, Color(224, 80, 60), Color(255, 255, 255), 2);
        }
    }
    text(std::string(mapWhole_ ? "Tab: this region" : "Tab: the whole map") + " · Blue: waypoint · Purple: dungeon · Orange: the way to another land · " + bindLabel("map") +
             " or Esc close",
         X + w / 2, Y + h + 22, pal::muted, 12, 0, Align::Center);
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

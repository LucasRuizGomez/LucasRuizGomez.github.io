// Conversations and the quest log. Speaking to someone opens a window with a line of their own and what they
// have for you: new quests (accept or decline), quests to hand in (pick your reward), trade, goodbye.
#include "app/app.h"
#include "app/palette.h"

#include <cmath>

namespace hc {

namespace {
const Color QUEST_GOLD(236, 190, 84);
}

// ───────────────────────────── shared pieces ─────────────────────────────

std::string App::questStatus(const QuestDef& q) const {
    const Sim& sim = *sim_;
    if (sim.questReady(q)) return "Ready to hand in";
    std::string s;
    for (size_t i = 0; i < q.objectives.size(); i++) {
        const auto& o = q.objectives[i];
        if (o.count > 1) s += (s.empty() ? "" : " · ") + std::to_string(sim.objectiveProgress(q, i)) + "/" + std::to_string(o.count);
    }
    return s.empty() ? (q.main ? "Main story" : "Side quest") : s;
}

Color App::questLevelColor(int level) const {
    const int d = level - sim_->P.level;
    if (d >= 10) return Color(226, 84, 64);
    if (d >= 4) return Color(232, 146, 64);
    if (d >= -4) return Color(230, 204, 92);
    if (d >= -15) return Color(122, 194, 102);
    return Color(140, 132, 120);
}

void App::ornament(double cx, double y, double w) {
    const Color brass(150, 118, 66), hi(222, 184, 104), shadow(0, 0, 0, 120);
    const double l = std::round(cx - w / 2), half = std::round(w / 2 - 8);
    g_.fillRect(l, std::round(y), half, 1, brass);
    g_.fillRect(std::round(cx + 8), std::round(y), half, 1, brass);
    g_.fillRect(l, std::round(y + 1), half, 1, shadow);
    g_.fillRect(std::round(cx + 8), std::round(y + 1), half, 1, shadow);
    for (int i = 0; i < 4; i++) g_.fillRect(std::round(cx - 4 + i), std::round(y - i), 8 - 2 * i, 1, i == 3 ? hi : brass);   // the diamond
    for (int i = 1; i < 4; i++) g_.fillRect(std::round(cx - 4 + i), std::round(y + i), 8 - 2 * i, 1, brass);
    g_.fillRect(l - 3, std::round(y - 1), 3, 3, brass);
    g_.fillRect(std::round(cx + w / 2), std::round(y - 1), 3, 3, brass);
}

// A heading with a small icon and a rule running out to the right; returns the y for what follows
double App::sectionHead(const std::string& label, const std::string& icon, double x, double y, double w) {
    double tx = x;
    if (!icon.empty()) {
        drawSpr(spr_.get(icon), x - 2, y - 17, 22, pal::gold);
        tx += 24;
    }
    text(label, tx, y, pal::gold, 15, 1);
    const double lx = std::round(tx + textW(label, 15, 1) + 10);
    g_.fillRect(lx, std::round(y - 5), std::round(x + w - lx), 1, Color(110, 88, 54));
    g_.fillRect(lx, std::round(y - 4), std::round(x + w - lx), 1, Color(0, 0, 0, 110));
    return y + 24;
}

// Objectives with checks, counts and progress bars; returns the y below them
double App::drawObjectives(const QuestDef& q, double x, double y, double w, bool progress) {
    Sim& sim = *sim_;
    if (!q.objectiveText.empty()) {
        for (const auto& l : wrap(q.objectiveText, w, 14)) {
            text(l, x, y, pal::muted, 14);
            y += 19;
        }
        y += 6;
    }
    for (size_t i = 0; i < q.objectives.size(); i++) {
        const QuestObjective& o = q.objectives[i];
        const int have = progress ? std::min(o.count, sim.objectiveProgress(q, i)) : 0;
        const bool done = progress && have >= o.count;
        box(x + 2, y - 12, 13, 13, pal::well, done ? pal::ok : Color(150, 118, 66));   // a brass-rimmed box, ticked when done
        if (done) drawSpr(spr_.get("owl_tick"), x - 1, y - 15, 19, pal::ok);
        text(o.label, x + 24, y, done ? pal::ok : pal::text, 14, 1);
        if (o.count > 1) {
            const std::string n = progress ? std::to_string(have) + " / " + std::to_string(o.count) : "x" + std::to_string(o.count);
            text(n, x + w, y, done ? pal::ok : pal::muted, 13, 1, Align::Right);
            if (progress) {
                const double bw = std::min(160.0, w * 0.3), bx = x + w - textW(n, 13, 1) - bw - 12;
                bar(bx, y - 7, bw, 5, static_cast<double>(have) / o.count, done ? pal::ok : Color(196, 150, 70), Color(30, 24, 20));
            }
        }
        y += 23;
    }
    return y;
}

// Rewards: experience and gold chips, items always given, and the items to choose one from. Returns the y below.
double App::drawRewards(const QuestDef& q, double x, double y, double w, int pick, bool choosing) {
    auto itemBox = [&](const std::string& id, int qty, double bx, double by, double bw, bool sel, bool dim) {
        box(bx, by, bw, 44, sel ? pal::slotSel : pal::well, sel ? pal::sel : pal::line, sel ? 2 : 1);
        slotBox(bx + 4, by + 4, 36, 36, false);
        if (dim) g_.setAlpha(0.45);
        drawSpr(spr_.icon(id), bx + 6, by + 6, 32);
        const ItemDef& d = db_.item(id);
        const auto lines = wrap(d.name, bw - 80, 13, 1);
        for (size_t i = 0; i < lines.size() && i < 2; i++) text(lines[i], bx + 50, by + (lines.size() > 1 ? 19 : 27) + i * 15.0, sel ? pal::gold : pal::text, 13, 1);
        if (qty > 1) {
            text(std::to_string(qty), bx + 40, by + 40, pal::ink, 12, 2, Align::Right);
            text(std::to_string(qty), bx + 39, by + 39, pal::text, 12, 2, Align::Right);
        }
        g_.setAlpha(1);
        if (sel) drawSpr(spr_.get("owl_tick"), bx + bw - 28, by + 11, 22, pal::gold);
    };
    auto chip = [&](double cx, const std::string& icon, const std::string& label, Color col) {
        const double cw = textW(label, 14, 1) + 40;
        box(cx, y - 16, cw, 24, pal::well, pal::line);
        drawSpr(spr_.get(icon), cx + 3, y - 15, 22, col);
        text(label, cx + 30, y + 1, col, 14, 1);
        return cx + cw + 8;
    };
    const double cx = chip(x, "owl_brave", std::to_string(q.xp) + " experience", Color(190, 160, 240));
    if (q.gold) chip(cx, "owl_coins", std::to_string(q.gold) + " gold", pal::gold);
    y += 16;
    const double half = std::floor((w - 8) / 2);
    if (!q.items.empty()) {
        text("You will receive:", x, y + 12, pal::muted, 12, 1);
        y += 20;
        for (size_t i = 0; i < q.items.size(); i++) itemBox(q.items[i].first, q.items[i].second, x + (i % 2) * (half + 8), y + (i / 2) * 50.0, half, false, false);
        y += ((q.items.size() + 1) / 2) * 50.0;
    }
    if (!q.choice.empty()) {
        text(choosing ? "Choose one of these rewards:" : "You will be able to choose one of these:", x, y + 12, choosing ? pal::gold : pal::muted, 12, 1);
        y += 20;
        for (size_t i = 0; i < q.choice.size(); i++) {
            const double bx = x + (i % 2) * (half + 8), by = y + (i / 2) * 50.0;
            itemBox(q.choice[i].first, q.choice[i].second, bx, by, half, choosing && static_cast<int>(i) == pick, choosing && pick >= 0 && static_cast<int>(i) != pick);
            if (choosing) {
                const int k = static_cast<int>(i);
                hit(bx, by, half, 44, [this, k] { rewardPick_ = k; });
            }
        }
        y += ((q.choice.size() + 1) / 2) * 50.0;
    }
    return y;
}

// ───────────────────────────── conversation ─────────────────────────────

std::vector<App::TalkOption> App::talkOptions() {
    Sim& sim = *sim_;
    std::vector<TalkOption> out;
    const std::string npc = sim.talkNpc;
    for (const QuestDef* q : sim.questsToHandIn(npc)) {
        const std::string id = q->id;
        out.push_back({"owl_question", q->title, QUEST_GOLD, [this, id] {
                           talkView_ = TalkView::Complete;
                           talkQuest_ = id;
                           rewardPick_ = 0;
                       }});
    }
    for (const QuestDef* q : sim.questsOffered(npc)) {
        const std::string id = q->id;
        out.push_back({"owl_exclamation", q->title + (q->main ? "  (main story)" : ""), QUEST_GOLD, [this, id] {
                           talkView_ = TalkView::Offer;
                           talkQuest_ = id;
                       }});
    }
    for (const QuestDef* q : sim.questsUnderway(npc)) {
        const std::string id = q->id;
        out.push_back({"owl_question", q->title, pal::faint, [this, id] {
                           talkView_ = TalkView::Progress;
                           talkQuest_ = id;
                       }});
    }
    if (const NpcDef* d = db_.npc(npc); d && !d->shop.empty())
        out.push_back({"owl_coins", d->role == "stable" ? "Show me your animals." : "Let me see what you have for sale.", pal::text, [this] {
                           const std::string n = sim_->talkNpc;
                           sim_->openShop(n);
                           shopBuying_ = true;
                           shopSel_ = 0;
                       }});
    out.push_back({"owl_cancel", "Goodbye.", pal::muted, [this] { sim_->mode = Mode::Play; }});
    return out;
}

void App::talkKey(const std::string& code, const std::string& dir, bool enter) {
    Sim& sim = *sim_;
    const bool back = code == "Escape";
    switch (talkView_) {
        case TalkView::Menu: {
            const auto opts = talkOptions();
            const int n = static_cast<int>(opts.size());
            if (dir == "down") talkSel_ = (talkSel_ + 1) % n;
            if (dir == "up") talkSel_ = (talkSel_ + n - 1) % n;
            if (enter || code == "Space") {
                const auto act = opts[std::clamp(talkSel_, 0, n - 1)].act;
                act();
            }
            if (back) sim.mode = Mode::Play;
            break;
        }
        case TalkView::Offer:
            if (enter || code == "KeyY") {
                sim.acceptQuest(talkQuest_);
                talkView_ = TalkView::Menu;
                talkSel_ = 0;
            } else if (back || code == "KeyN") talkView_ = TalkView::Menu;
            break;
        case TalkView::Progress:
            if (enter || back) talkView_ = TalkView::Menu;
            break;
        case TalkView::Complete: {
            const QuestDef* q = db_.quest(talkQuest_);
            const int n = q ? static_cast<int>(q->choice.size()) : 0;
            if (n && (dir == "right" || dir == "down")) rewardPick_ = (rewardPick_ + 1) % n;
            if (n && (dir == "left" || dir == "up")) rewardPick_ = (rewardPick_ + n - 1) % n;
            if (enter) {
                if (n && rewardPick_ < 0) {
                    sim.toast("Choose your reward first.");
                    sim.play("deny");
                } else if (sim.turnInQuest(talkQuest_, rewardPick_)) {
                    talkView_ = TalkView::Menu;
                    talkSel_ = 0;
                }
            }
            if (back) talkView_ = TalkView::Menu;
            break;
        }
    }
}

void App::drawTalk() {
    Sim& sim = *sim_;
    const NpcDef* npc = db_.npc(sim.talkNpc);
    if (!npc) {
        sim.mode = Mode::Play;
        return;
    }
    dim(0.55);
    const QuestDef* q = db_.quest(talkQuest_);
    if (talkView_ != TalkView::Menu && !q) talkView_ = TalkView::Menu;
    // Each view is as tall as what it holds (measured on the previous frame)
    const double W = std::min(760.0, uiW() - 40), H = std::min(talkH_ > 0 ? talkH_ : 420.0, uiH() - 30);
    const double X = std::round((uiW() - W) / 2), Y = std::round((uiH() - H) / 2);
    panel(X, Y, W, H);
    closeButton(X + W - 40, Y + 14);

    // ── the speaker: portrait in a brass frame, name, where they live ──
    box(X + 12, Y + 12, W - 24, 100, Color(14, 11, 10, 150));
    box(X + 22, Y + 20, 84, 84, Color(150, 118, 66), Color(10, 8, 7), 1);
    box(X + 25, Y + 23, 78, 78, Color(30, 26, 30), Color(70, 54, 34), 1);
    g_.drawTinted(spr_.glow, X + 30, Y + 28, 68, 68, Color(255, 190, 110, 40), true);
    {
        const CharSet& cs = spr_.character(npc->sprite, npc->tint);
        const Frames& idle = cs.get("idle");
        if (!idle.empty()) drawSpr(idle[(sim.tickCount / 14) % idle.size()], X + 22, Y + 16, 84);
    }
    text(npc->name, X + 124, Y + 52, pal::text, 24, 1);
    std::string where;
    if (const BuildingDef* b = db_.building(npc->building)) where = b->name;
    if (const TownDef* t = db_.town(npc->town)) where += (where.empty() ? "" : ", ") + t->name;
    text(where, X + 124, Y + 74, pal::muted, 13);
    if (q && talkView_ != TalkView::Menu) {   // what kind of talk this is
        const std::string tag = talkView_ == TalkView::Offer ? "New quest" : talkView_ == TalkView::Progress ? "Quest under way" : "Quest complete";
        const double tw = textW(tag, 12, 1) + 36, tx = X + W - 64 - tw;
        box(tx, Y + 66, tw, 24, pal::well, Color(150, 118, 66));
        drawSpr(spr_.get(talkView_ == TalkView::Offer ? "owl_exclamation" : "owl_question"), tx + 2, Y + 67, 22, talkView_ == TalkView::Progress ? pal::muted : pal::gold);
        text(tag, tx + 28, Y + 82, talkView_ == TalkView::Progress ? pal::muted : pal::gold, 12, 1);
    }

    const double cx = X + 30, cw = W - 60;
    double y = Y + 140;

    if (talkView_ == TalkView::Menu) {
        // what they say, in a sunken letter
        const auto lines = wrap(sim.talkLine, cw - 44, 16);
        box(cx, y - 14, cw, lines.size() * 22.0 + 22, pal::well, pal::line);
        text("\"", cx + 10, y + 14, Color(150, 118, 66), 30, 1);
        for (const auto& l : lines) {
            text(l, cx + 30, y + 9, pal::text, 16);
            y += 22;
        }
        y += 30;
        const auto opts = talkOptions();
        talkSel_ = std::clamp(talkSel_, 0, static_cast<int>(opts.size()) - 1);
        for (size_t i = 0; i < opts.size(); i++) {
            const bool sel = static_cast<int>(i) == talkSel_;
            if (sel) {
                box(cx, y, cw, 34, Color(80, 60, 34, 150), Color(176, 138, 70));
                g_.fillRect(cx, y, 3, 34, Color(236, 196, 110));
            } else box(cx, y, cw, 34, Color(0, 0, 0, 0), Color(60, 48, 34));
            drawSpr(spr_.get(opts[i].icon), cx + 8, y + 3, 28, opts[i].col);
            text(opts[i].label, cx + 46, y + 17, opts[i].col, 15, 1, Align::Left, Baseline::Middle);
            const int k = static_cast<int>(i);
            hit(cx, y, cw, 34, [this, k] {
                if (talkSel_ == k || scheme_ == Scheme::Mouse) {
                    talkSel_ = k;
                    const auto o = talkOptions();
                    if (k < static_cast<int>(o.size())) {
                        const auto act = o[k].act;
                        act();
                    }
                } else talkSel_ = k;
            }, [this, k] { talkSel_ = k; });
            y += 40;
        }
        y += 14;
        text("Up / Down choose · Enter select · Esc leave", X + W / 2, y + 6, pal::faint, 12, 0, Align::Center);
        talkH_ = y + 24 - Y;
        return;
    }

    // ── a quest: offered, under way, or ready to hand in ──
    const Color lc = questLevelColor(q->level);
    text(q->title, X + W / 2, y + 4, pal::gold, 26, 1, Align::Center);
    y += 22;
    {
        const std::string lv = "Level " + std::to_string(q->level), kind = q->main ? "Main story" : "Side quest";
        const double lw = textW(lv, 12, 1) + 16, kw = textW(kind, 12, 1), sx = std::round(X + W / 2 - (lw + 10 + kw) / 2);
        box(sx, y - 3, lw, 18, Color(lc.r, lc.g, lc.b, 40), lc);
        text(lv, sx + lw / 2, y + 6, lc, 12, 1, Align::Center, Baseline::Middle);
        text(kind, sx + lw + 10, y + 11, q->main ? pal::gold : pal::muted, 12, 1);
    }
    y += 30;
    ornament(X + W / 2, y, cw * 0.7);
    y += 26;
    const std::string body = talkView_ == TalkView::Offer ? q->offer : talkView_ == TalkView::Progress ? (q->progress.empty() ? q->offer : q->progress) : (q->done.empty() ? "Well done." : q->done);
    for (const auto& l : wrap(body, cw, 15)) {
        text(l, cx, y, pal::text, 15);
        y += 21;
    }
    y += 16;
    if (talkView_ != TalkView::Complete) {
        y = sectionHead(talkView_ == TalkView::Offer ? "Objectives" : "Progress", "owl_scroll", cx, y, cw);
        y = drawObjectives(*q, cx, y, cw, talkView_ == TalkView::Progress) + 8;
    }
    y = sectionHead("Rewards", "owl_chest", cx, y, cw);
    y = drawRewards(*q, cx, y, cw, rewardPick_, talkView_ == TalkView::Complete) + 16;

    // ── buttons, under a rule ──
    ornament(X + W / 2, y, cw);
    y += 16;
    const double by = y;
    auto btn = [&](double bx, double bw, const std::string& label, bool primary, std::function<void()> act) {
        button(bx, by, bw, 34, label, primary, 15);
        hit(bx, by, bw, 34, std::move(act));
    };
    std::string help;
    if (talkView_ == TalkView::Offer) {
        btn(cx, 150, "Accept", true, [this] { talkKey("Enter", "", true); });
        btn(cx + cw - 150, 150, "Decline", false, [this] { talkKey("Escape", "", false); });
        help = "Enter accept · Esc decline";
    } else if (talkView_ == TalkView::Progress) {
        btn(cx, 150, "Back", false, [this] { talkKey("Escape", "", false); });
        if (q->turnIn != sim.talkNpc)
            if (const NpcDef* t = db_.npc(q->turnIn)) help = "Hand it in to " + t->name + ".";
    } else {
        btn(cx, 180, "Complete quest", true, [this] { talkKey("Enter", "", true); });
        btn(cx + cw - 150, 150, "Back", false, [this] { talkKey("Escape", "", false); });
        help = q->choice.empty() ? "Enter complete · Esc back" : "Left / Right choose · Enter complete · Esc back";
    }
    text(help, X + W / 2, by + 22, pal::faint, 12, 0, Align::Center);
    talkH_ = by + 34 + 20 - Y;
}

// ───────────────────────────── quest log ─────────────────────────────

std::vector<const QuestDef*> App::questLogList() const {
    // Main story first, then side quests grouped by the town they come from
    std::vector<const QuestDef*> list = sim_->activeQuests();
    std::stable_sort(list.begin(), list.end(), [this](const QuestDef* a, const QuestDef* b) {
        if (a->main != b->main) return a->main;
        return sim_->questGiverTown(*a) < sim_->questGiverTown(*b);
    });
    return list;
}

void App::drawQuests() {
    Sim& sim = *sim_;
    dim(0.6);
    const double W = std::min(980.0, uiW() - 40), H = std::min(600.0, uiH() - 40), X = std::round((uiW() - W) / 2), Y = std::round((uiH() - H) / 2);
    panel(X, Y, W, H);
    text("Quest log", X + 24, Y + 38, pal::text, 22, 1);
    int doneCount = 0;
    for (const auto& [id, st] : sim.quests) doneCount += st.state == 2;
    const auto list = questLogList();
    text(std::to_string(list.size()) + " under way · " + std::to_string(doneCount) + " of " + std::to_string(db_.quests.size()) + " finished", X + W - 56, Y + 38, pal::faint, 13, 1, Align::Right);
    closeButton(X + W - 40, Y + 14);
    const double LW = 320, ly = Y + 60, lh = H - 60 - 46;
    questSel_ = std::clamp(questSel_, 0, std::max(0, static_cast<int>(list.size()) - 1));

    // The list, with a header whenever the group changes
    box(X + 24, ly, LW, lh, pal::well, pal::line);
    struct Line {
        bool header;
        std::string label;
        int index;
    };
    std::vector<Line> lines;
    std::string group = "\x01";
    for (size_t i = 0; i < list.size(); i++) {
        const std::string g = list[i]->main ? "Main story" : sim.questGiverTown(*list[i]);
        if (g != group) {
            lines.push_back({true, g.empty() ? "Elsewhere" : g, -1});
            group = g;
        }
        lines.push_back({false, "", static_cast<int>(i)});
    }
    const double rowH = 34;
    const int fit = static_cast<int>((lh - 12) / rowH);
    int selLine = 0;
    for (size_t i = 0; i < lines.size(); i++)
        if (lines[i].index == questSel_) selLine = static_cast<int>(i);
    questScroll_ = std::clamp(questScroll_, std::max(0, selLine - fit + 1), selLine > 0 && lines[selLine - 1].header ? selLine - 1 : selLine);
    questScroll_ = std::clamp(questScroll_, 0, std::max(0, static_cast<int>(lines.size()) - fit));
    for (int i = questScroll_; i < std::min(static_cast<int>(lines.size()), questScroll_ + fit); i++) {
        const double y = ly + 6 + (i - questScroll_) * rowH;
        const Line& L = lines[i];
        if (L.header) {
            text(L.label, X + 36, y + 22, QUEST_GOLD, 14, 2);
            continue;
        }
        const QuestDef& q = *list[L.index];
        const bool sel = L.index == questSel_;
        row(X + 30, y + 2, LW - 12, rowH - 4, sel);
        hit(X + 30, y + 2, LW - 12, rowH - 4, [this, k = L.index] {
            questSel_ = k;
            confirmAbandon_ = false;
        });
        const bool ready = sim.questReady(q);
        text("[" + std::to_string(q.level) + "]", X + 40, y + 18, q.level > sim.P.level + 8 ? pal::bad : q.level + 15 < sim.P.level ? pal::faint : pal::muted, 12, 1, Align::Left, Baseline::Middle);
        const auto t = wrap(q.title, LW - 150, 13, 1);
        text(t.empty() ? q.title : t.front(), X + 72, y + 18, pal::text, 13, 1, Align::Left, Baseline::Middle);
        text(ready ? "Done" : questStatus(q), X + 24 + LW - 14, y + 18, ready ? pal::ok : pal::faint, 11, 1, Align::Right, Baseline::Middle);
    }

    // The selected quest in full
    const double dx = X + 24 + LW + 18, dw = W - (dx - X) - 24;
    box(dx, ly, dw, lh, pal::well, pal::line);
    if (list.empty()) {
        text("No quests yet.", dx + dw / 2, ly + 60, pal::muted, 15, 1, Align::Center);
        text("People with a yellow ! over their heads have work for you.", dx + dw / 2, ly + 86, pal::faint, 13, 0, Align::Center);
    } else {
        const QuestDef& q = *list[questSel_];
        const NpcDef* giver = db_.npc(q.giver);
        const NpcDef* turnIn = db_.npc(q.turnIn);
        const TownDef* town = turnIn ? db_.town(turnIn->town) : nullptr;
        double y = ly + 32;
        text(q.title, dx + 18, y, pal::gold, 19, 1);
        text("Level " + std::to_string(q.level), dx + dw - 18, y, pal::muted, 12, 1, Align::Right);
        y += 20;
        text("From " + (giver ? giver->name : std::string("?")) + (sim.questGiverTown(q).empty() ? "" : " of " + sim.questGiverTown(q)), dx + 18, y, pal::muted, 12, 1);
        y += 26;
        for (const auto& l : wrap(q.offer, dw - 36, 13)) {
            text(l, dx + 18, y, pal::text, 13);
            y += 17;
        }
        y += 12;
        text("Objectives", dx + 18, y, QUEST_GOLD, 14, 1);
        y += 22;
        y = drawObjectives(q, dx + 18, y, dw - 36, true);
        text(sim.questReady(q) ? std::string("Return to ") + (turnIn ? turnIn->name : "?") + (town ? " in " + town->name : "") + "."
                               : std::string("Hand in to ") + (turnIn ? turnIn->name : "?") + (town ? " in " + town->name : "") + ".",
             dx + 18, y + 4, sim.questReady(q) ? pal::ok : pal::muted, 13, 1);
        y += 26;
        text("Rewards", dx + 18, y, QUEST_GOLD, 14, 1);
        y += 20;
        drawRewards(q, dx + 18, y, dw - 36, -1, false);
        if (!q.main) {
            const double bx = dx + dw - 150, by = ly + lh - 42;
            button(bx, by, 132, 30, confirmAbandon_ ? "Really abandon?" : "Abandon", confirmAbandon_, 13);
            hit(bx, by, 132, 30, [this] { questKeyAbandon(); });
        }
    }
    text("Up / Down choose · X abandon · " + bindLabel("map") + " world map · " + bindLabel("quests") + " or Esc close", X + W / 2, Y + H - 18, pal::muted, 12, 0, Align::Center);
}

void App::questKeyAbandon() {
    const auto list = questLogList();
    if (questSel_ < 0 || questSel_ >= static_cast<int>(list.size()) || list[questSel_]->main) return;
    if (!confirmAbandon_) {
        confirmAbandon_ = true;
        return;
    }
    confirmAbandon_ = false;
    sim_->abandonQuest(list[questSel_]->id);
}

}   // namespace hc

// Conversations and the quest log. Speaking to someone opens a window with a line of their own and what they
// have for you: new quests (accept or decline), quests to hand in (pick your reward), trade, goodbye.
#include "app/app.h"
#include "app/palette.h"

#include <cmath>

namespace hc {

namespace {
const Color QUEST_GOLD(196, 140, 20);
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

// Objectives with ticks and counts; returns the y below them
double App::drawObjectives(const QuestDef& q, double x, double y, double w, bool progress) {
    Sim& sim = *sim_;
    if (!q.objectiveText.empty()) {
        for (const auto& l : wrap(q.objectiveText, w, 14)) {
            text(l, x, y, pal::text, 14);
            y += 19;
        }
        y += 4;
    }
    for (size_t i = 0; i < q.objectives.size(); i++) {
        const QuestObjective& o = q.objectives[i];
        const int have = progress ? sim.objectiveProgress(q, i) : 0;
        const bool done = progress && have >= o.count;
        box(x, y - 11, 12, 12, done ? pal::ok : pal::well, pal::line);
        std::string label = o.label;
        if (o.count > 1) label += progress ? "   " + std::to_string(have) + " / " + std::to_string(o.count) : "   x" + std::to_string(o.count);
        text(label, x + 20, y, done ? pal::ok : pal::text, 14, 1);
        y += 21;
    }
    return y;
}

// Rewards: xp and gold, items always given, and the items to choose one from. pick >= -1 makes the choice
// boxes clickable (pick = the chosen one). Returns the y below.
double App::drawRewards(const QuestDef& q, double x, double y, double w, int pick, bool choosing) {
    auto itemBox = [&](const std::string& id, int qty, double bx, double by, double bw, bool sel, bool dim) {
        box(bx, by, bw, 40, sel ? pal::slotSel : pal::slot, sel ? pal::sel : pal::line, sel ? 2 : 1);
        if (dim) g_.setAlpha(0.55);
        drawSpr(spr_.icon(id), bx + 4, by + 4, 32);
        const ItemDef& d = db_.item(id);
        const auto lines = wrap(d.name, bw - 48, 12, 1);
        for (size_t i = 0; i < lines.size() && i < 2; i++) text(lines[i], bx + 42, by + (lines.size() > 1 ? 16 : 24) + i * 14.0, pal::text, 12, 1);
        if (qty > 1) {
            text(std::to_string(qty), bx + 36, by + 36, pal::ltText, 12, 2, Align::Right);
            text(std::to_string(qty), bx + 35, by + 35, pal::text, 12, 2, Align::Right);
        }
        g_.setAlpha(1);
    };
    const double half = std::floor((w - 8) / 2);
    std::string head = std::to_string(q.xp) + " experience";
    if (q.gold) head += "   ·   " + std::to_string(q.gold) + " gold";
    if (q.gold) drawSpr(spr_.icon("gold"), x, y - 15, 18);
    text(head, x + (q.gold ? 24 : 0), y, pal::gold, 14, 1);
    y += 14;
    if (!q.items.empty()) {
        text("You will receive:", x, y + 14, pal::muted, 12, 1);
        y += 22;
        for (size_t i = 0; i < q.items.size(); i++) itemBox(q.items[i].first, q.items[i].second, x + (i % 2) * (half + 8), y + (i / 2) * 46.0, half, false, false);
        y += ((q.items.size() + 1) / 2) * 46.0;
    }
    if (!q.choice.empty()) {
        text(choosing ? "Choose one of these rewards:" : "You will be able to choose one of these:", x, y + 14, choosing ? QUEST_GOLD : pal::muted, 12, 1);
        y += 22;
        for (size_t i = 0; i < q.choice.size(); i++) {
            const double bx = x + (i % 2) * (half + 8), by = y + (i / 2) * 46.0;
            itemBox(q.choice[i].first, q.choice[i].second, bx, by, half, choosing && static_cast<int>(i) == pick, choosing && pick >= 0 && static_cast<int>(i) != pick);
            if (choosing) {
                const int k = static_cast<int>(i);
                hit(bx, by, half, 40, [this, k] { rewardPick_ = k; });
            }
        }
        y += ((q.choice.size() + 1) / 2) * 46.0;
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
    dim(0.45);
    // A plain chat is only as tall as what is said and the choices; a quest gets the full window
    double want = 560;
    if (talkView_ == TalkView::Menu) want = 190 + wrap(sim.talkLine, std::min(780.0, uiW() - 40) - 48, 16).size() * 22.0 + talkOptions().size() * 40.0 + 30;
    const double W = std::min(780.0, uiW() - 40), H = std::min(want, uiH() - 40), X = std::round((uiW() - W) / 2), Y = std::round((uiH() - H) / 2);
    panel(X, Y, W, H);
    closeButton(X + W - 40, Y + 14);

    // The speaker
    box(X + 22, Y + 22, 84, 84, pal::well, pal::line);
    {
        const CharSet& cs = spr_.character(npc->sprite, npc->tint);
        const Frames& idle = cs.get("idle");
        if (!idle.empty()) drawSpr(idle[(sim.tickCount / 14) % idle.size()], X + 22, Y + 18, 84);
    }
    text(npc->name, X + 122, Y + 44, pal::text, 22, 1);
    std::string where;
    if (const BuildingDef* b = db_.building(npc->building)) where = b->name;
    if (const TownDef* t = db_.town(npc->town)) where += (where.empty() ? "" : ", ") + t->name;
    text(where, X + 122, Y + 64, pal::muted, 12, 1);

    const double cx = X + 24, cw = W - 48;
    double y = Y + 132;
    const QuestDef* q = db_.quest(talkQuest_);
    if (talkView_ != TalkView::Menu && !q) talkView_ = TalkView::Menu;

    if (talkView_ == TalkView::Menu) {
        for (const auto& l : wrap("\"" + sim.talkLine + "\"", cw, 16)) {
            text(l, cx, y, pal::text, 16);
            y += 22;
        }
        y += 16;
        const auto opts = talkOptions();
        talkSel_ = std::clamp(talkSel_, 0, static_cast<int>(opts.size()) - 1);
        for (size_t i = 0; i < opts.size(); i++) {
            const bool sel = static_cast<int>(i) == talkSel_;
            row(cx, y, cw, 34, sel);
            drawSpr(spr_.get(opts[i].icon), cx + 6, y + 3, 28);
            text(opts[i].label, cx + 44, y + 17, opts[i].col, 15, 1, Align::Left, Baseline::Middle);
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
        text("Up / Down choose · Enter select · Esc leave", X + W / 2, Y + H - 16, pal::muted, 12, 0, Align::Center);
        return;
    }

    // A quest: offered, under way, or ready to hand in
    text(q->title, cx, y, pal::gold, 21, 1);
    text("Level " + std::to_string(q->level) + (q->main ? " · Main story" : " · Side quest"), cx + cw, y, pal::muted, 12, 1, Align::Right);
    y += 26;
    const std::string body = talkView_ == TalkView::Offer ? q->offer : talkView_ == TalkView::Progress ? (q->progress.empty() ? q->offer : q->progress) : (q->done.empty() ? "Well done." : q->done);
    for (const auto& l : wrap(body, cw, 15)) {
        text(l, cx, y, pal::text, 15);
        y += 20;
    }
    y += 12;
    if (talkView_ != TalkView::Complete) {
        text(talkView_ == TalkView::Offer ? "Quest objectives" : "Progress", cx, y, QUEST_GOLD, 15, 1);
        y += 22;
        y = drawObjectives(*q, cx, y, cw, talkView_ == TalkView::Progress) + 6;
    }
    text("Rewards", cx, y, QUEST_GOLD, 15, 1);
    y += 22;
    drawRewards(*q, cx, y, cw, rewardPick_, talkView_ == TalkView::Complete);

    // Buttons
    const double by = Y + H - 62;
    auto btn = [&](double bx, double bw, const std::string& label, bool primary, std::function<void()> act) {
        button(bx, by, bw, 32, label, primary, 15);
        hit(bx, by, bw, 32, std::move(act));
    };
    if (talkView_ == TalkView::Offer) {
        btn(cx, 140, "Accept", true, [this] { talkKey("Enter", "", true); });
        btn(cx + cw - 140, 140, "Decline", false, [this] { talkKey("Escape", "", false); });
        text("Enter accept · Esc decline", X + W / 2, Y + H - 16, pal::muted, 12, 0, Align::Center);
    } else if (talkView_ == TalkView::Progress) {
        btn(cx, 140, "Back", false, [this] { talkKey("Escape", "", false); });
        if (q->turnIn != sim.talkNpc)
            if (const NpcDef* t = db_.npc(q->turnIn)) text("Hand it in to " + t->name + ".", cx + 160, by + 20, pal::muted, 13, 1);
    } else {
        btn(cx, 170, "Complete quest", true, [this] { talkKey("Enter", "", true); });
        btn(cx + cw - 140, 140, "Back", false, [this] { talkKey("Escape", "", false); });
        text(q->choice.empty() ? "Enter complete · Esc back" : "Left / Right choose a reward · Enter complete · Esc back", X + W / 2, Y + H - 16, pal::muted, 12, 0, Align::Center);
    }
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
        text("[" + std::to_string(q.level) + "]", X + 40, y + 18, q.level > sim.P.level + 2 ? pal::bad : q.level + 3 < sim.P.level ? pal::faint : pal::muted, 12, 1, Align::Left, Baseline::Middle);
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

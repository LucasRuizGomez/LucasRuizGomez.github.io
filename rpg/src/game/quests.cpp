// Quest log: offering, tracking, handing in, and what the people of the realm say about it.
#include "game/sim.h"

namespace hc {

bool Sim::questAvailable(const QuestDef& q) const {
    if (auto it = quests.find(q.id); it != quests.end() && it->second.state != 0) return false;
    for (const auto& r : q.requires_) {
        auto it = quests.find(r);
        if (it == quests.end() || it->second.state != 2) return false;
    }
    return true;
}

int Sim::objectiveProgress(const QuestDef& q, size_t i) const {
    const QuestObjective& o = q.objectives[i];
    if (o.type == "have" || o.type == "collect") {
        if (o.anyWeapon)
            for (const auto& it : P.inv)
                if (db.item(it.id).type == ItemType::Weapon) return o.count;
        return std::min(o.count, countItem(o.item));
    }
    auto it = quests.find(q.id);
    if (it == quests.end() || i >= it->second.counts.size()) return 0;
    return std::min(o.count, it->second.counts[i]);
}

bool Sim::questReady(const QuestDef& q) const {
    auto it = quests.find(q.id);
    if (it == quests.end() || it->second.state != 1) return false;
    for (size_t i = 0; i < q.objectives.size(); i++)
        if (objectiveProgress(q, i) < q.objectives[i].count) return false;
    return true;
}

int Sim::npcMarker(const std::string& npcId) const {
    int best = 0;
    for (const auto& q : db.quests) {
        auto it = quests.find(q.id);
        const int state = it == quests.end() ? 0 : it->second.state;
        if (state == 1 && q.turnIn == npcId) best = std::max(best, questReady(q) ? 3 : 2);
        else if (state == 0 && q.giver == npcId && questAvailable(q)) best = std::max(best, 1);
    }
    return best == 2 ? 2 : best;
}

std::vector<const QuestDef*> Sim::activeQuests() const {
    std::vector<const QuestDef*> out;
    for (const auto& q : db.quests)
        if (auto it = quests.find(q.id); it != quests.end() && it->second.state == 1) out.push_back(&q);
    return out;
}

void Sim::questEvent(const std::string& type, const std::string& target, const std::string& alt) {
    for (const auto& q : db.quests) {
        auto it = quests.find(q.id);
        if (it == quests.end() || it->second.state != 1) continue;
        const bool wasReady = questReady(q);
        for (size_t i = 0; i < q.objectives.size(); i++) {
            const QuestObjective& o = q.objectives[i];
            if (o.type != type) continue;
            if (!o.target.empty() && o.target != target && o.target != alt) continue;
            it->second.counts.resize(q.objectives.size(), 0);
            if (it->second.counts[i] >= o.count) continue;
            it->second.counts[i]++;
            toast(o.label + ": " + std::to_string(it->second.counts[i]) + " / " + std::to_string(o.count), Color(242, 210, 122));
        }
        if (!wasReady && questReady(q)) {
            const NpcDef* n = db.npc(q.turnIn);
            toast(q.title + " complete. Return to " + (n ? n->name : std::string("the quest giver")) + ".", Color(123, 224, 138));
            play("quest");
        }
    }
}

void Sim::completeQuest(const QuestDef& q, int choice) {
    for (const auto& o : q.objectives)
        if (o.type == "collect") removeItem(o.item, o.count);
    quests[q.id].state = 2;
    std::string got;
    if (q.gold) {
        addItem({"gold", q.gold});
        got += "+" + std::to_string(q.gold) + " gold";
    }
    auto give = [&](const std::string& id, int qty) {
        const ItemDef& def = db.item(id);
        Item it = def.isGear() ? makeGear(id, q.rewardRarity, 1.5) : Item{id, qty};
        got += (got.empty() ? "+" : ", +") + (qty > 1 ? std::to_string(qty) + " " : std::string()) + itemName(it);
        if (!addItem(it)) dropItem(map(), it, P.x, P.y + 6);
    };
    for (const auto& [id, qty] : q.items) give(id, qty);
    if (!q.choice.empty()) {
        const auto& [id, qty] = q.choice[std::clamp(choice, 0, static_cast<int>(q.choice.size()) - 1)];
        give(id, qty);
    }
    play("questDone");
    toast("Quest complete: " + q.title + (got.empty() ? "" : " · " + got), Color(123, 224, 138));
    gainXp(q.xp);
    wantSave = true;
}

std::vector<const QuestDef*> Sim::questsOffered(const std::string& npcId) const {
    std::vector<const QuestDef*> out;
    for (const auto& q : db.quests)
        if (q.giver == npcId && questAvailable(q)) out.push_back(&q);
    return out;
}

std::vector<const QuestDef*> Sim::questsToHandIn(const std::string& npcId) const {
    std::vector<const QuestDef*> out;
    for (const auto& q : db.quests)
        if (q.turnIn == npcId && questReady(q)) out.push_back(&q);
    return out;
}

std::vector<const QuestDef*> Sim::questsUnderway(const std::string& npcId) const {
    std::vector<const QuestDef*> out;
    for (const auto& q : db.quests) {
        auto it = quests.find(q.id);
        if (it != quests.end() && it->second.state == 1 && !questReady(q) && (q.turnIn == npcId || q.giver == npcId)) out.push_back(&q);
    }
    return out;
}

bool Sim::acceptQuest(const std::string& id) {
    const QuestDef* q = db.quest(id);
    if (!q || !questAvailable(*q)) return false;
    QuestState st;
    st.state = 1;
    st.counts.assign(q->objectives.size(), 0);
    quests[q->id] = st;
    toast("Quest accepted: " + q->title, Color(242, 210, 122));
    play("quest");
    wantSave = true;
    // Already where the quest sends you, or talking to the person it wants you to meet? Count it now.
    if (!townId.empty()) questEvent("visit", townId);
    if (!talkNpc.empty()) questEvent("talk", talkNpc);
    return true;
}

bool Sim::turnInQuest(const std::string& id, int choice) {
    const QuestDef* q = db.quest(id);
    if (!q || !questReady(*q)) return false;
    completeQuest(*q, choice);
    return true;
}

bool Sim::abandonQuest(const std::string& id) {
    const QuestDef* q = db.quest(id);
    auto it = quests.find(id);
    if (!q || it == quests.end() || it->second.state != 1) return false;
    quests.erase(it);
    toast("Quest abandoned: " + q->title + ". Its giver will offer it again.", Color(200, 190, 170));
    play("deny");
    wantSave = true;
    return true;
}

void Sim::openShop(const std::string& npcId) {
    const NpcDef* def = db.npc(npcId);
    if (!def || def->shop.empty()) return;
    shopId = def->shop;
    signTitle = def->name;
    mode = Mode::Shop;
    play("uiOpen");
}

std::string Sim::questGiverTown(const QuestDef& q) const {
    if (const NpcDef* n = db.npc(q.giver))
        if (const TownDef* t = db.town(n->town)) return t->name;
    return "";
}

// Speaking to someone opens the conversation window: a line of their own, then what they have for you
void Sim::talkTo(Object& npc) {
    const NpcDef* def = db.npc(npc.id);
    if (!def) return;
    npc.face = P.x < npc.c * TILE + 8 ? -1 : 1;
    talkNpc = npc.id;
    questEvent("talk", npc.id);
    signTitle = def->name;
    const int n = chats_[npc.id]++;
    talkLine = def->lines.empty() ? (def->text.empty() ? "..." : def->text) : def->lines[static_cast<size_t>(n) % def->lines.size()];
    mode = Mode::Talk;
    play("uiOpen");
}

}   // namespace hc

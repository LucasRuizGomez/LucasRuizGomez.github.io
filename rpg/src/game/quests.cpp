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

void Sim::completeQuest(const QuestDef& q) {
    for (const auto& o : q.objectives)
        if (o.type == "collect") removeItem(o.item, o.count);
    quests[q.id].state = 2;
    std::string got;
    if (q.gold) {
        addItem({"gold", q.gold});
        got += "+" + std::to_string(q.gold) + " gold";
    }
    for (const auto& [id, qty] : q.items) {
        const ItemDef& def = db.item(id);
        Item it = def.isGear() ? makeGear(id, q.rewardRarity, 1.5) : Item{id, qty};
        got += (got.empty() ? "+" : ", +") + (qty > 1 ? std::to_string(qty) + " " : std::string()) + itemName(it);
        if (!addItem(it)) dropItem(map(), it, P.x, P.y + 6);
    }
    play("questDone");
    toast("Quest complete: " + q.title + (got.empty() ? "" : " · " + got), Color(123, 224, 138));
    gainXp(q.xp);
    wantSave = true;
}

void Sim::talkTo(Object& npc) {
    const NpcDef* def = db.npc(npc.id);
    if (!def) return;
    npc.face = P.x < npc.c * TILE + 8 ? -1 : 1;
    questEvent("talk", npc.id);
    signTitle = def->name;
    // 1. a quest to hand in here
    for (const auto& q : db.quests)
        if (q.turnIn == npc.id && questReady(q)) {
            completeQuest(q);
            signText = q.done;
            mode = Mode::Sign;
            return;
        }
    // 2. a new quest on offer
    for (const auto& q : db.quests)
        if (q.giver == npc.id && questAvailable(q)) {
            QuestState st;
            st.state = 1;
            st.counts.assign(q.objectives.size(), 0);
            quests[q.id] = st;
            toast("New quest: " + q.title, Color(242, 210, 122));
            play("quest");
            signText = q.offer;
            mode = Mode::Sign;
            wantSave = true;
            // Already somewhere the quest sends you? Count it straight away.
            if (!townId.empty()) questEvent("visit", townId);
            return;
        }
    // 3. shops open directly once there is nothing new to say
    if (!def->shop.empty()) {
        shopId = def->shop;
        mode = Mode::Shop;
        play("uiOpen");
        return;
    }
    // 4. a quest in progress
    for (const auto& q : db.quests) {
        auto it = quests.find(q.id);
        if (it == quests.end() || it->second.state != 1 || (q.turnIn != npc.id && q.giver != npc.id)) continue;
        signText = q.turnIn == npc.id || q.progress.empty() ? q.progress : q.progress;
        for (size_t i = 0; i < q.objectives.size(); i++)
            signText += " (" + q.objectives[i].label + ": " + std::to_string(objectiveProgress(q, i)) + " / " + std::to_string(q.objectives[i].count) + ")";
        mode = Mode::Sign;
        return;
    }
    signText = def->text.empty() ? "..." : def->text;
    mode = Mode::Sign;
}

}   // namespace hc

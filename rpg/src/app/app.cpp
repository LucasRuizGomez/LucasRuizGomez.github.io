#include "app/app.h"

#include "app/palette.h"
#include "engine/platform.h"

#include <SDL.h>
#include <cmath>
#include <ctime>

namespace hc {

namespace {

const char* SETTINGS_KEY = "hollowcrypt3_settings";
std::string slotKey(int i) { return "hollowcrypt3_slot" + std::to_string(i); }

const std::map<std::string, Vec2> MOVE_KEYS = {{"ArrowUp", {0, -1}}, {"KeyW", {0, -1}}, {"ArrowDown", {0, 1}}, {"KeyS", {0, 1}},
                                               {"ArrowLeft", {-1, 0}}, {"KeyA", {-1, 0}}, {"ArrowRight", {1, 0}}, {"KeyD", {1, 0}}};
bool isEnter(const std::string& c) { return c == "Enter" || c == "NumpadEnter"; }
std::string dirOf(const std::string& code) {
    if (code == "ArrowUp" || code == "KeyW") return "up";
    if (code == "ArrowDown" || code == "KeyS") return "down";
    if (code == "ArrowLeft" || code == "KeyA") return "left";
    if (code == "ArrowRight" || code == "KeyD") return "right";
    return "";
}

}   // namespace

// ───────────────────────────── start-up ─────────────────────────────

bool App::init(int argc, char** argv) {
    platform::setArgs(argc, argv);
    root_ = platform::dataRoot();
    try {
        db_ = Database::load(root_ + "config/");
    } catch (const std::exception& e) {
        logWarn("Cannot load game data: %s", e.what());
        return false;
    }
    const bool headless = !platform::queryParam("shot").empty() && platform::canQuit();
    int w = 1280, h = 720;
    if (const std::string size = platform::queryParam("size"); !size.empty()) std::sscanf(size.c_str(), "%dx%d", &w, &h);
    if (!g_.init(db_.tuning.saveSlots ? "Hollow Crypt" : "Hollow Crypt", w, h, headless)) return false;
    if (!headless) audio_.init();
    audio_.load(db_.audio, root_);
    fonts_.load(0, root_ + "assets/fonts/AlegreyaSans-Medium.ttf");
    fonts_.load(1, root_ + "assets/fonts/AlegreyaSans-Bold.ttf");
    fonts_.load(2, root_ + "assets/fonts/AlegreyaSans-ExtraBold.ttf");
    loadSettings();
    spr_.build(g_, db_, root_);
    world_ = std::make_unique<WorldView>(g_, spr_, db_);
    sim_ = std::make_unique<Sim>(db_, static_cast<uint32_t>(std::time(nullptr)));
    sim_->prepareTitle();
    readSlots();
    resize();
    applySettings();
    lastCounter_ = SDL_GetPerformanceCounter();
    runDebugSwitches();
    return true;
}

void App::shutdown() {
    world_.reset();
    audio_.shutdown();
    g_.shutdown();
}

// Development switches: URL query on the web (?autostart&tp=52,62) or --name=value on desktop.
void App::runDebugSwitches() {
    auto param = [](const char* n) { return platform::queryParam(n); };
    if (!param("mute").empty()) {
        set_.muted = true;
        applySettings();
    }
    if (param("autostart").empty() && param("tp").empty() && param("screen").empty()) return;
    const std::string screen = param("screen");
    if (screen == "load") {
        titleView_ = TitleView::Load;
        return;
    }
    if (screen == "new") {
        titleView_ = TitleView::NewGame;
        return;
    }
    if (screen == "settings") {
        titleView_ = TitleView::Settings;
        optSel_ = 1;
        return;
    }
    if (screen == "title") return;
    slot_ = -1;   // debug runs never touch the real saves
    platform::showPageLink(false);
    sim_->newGame("Tester");
    world_->forget();
    if (!param("kit").empty()) {
        for (const char* id : {"iron_sword", "wood_shield", "bow", "torch", "pickaxe", "leather_armor", "mount_boar"}) sim_->addItem({id, 1});
        sim_->addItem({"arrows", 40});
        sim_->addItem({"gold", 200});
        sim_->addItem({"potion", 3});
        sim_->addItem(sim_->makeGear("knight_blade", 3));
        sim_->gainXp(400);
    }
    if (const std::string tp = param("tp"); !tp.empty()) {
        int c = 0, r = 0;
        if (std::sscanf(tp.c_str(), "%d,%d", &c, &r) == 2) sim_->enterMap(sim_->mapId, c * TILE + 8, r * TILE + 10);
    }
    if (const std::string map = param("map"); !map.empty() && sim_->maps.count(map)) {
        const Map& m = sim_->maps.at(map);
        sim_->enterMap(map, m.start.c * TILE + 8, m.start.r * TILE + 10);
    }
    if (const std::string hour = param("hour"); !hour.empty()) sim_->time = static_cast<int>(db_.tuning.dayLength * (std::atof(hour.c_str()) - 6) / 24.0 + db_.tuning.dayLength);
    for (int i = 0; i < 40; i++) sim_->tick(SimInput());
    if (!param("reveal").empty()) {
        Map& over = sim_->overworld();
        std::fill(over.explored.begin(), over.explored.end(), static_cast<uint8_t>(1));
        over.exploredDirty = true;
        for (const auto& z : db_.zones) sim_->zonesFound.insert(z.id);
        for (const auto& t : db_.towns) sim_->waypoints.insert(t.id);
    }
    if (screen == "inv") sim_->mode = Mode::Inv;
    else if (screen == "craft") sim_->mode = Mode::Craft;
    else if (screen == "quests") sim_->mode = Mode::Quests;
    else if (screen == "map") sim_->mode = Mode::WorldMap;
    else if (screen == "pause") openPause();
    else if (screen == "controls") {
        openPause();
        pauseView_ = PauseView::Controls;
    }
}

// ───────────────────────────── settings and saves ─────────────────────────────

void App::loadSettings() {
    set_ = Settings();
    set_.sfx = db_.defaultSfx;
    set_.music = db_.defaultMusic;
    set_.ui = db_.defaultUi;
    for (const auto& a : db_.actions) set_.binds[a.id] = a.keys;
    const std::string raw = platform::storageGet(SETTINGS_KEY);
    if (raw.empty()) return;
    const json j = json::parse(raw, nullptr, false);
    if (!j.is_object()) return;
    set_.sfx = std::clamp(j.value("sfx", set_.sfx), 0.0, 1.0);
    set_.music = std::clamp(j.value("music", set_.music), 0.0, 1.0);
    set_.ui = std::clamp(j.value("ui", set_.ui), 0.5, 2.0);
    set_.muted = j.value("muted", false);
    if (j.contains("binds") && j.at("binds").is_object())
        for (const auto& a : db_.actions)
            if (j.at("binds").contains(a.id) && j.at("binds").at(a.id).is_array() && !j.at("binds").at(a.id).empty()) {
                std::vector<std::string> keys;
                for (const auto& k : j.at("binds").at(a.id))
                    if (k.is_string() && keys.size() < 2) keys.push_back(k.get<std::string>());
                if (!keys.empty()) set_.binds[a.id] = keys;
            }
}

void App::saveSettings() {
    json j = {{"sfx", set_.sfx}, {"music", set_.music}, {"ui", set_.ui}, {"muted", set_.muted}, {"binds", set_.binds}};
    platform::storageSet(SETTINGS_KEY, j.dump());
}

void App::applySettings() { audio_.setVolumes(set_.sfx, set_.music, set_.muted); }

std::string App::actionFor(const std::string& code) const {
    for (const auto& a : db_.actions) {
        auto it = set_.binds.find(a.id);
        if (it != set_.binds.end() && std::find(it->second.begin(), it->second.end(), code) != it->second.end()) return a.id;
    }
    return "";
}

bool App::held(const std::string& action) const {
    auto it = set_.binds.find(action);
    if (it != set_.binds.end())
        for (const auto& k : it->second)
            if (in_.down(k)) return true;
    for (const auto& [button, act] : db_.gamepad)
        if (act == action && in_.padHeld.count(button)) return true;
    return false;
}

std::string App::bindLabel(const std::string& action) const {
    auto it = set_.binds.find(action);
    return it == set_.binds.end() || it->second.empty() ? "-" : keyLabel(it->second.front());
}

// Button name for an action on the device currently in use
std::string App::btnLabel(const std::string& action) const {
    if (scheme_ == Scheme::Pad) {
        static const std::map<std::string, std::string> PAD = {{"attack", "X"}, {"shoot", "RT"}, {"skill", "LT"}, {"heal", "Y"}, {"interact", "A"},
                                                              {"dodge", "B"},  {"bag", "Back"}, {"craft", "LB"}, {"mount", "L3"}, {"map", "R3"}};
        auto it = PAD.find(action);
        return it == PAD.end() ? "" : it->second;
    }
    if (scheme_ == Scheme::Mouse && action == "attack") return "LMB";
    if (scheme_ == Scheme::Mouse && action == "shoot") return "RMB";
    return bindLabel(action);
}

// Movement, Enter and Escape stay fixed so the game can never become unplayable
void App::rebindTo(const std::string& code) {
    const std::string a = rebind_;
    rebind_.clear();
    if (code == "Escape") return;
    if (MOVE_KEYS.count(code) || isEnter(code) || code == "KeyP") {
        notice("That key is reserved for moving or menus.");
        audio_.play("deny");
        return;
    }
    for (const auto& other : db_.actions)
        if (other.id != a && set_.binds[other.id].size() == 1 && set_.binds[other.id][0] == code) {
            notice(keyLabel(code) + " is the only key for " + other.label + ".");
            audio_.play("deny");
            return;
        }
    for (const auto& other : db_.actions)
        if (other.id != a) std::erase(set_.binds[other.id], code);
    auto& keys = set_.binds[a];
    std::erase(keys, code);
    keys.insert(keys.begin(), code);
    if (keys.size() > 2) keys.resize(2);
    saveSettings();
    audio_.play("pick");
}

void App::readSlots() {
    slots_.assign(static_cast<size_t>(std::max(1, db_.tuning.saveSlots)), SaveSlot());
    for (size_t i = 0; i < slots_.size(); i++) {
        const std::string raw = platform::storageGet(slotKey(static_cast<int>(i)));
        if (raw.empty()) continue;
        const json j = json::parse(raw, nullptr, false);
        if (!j.is_object() || !j.contains("summary")) continue;
        const json& s = j.at("summary");
        SaveSlot& slot = slots_[i];
        slot.used = true;
        slot.name = s.value("name", "Wanderer");
        slot.place = s.value("place", "");
        slot.level = s.value("level", 1);
        slot.time = s.value("time", 0);
        slot.day = s.value("day", 1);
        slot.deaths = s.value("deaths", 0);
        slot.victory = s.value("victory", false);
        slot.savedAt = j.value("savedAt", "");
    }
}

void App::writeSave() {
    sim_->wantSave = false;
    if (slot_ < 0 || sim_->mode == Mode::Title) return;
    json j = sim_->saveGame();
    j["savedAt"] = platform::timestamp();
    if (!platform::storageSet(slotKey(slot_), j.dump())) logWarn("Could not write the save");
}

bool App::loadSlot(int i) {
    if (i < 0 || i >= static_cast<int>(slots_.size()) || !slots_[i].used) return false;
    const json j = json::parse(platform::storageGet(slotKey(i)), nullptr, false);
    audio_.play("open");
    sim_->transition([this, i, j] {
        world_->forget();
        if (sim_->loadGame(j)) {
            slot_ = i;
            platform::showPageLink(false);
        } else {
            sim_->prepareTitle();
            titleView_ = TitleView::Main;
            notice("That save could not be read (it is from an older version).");
        }
    });
    return true;
}

void App::deleteSlot(int i) {
    platform::storageRemove(slotKey(i));
    readSlots();
}

int App::freeSlot() const {
    for (size_t i = 0; i < slots_.size(); i++)
        if (!slots_[i].used) return static_cast<int>(i);
    return -1;
}

int App::newestSlot() const {
    int best = -1;
    for (size_t i = 0; i < slots_.size(); i++)
        if (slots_[i].used && (best < 0 || slots_[i].savedAt > slots_[best].savedAt)) best = static_cast<int>(i);
    return best;
}

void App::startNewGame() {
    audio_.play("open");
    const int slot = newSlot_;
    const std::string name = heroName_;
    sim_->transition([this, slot, name] {
        world_->forget();
        slot_ = slot;
        sim_->newGame(name);
        writeSave();
        readSlots();
        platform::showPageLink(false);
    });
}

void App::openPause() {
    sim_->mode = Mode::Pause;
    pauseSel_ = 0;
    pauseView_ = PauseView::Main;
    audio_.play("uiOpen");
}

void App::closePause() {
    sim_->mode = Mode::Play;
    saveSettings();
    platform::showPageLink(false);
}

void App::quitToTitle() {
    writeSave();
    readSlots();
    sim_->transition([this] {
        sim_->prepareTitle();
        world_->forget();
        titleView_ = TitleView::Main;
        titleSel_ = 0;
        slot_ = -1;
        platform::showPageLink(true);
    });
}

void App::setMode(Mode m) {
    if (sim_->mode != m && m != Mode::Play) audio_.play("uiOpen");
    sim_->mode = m;
}

// ───────────────────────────── input ─────────────────────────────

void App::playAction(const std::string& a, Scheme from) {
    if (a == "attack" || a == "shoot") scheme_ = from;
    if (a == "bag") {
        setMode(Mode::Inv);
        invSel_ = std::min(invSel_, std::max(0, static_cast<int>(sim_->P.inv.size()) - 1));
    } else if (a == "craft") setMode(Mode::Craft);
    else if (a == "quests") setMode(Mode::Quests);
    else if (a == "map") setMode(Mode::WorldMap);
    else sim_->action(a, from == Scheme::Mouse);
}

void App::handleKey(const std::string& code, bool shift) {
    Sim& sim = *sim_;
    if (sim.fade) return;
    const bool enter = isEnter(code);
    const std::string dir = dirOf(code), act = actionFor(code);
    switch (sim.mode) {
        case Mode::Title: titleKey(code, shift); break;
        case Mode::Play:
            if (code == "Escape" || code == "KeyP") openPause();
            else if (!act.empty()) playAction(act, Scheme::Keys);
            break;
        case Mode::Pause: pauseKey(code); break;
        case Mode::Inv: {
            const int cols = 6, n = db_.tuning.invSlots;
            if (dir == "right") invSel_ = std::min(n - 1, invSel_ + 1);
            if (dir == "left") invSel_ = std::max(0, invSel_ - 1);
            if (dir == "down") invSel_ = std::min(n - 1, invSel_ + cols);
            if (dir == "up") invSel_ = std::max(0, invSel_ - cols);
            if (enter || code == "Space" || act == "interact") sim.useOrEquip(invSel_);
            if (act == "bag" || code == "Escape") sim.mode = Mode::Play;
            if (act == "craft") setMode(Mode::Craft);
            if (act == "quests") setMode(Mode::Quests);
            break;
        }
        case Mode::Craft: {
            const int n = static_cast<int>(sim.recipes(craftCat_).size());
            if (dir == "down") craftSel_ = std::min(n - 1, craftSel_ + 1);
            if (dir == "up") craftSel_ = std::max(0, craftSel_ - 1);
            if (code == "Tab" || dir == "right" || dir == "left") setCraftCat(dir == "left" ? -1 : 1);
            else if (act == "bag") setMode(Mode::Inv);
            if (enter) craftSelected(shift ? 5 : 1);
            if (act == "craft" || code == "Escape") sim.mode = Mode::Play;
            break;
        }
        case Mode::Shop: {
            const int n = static_cast<int>(sim.shopRows(shopBuying_).size());
            if (dir == "down") shopSel_ = std::min(std::max(0, n - 1), shopSel_ + 1);
            if (dir == "up") shopSel_ = std::max(0, shopSel_ - 1);
            if (dir == "left" || dir == "right" || code == "Tab") {
                shopBuying_ = !shopBuying_;
                shopSel_ = 0;
            }
            if (enter) sim.shopTrade(shopBuying_, shopSel_, shift ? 5 : 1);
            if (code == "Escape" || act == "interact") sim.mode = Mode::Play;
            break;
        }
        case Mode::Sign:
            if (enter || act == "interact" || code == "Escape" || code == "Space") sim.mode = Mode::Play;
            break;
        case Mode::Quests: {
            const int n = static_cast<int>(sim.activeQuests().size());
            if (dir == "down") questSel_ = std::min(std::max(0, n - 1), questSel_ + 1);
            if (dir == "up") questSel_ = std::max(0, questSel_ - 1);
            if (act == "map") setMode(Mode::WorldMap);
            else if (act == "quests" || code == "Escape" || enter) sim.mode = Mode::Play;
            break;
        }
        case Mode::WorldMap:
            if (act == "quests") setMode(Mode::Quests);
            else if (act == "map" || code == "Escape" || enter || code == "Space") sim.mode = Mode::Play;
            break;
        case Mode::Travel: {
            const auto list = travelList();
            const int n = static_cast<int>(list.size());
            if (dir == "down") travelSel_ = std::min(std::max(0, n - 1), travelSel_ + 1);
            if (dir == "up") travelSel_ = std::max(0, travelSel_ - 1);
            if (enter && travelSel_ < n) {
                if (list[travelSel_]->id == sim.townId) sim.mode = Mode::Play;
                else sim.travelTo(list[travelSel_]->id);
            }
            if (code == "Escape" || act == "interact") sim.mode = Mode::Play;
            break;
        }
        case Mode::Dead:
            if (enter || code == "Space") sim.transition([this] { sim_->respawn(); });
            break;
        case Mode::Victory:
            if (enter || code == "Space") {
                sim.victorySeen = true;
                sim.mode = Mode::Play;
                sim.wantSave = true;
            }
            break;
    }
}

// Gamepad (standard mapping). Play: buttons map to actions (controls.json). Menus: d-pad / stick, A confirm, B back.
void App::handlePad() {
    Sim& sim = *sim_;
    if (!in_.padConnected) return;
    if (in_.padActive()) scheme_ = Scheme::Pad;
    if (sim.fade) return;
    auto pressed = [&](const char* b) { return std::find(in_.padPresses.begin(), in_.padPresses.end(), b) != in_.padPresses.end(); };
    if (sim.mode == Mode::Play) {
        for (const auto& b : in_.padPresses) {
            auto it = db_.gamepad.find(b);
            if (it != db_.gamepad.end()) playAction(it->second, Scheme::Pad);
        }
        if (pressed("start")) openPause();
        return;
    }
    const std::string d = in_.padLY < -0.5 ? "ArrowUp" : in_.padLY > 0.5 ? "ArrowDown" : in_.padLX < -0.5 ? "ArrowLeft" : in_.padLX > 0.5 ? "ArrowRight" : "";
    if (!d.empty() && (d != padDir_ || --padRepeat_ <= 0)) {
        padRepeat_ = d != padDir_ ? 18 : 6;
        handleKey(d, false);
    }
    padDir_ = d;
    if (pressed("a")) handleKey("Enter", false);
    if (pressed("b")) handleKey("Escape", false);
    if (pressed("start")) handleKey(sim.mode == Mode::Title ? "Enter" : "Escape", false);
    if (pressed("x") && sim.mode == Mode::Craft) handleKey("Enter", true);
    if (pressed("x") && sim.mode == Mode::Title) handleKey("Delete", false);
    if (pressed("leftshoulder")) handleKey("ArrowLeft", false);
    if (pressed("rightshoulder")) handleKey("ArrowRight", false);
}

void App::handleClick(const MouseClick& c) {
    Sim& sim = *sim_;
    if (sim.fade) return;
    if (sim.mode == Mode::Play) {
        scheme_ = Scheme::Mouse;
        playAction(c.button == 0 ? "attack" : "shoot", Scheme::Mouse);
        return;
    }
    if (c.button != 0) {   // right click = back in menus
        handleKey("Escape", false);
        return;
    }
    if (!rebind_.empty()) return;
    if (const Hit* h = hitAt(c.x / ui_, c.y / ui_); h && h->click) {
        const auto click = h->click;   // copy: the handler may rebuild the hit list
        click();
        return;
    }
    // A click anywhere also confirms the simple screens
    if (sim.mode == Mode::Sign || sim.mode == Mode::Dead || sim.mode == Mode::Victory) handleKey("Enter", false);
}

void App::handleInput() {
    in_.poll();
    if (in_.quit) running = false;
    Sim& sim = *sim_;
    for (const auto& k : in_.presses) {
        if (!rebind_.empty()) {
            if (!k.repeat) rebindTo(k.code);
            continue;
        }
        const bool typing = sim.mode == Mode::Title && titleView_ == TitleView::NewGame && k.code == "Backspace";
        if (k.repeat && !MOVE_KEYS.count(k.code) && !typing) continue;
        if (k.repeat && sim.mode == Mode::Play) continue;
        if (k.code == "F11" && platform::canQuit()) {
            set_.fullscreen = !set_.fullscreen;
            g_.setFullscreen(set_.fullscreen);
            continue;
        }
        handleKey(k.code, k.shift);
    }
    handlePad();
    if (in_.mouseMoved) {
        // Only a real hand on the mouse switches to mouse controls (not a resting cursor)
        if (sim.mode == Mode::Play) scheme_ = Scheme::Mouse;
        else if (rebind_.empty())
            if (const Hit* h = hitAt(in_.mouseX / ui_, in_.mouseY / ui_); h && h->hover) h->hover();
    }
    for (const auto& c : in_.clicks) handleClick(c);
    if (in_.wheel != 0 && (sim.mode == Mode::Craft || sim.mode == Mode::Shop || sim.mode == Mode::Quests || sim.mode == Mode::Travel || sim.mode == Mode::Pause || sim.mode == Mode::Title))
        handleKey(in_.wheel < 0 ? "ArrowDown" : "ArrowUp", false);
    if (in_.lostFocus && sim.mode == Mode::Play && !sim.fade && platform::queryParam("shot").empty() && platform::queryParam("autostart").empty()) openPause();
}

SimInput App::buildSimInput() {
    SimInput si;
    if (sim_->mode != Mode::Play) return si;
    // Keyboard gives 8 directions; a stick gives any angle and walks slower when barely tilted
    double kx = 0, ky = 0;
    for (const auto& [code, v] : MOVE_KEYS)
        if (in_.down(code)) {
            kx += v.x;
            ky += v.y;
        }
    si.moveX = sign(kx);
    si.moveY = sign(ky);
    if (si.moveX == 0 && si.moveY == 0) {
        si.moveX = in_.padLX;
        si.moveY = in_.padLY;
    }
    const bool mouseMode = scheme_ == Scheme::Mouse;
    si.holdAttack = (mouseMode && in_.mouseLeft) || held("attack");
    si.holdShoot = (mouseMode && in_.mouseRight) || held("shoot");
    si.mouseHeld = mouseMode && (in_.mouseLeft || in_.mouseRight);
    si.aimX = (in_.mouseX + world_->offX) / zoom_ - sim_->P.x;
    si.aimY = (in_.mouseY + world_->offY) / zoom_ - (sim_->P.y - 6);
    si.padAimX = in_.padRX;
    si.padAimY = in_.padRY;
    si.keyboardScheme = scheme_ == Scheme::Keys;
    return si;
}

// ───────────────────────────── music ─────────────────────────────

// Picks the track for what is happening right now: boss, fight, death, town, dungeon, night or the zone's own theme.
void App::updateMusic() {
    Sim& sim = *sim_;
    const json& states = db_.audio.at("states");
    auto state = [&](const char* k) { return states.value(k, std::string()); };
    static int fightT = 0, calmT = 9999;
    static int fightLevel = 0;
    std::string track;
    bool loop = true;
    if (sim.mode == Mode::Title) track = state("title");
    else if (sim.mode == Mode::Dead) {
        track = state("death");
        loop = false;
    } else if (sim.mode == Mode::Victory) {
        track = state("victory");
        loop = false;
    } else {
        // Fights need a moment to count (one stray fox is not a battle) and linger a little after the last blow
        const int level = sim.combatLevel();
        if (level > 0) {
            fightT++;
            calmT = 0;
            fightLevel = std::max(fightLevel, level);
        } else if (++calmT > 240) {
            fightT = 0;
            fightLevel = 0;
        }
        const Map& m = sim.map();
        if (fightLevel >= 3) track = state("boss");
        else if (fightLevel == 2 && fightT > 30) track = state("elite");
        else if (fightLevel == 1 && fightT > 150) track = state("combat");
        else if (m.dungeon) track = m.dungeon->music;
        else if (const TownDef* t = db_.town(sim.townId)) track = t->music;
        else if (sim.isNight()) track = state("night");
        else if (const ZoneDef* z = db_.zone(sim.zoneId)) track = z->music;
    }
    audio_.setMusic(track, loop);
    audio_.update();
}

// ───────────────────────────── frame ─────────────────────────────

void App::resize() {
    g_.syncSize();
    const int DW = g_.width(), DH = g_.height();
    // Whole-number zoom that shows about 22x13 tiles, but never fewer than 15x9 on small screens
    int z = std::max(1, static_cast<int>(std::lround(std::min(DW / (TILE * 22.0), DH / (TILE * 13.0)))));
    while (z > 1 && (DW / static_cast<double>(z * TILE) < 15 || DH / static_cast<double>(z * TILE) < 9)) z--;
    zoom_ = z;
    ui_ = std::max(0.5, std::max(1.0, std::min(DW / 1280.0, DH / 720.0)) * set_.ui);
    sim_->viewW = static_cast<double>(DW) / zoom_;
    sim_->viewH = static_cast<double>(DH) / zoom_;
}

void App::render() {
    Sim& sim = *sim_;
    world_->showTargetMark = scheme_ != Scheme::Mouse && std::hypot(in_.padRX, in_.padRY) <= 0.4;
    world_->render(sim, zoom_, sim.mode != Mode::Title);
    g_.setTransform(ui_);
    g_.setAlpha(1);
    hits_.clear();
    switch (sim.mode) {
        case Mode::Title: drawTitle(); break;
        default:
            drawHud();
            if (sim.mode == Mode::Inv) drawInventory();
            if (sim.mode == Mode::Craft) drawCrafting();
            if (sim.mode == Mode::Shop) drawShop();
            if (sim.mode == Mode::Sign) drawSign();
            if (sim.mode == Mode::Quests) drawQuests();
            if (sim.mode == Mode::WorldMap) drawWorldMap();
            if (sim.mode == Mode::Travel) drawTravel();
            if (sim.mode == Mode::Pause) drawPause();
            if (sim.mode == Mode::Dead) drawDead();
            if (sim.mode == Mode::Victory) drawVictory();
            break;
    }
    if (sim.fade) {
        const double a = sim.fade->t < sim.fade->dur ? static_cast<double>(sim.fade->t) / sim.fade->dur : 2.0 - static_cast<double>(sim.fade->t) / sim.fade->dur;
        g_.setTransform(1);
        g_.fillRect(0, 0, g_.width(), g_.height(), Color(8, 7, 12, static_cast<uint8_t>(255 * std::clamp(a, 0.0, 1.0))));
    }
    g_.setTransform(1);
    g_.present();
}

void App::frame() {
    frameNo_++;
    const uint64_t now = SDL_GetPerformanceCounter();
    const double dt = std::min(0.1, static_cast<double>(now - lastCounter_) / SDL_GetPerformanceFrequency());
    lastCounter_ = now;
    static int lastW = 0, lastH = 0;
    if (g_.syncSize() || lastW != g_.width() || lastH != g_.height() || in_.resized) {
        lastW = g_.width();
        lastH = g_.height();
        resize();
    }
    handleInput();
    if (noticeT_ > 0) noticeT_--;

    // Fixed 60 Hz simulation, whatever the display's refresh rate
    const std::string shot = platform::queryParam("shot");
    acc_ += shot.empty() ? dt : 1.0 / 60.0;
    int n = 0;
    const double STEP = 1.0 / 60.0;
    while (acc_ >= STEP && n < 6) {
        sim_->tick(buildSimInput());
        acc_ -= STEP;
        n++;
    }
    if (n == 6) acc_ = 0;
    for (const auto& s : sim_->sfx) audio_.play(s);
    sim_->sfx.clear();
    if (sim_->wantSave) {
        writeSave();
        readSlots();
    }
    updateMusic();
    platform::setCrosshair(sim_->mode == Mode::Play && scheme_ == Scheme::Mouse);
    render();

    // --shot=file.png: render a number of frames, save a screenshot and exit (desktop only)
    if (!shot.empty() && platform::canQuit()) {
        const std::string frames = platform::queryParam("frames");
        if (frameNo_ >= (frames.empty() ? 90 : std::atoi(frames.c_str()))) {
            render();
            g_.screenshot(shot);
            running = false;
        }
    }
}

}   // namespace hc

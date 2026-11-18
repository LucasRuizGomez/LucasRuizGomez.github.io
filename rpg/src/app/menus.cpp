// The two menus.
//  - Title menu: full screen over a slow pan of the world. Continue, new game (name + save slot),
//    load game (slot list with details, delete), settings, credits.
//  - Pause menu (Esc while playing): a drawer on the left with the hero's card and a shorter list
//    (resume, save, settings, controls, quit), and a context panel on the right: the current quests,
//    the settings, or the control reference.
// Both share one settings component.
#include "app/app.h"
#include "app/palette.h"
#include "engine/platform.h"

#include <cmath>
#include <cstdio>

namespace hc {

namespace {
std::string playTime(int ticks) {
    char buf[32];
    const int mins = ticks / 3600;
    if (mins >= 60) std::snprintf(buf, sizeof buf, "%dh %02dm", mins / 60, mins % 60);
    else std::snprintf(buf, sizeof buf, "%d min", mins);
    return buf;
}
bool isUp(const std::string& c) { return c == "ArrowUp" || c == "KeyW"; }
bool isDown(const std::string& c) { return c == "ArrowDown" || c == "KeyS"; }
bool isLeft(const std::string& c) { return c == "ArrowLeft" || c == "KeyA"; }
bool isRight(const std::string& c) { return c == "ArrowRight" || c == "KeyD"; }
bool isEnter(const std::string& c) { return c == "Enter" || c == "NumpadEnter"; }
}   // namespace

void App::notice(const std::string& t) {
    notice_ = t;
    noticeT_ = 150;
}

// ───────────────────────────── settings (shared) ─────────────────────────────

std::vector<App::OptionRow> App::optionRows() {
    std::vector<OptionRow> rows;
    auto pct = [](double v) { return std::to_string(static_cast<int>(std::lround(v * 100))) + "%"; };
    auto step = [](double v, int d) { return std::clamp(std::round((v + d * 0.1) * 10) / 10, 0.0, 1.0); };
    rows.push_back({"Sound", "", "", nullptr, nullptr, true});
    rows.push_back({"Sound effects", pct(set_.sfx), "", [this, step](int d) {
                        set_.sfx = step(set_.sfx, d);
                        applySettings();
                        audio_.play("pick");
                    }});
    rows.push_back({"Music", pct(set_.music), "", [this, step](int d) {
                        set_.music = step(set_.music, d);
                        applySettings();
                    }});
    rows.push_back({"Mute everything", set_.muted ? "On" : "Off", "", [this](int) {
                        set_.muted = !set_.muted;
                        applySettings();
                    }});
    rows.push_back({"Display", "", "", nullptr, nullptr, true});
    rows.push_back({"Interface size", pct(set_.ui), "", [this](int d) {
                        static const double SIZES[4] = {0.75, 1.0, 1.25, 1.5};
                        int i = 1;
                        for (int k = 0; k < 4; k++)
                            if (std::abs(SIZES[k] - set_.ui) < 0.01) i = k;
                        set_.ui = SIZES[std::clamp(i + d, 0, 3)];
                        resize();
                    }});
    if (platform::canQuit())
        rows.push_back({"Fullscreen", set_.fullscreen ? "On" : "Off", "", [this](int) {
                            set_.fullscreen = !set_.fullscreen;
                            g_.setFullscreen(set_.fullscreen);
                        }});
    rows.push_back({"Controls", "", "", nullptr, nullptr, true});
    for (const auto& a : db_.actions) {
        std::string keys;
        for (const auto& k : set_.binds[a.id]) keys += (keys.empty() ? "" : " / ") + keyLabel(k);
        const std::string id = a.id;
        rows.push_back({a.label, keys.empty() ? "-" : keys, id, nullptr, [this, id] { rebind_ = id; }});
    }
    rows.push_back({"Reset controls to defaults", "", "", nullptr, [this] {
                        set_.binds.clear();
                        for (const auto& a : db_.actions) set_.binds[a.id] = a.keys;
                        saveSettings();
                        notice("Controls reset.");
                    }});
    return rows;
}

void App::drawSettings(double X, double Y, double W, double H) {
    const auto rows = optionRows();
    const double rowH = 32, top = Y + 58;
    const int visible = std::max(3, static_cast<int>((H - 100) / rowH));
    optSel_ = std::clamp(optSel_, 0, static_cast<int>(rows.size()) - 1);
    if (rows[optSel_].header) optSel_++;
    if (optSel_ < optScroll_ + 1) optScroll_ = std::max(0, optSel_ - 1);
    if (optSel_ >= optScroll_ + visible) optScroll_ = optSel_ - visible + 1;
    panel(X, Y, W, H);
    text("Settings", X + 24, Y + 38, pal::text, 22, 1);
    text("Movement (WASD / arrows), Enter and Esc are fixed", X + W - 24, Y + 38, pal::faint, 12, 0, Align::Right);
    for (int i = optScroll_; i < std::min(static_cast<int>(rows.size()), optScroll_ + visible); i++) {
        const OptionRow& r = rows[i];
        const double y = top + (i - optScroll_) * rowH;
        if (r.header) {
            text(r.label, X + 24, y + rowH / 2 + 4, pal::gold, 13, 2, Align::Left, Baseline::Middle);
            g_.fillRect(X + 24 + textW(r.label, 13, 2) + 10, y + rowH / 2 + 4, W - 58 - textW(r.label, 13, 2), 1, pal::line);
            continue;
        }
        const bool sel = i == optSel_;
        row(X + 20, y, W - 40, rowH - 4, sel);
        text(r.label, X + 34, y + rowH / 2 - 2, sel ? pal::text : pal::muted, 14, 1, Align::Left, Baseline::Middle);
        const bool waiting = !rebind_.empty() && r.bind == rebind_;
        const std::string val = waiting ? "Press a key...  (Esc cancels)" : r.adjust ? "<   " + r.value + "   >" : r.value;
        text(val, X + W - 34, y + rowH / 2 - 2, waiting ? pal::gold : pal::text, 14, 1, Align::Right, Baseline::Middle);
        hit(X + 20, y, W - 40, rowH - 4,
            [this, i, X, W] {
                const auto rs = optionRows();
                if (i >= static_cast<int>(rs.size())) return;
                optSel_ = i;
                if (rs[i].adjust) {
                    rs[i].adjust(in_.mouseX / ui_ < X + W - 90 ? -1 : 1);   // "<" lowers, ">" raises
                    saveSettings();
                } else if (rs[i].act) rs[i].act();
            },
            [this, i] {
                if (rebind_.empty()) optSel_ = i;
            });
    }
    text("Up / Down choose · Left / Right change · Enter select or rebind · Esc back", X + W / 2, Y + H - 20, pal::muted, 12, 0, Align::Center);
}

bool App::settingsKey(const std::string& code) {
    const auto rows = optionRows();
    const int n = static_cast<int>(rows.size());
    auto move = [&](int d) {
        do optSel_ = (optSel_ + d + n) % n;
        while (rows[optSel_].header);
        audio_.play("uiMove");
    };
    if (isUp(code)) move(-1);
    else if (isDown(code)) move(1);
    else if ((isLeft(code) || isRight(code)) && rows[optSel_].adjust) {
        rows[optSel_].adjust(isLeft(code) ? -1 : 1);
        saveSettings();
    } else if (isEnter(code) || code == "Space") {
        if (rows[optSel_].act) rows[optSel_].act();
        else if (rows[optSel_].adjust) {
            rows[optSel_].adjust(1);
            saveSettings();
        }
        audio_.play("ui");
    } else if (code == "Escape") {
        saveSettings();
        return true;
    }
    return false;
}

// ───────────────────────────── title menu ─────────────────────────────

std::vector<App::MenuEntry> App::titleEntries() {
    std::vector<MenuEntry> e;
    const int newest = newestSlot();
    if (newest >= 0) {
        const SaveSlot& s = slots_[newest];
        e.push_back({"Continue", s.name + " · level " + std::to_string(s.level) + " · " + s.place, [this, newest] { loadSlot(newest); }});
    }
    e.push_back({"New game", "Start a new adventure in an empty save slot", [this] {
                     titleView_ = TitleView::NewGame;
                     newSlot_ = std::max(0, freeSlot());
                     confirmOverwrite_ = false;
                     heroName_ = "Wanderer";
                 }});
    if (newest >= 0)
        e.push_back({"Load game", "Pick up any of your saved adventures", [this] {
                         titleView_ = TitleView::Load;
                         loadSel_ = std::max(0, newestSlot());
                         confirmDelete_ = -1;
                     }});
    e.push_back({"Settings", "Sound, interface size and key bindings", [this] {
                     titleView_ = TitleView::Settings;
                     optSel_ = 1;
                     optScroll_ = 0;
                 }});
    e.push_back({"Credits", "Who made the art and the music", [this] { titleView_ = TitleView::Credits; }});
    if (platform::canQuit()) e.push_back({"Quit", "Close the game", [this] { running = false; }});
    return e;
}

void App::drawTitle() {
    dim(0.62);
    switch (titleView_) {
        case TitleView::Main: drawTitleMain(); break;
        case TitleView::Load: drawLoadMenu(); break;
        case TitleView::NewGame: drawNewGame(); break;
        case TitleView::Credits: drawCredits(); break;
        case TitleView::Settings: {
            const double W = std::min(680.0, uiW() - 40), H = std::min(uiH() - 60, 620.0);
            drawSettings(std::round((uiW() - W) / 2), std::round((uiH() - H) / 2), W, H);
            break;
        }
    }
    if (noticeT_ > 0) {
        g_.setAlpha(std::min(1.0, noticeT_ / 30.0));
        shadowText(notice_, uiW() / 2, uiH() - 52, pal::ltGold, 14, 1, Align::Center);
        g_.setAlpha(1);
    }
}

void App::drawTitleMain() {
    const double left = std::max(60.0, uiW() * 0.09), cy = uiH() / 2;
    shadowText("HOLLOW", left, cy - 150, pal::ltText, 76, 2);
    shadowText("CRYPT", left, cy - 84, pal::ltGold, 76, 2);
    text("An open world of towns, dungeons and things that bite.", left + 4, cy - 52, pal::ltMuted, 16);
    g_.fillRect(left + 4, cy - 34, 300, 2, Color(255, 210, 122, 150));

    const auto entries = titleEntries();
    titleSel_ = std::clamp(titleSel_, 0, static_cast<int>(entries.size()) - 1);
    for (size_t i = 0; i < entries.size(); i++) {
        const bool sel = static_cast<int>(i) == titleSel_;
        const double y = cy + 2 + i * 44.0;
        if (sel) {
            g_.fillRect(left - 14, y - 4, 340, 38, Color(255, 210, 122, 38));
            g_.fillRect(left - 14, y - 4, 4, 38, pal::ltGold);
        }
        shadowText(entries[i].label, left + 6, y + 22, sel ? pal::ltGold : pal::ltText, sel ? 26 : 22, sel ? 2 : 1);
        hit(left - 14, y - 4, 340, 40, [this, i] {
            titleSel_ = static_cast<int>(i);
            handleKey("Enter", false);
        }, [this, i] { titleSel_ = static_cast<int>(i); });
    }
    text(entries[titleSel_].hint, left + 6, cy + 20 + entries.size() * 44.0, pal::ltMuted, 14);

    // The most recent save, as a card on the right
    const int newest = newestSlot();
    if (newest >= 0 && uiW() > 900) {
        const SaveSlot& s = slots_[newest];
        const double W = 330, H = 178, X = uiW() - W - std::max(40.0, uiW() * 0.07), Y = cy - 10;
        panel(X, Y, W, H);
        text("Last adventure", X + 22, Y + 32, pal::faint, 12, 2);
        const Frames& idle = spr_.character("ch8", "").get("idle");
        drawSpr(idle[(frameNo_ / 12) % idle.size()], X + 16, Y + 40, 72);
        text(s.name, X + 100, Y + 66, pal::text, 20, 1);
        text("Level " + std::to_string(s.level) + (s.victory ? " · King slain" : ""), X + 100, Y + 88, pal::gold, 14, 1);
        text(s.place, X + 100, Y + 108, pal::muted, 13);
        text("Played " + playTime(s.time) + " · day " + std::to_string(s.day), X + 22, Y + 140, pal::muted, 13);
        text("Saved " + s.savedAt + " · slot " + std::to_string(newest + 1), X + 22, Y + 160, pal::faint, 12);
    }
    text("Up / Down choose · Enter select · mouse and gamepad work too", uiW() / 2, uiH() - 22, Color(236, 230, 216, 150), 12, 0, Align::Center);
    text("v" HC_VERSION, uiW() - 16, uiH() - 22, Color(236, 230, 216, 110), 12, 0, Align::Right);
}

void App::drawLoadMenu() {
    const double W = std::min(720.0, uiW() - 40), rowH = 86, H = 110 + slots_.size() * rowH, X = std::round((uiW() - W) / 2), Y = std::round((uiH() - H) / 2);
    panel(X, Y, W, H);
    text("Load game", X + 24, Y + 40, pal::text, 22, 1);
    text(std::to_string(slots_.size()) + " save slots, kept in this browser", X + W - 24, Y + 40, pal::faint, 12, 0, Align::Right);
    loadSel_ = std::clamp(loadSel_, 0, static_cast<int>(slots_.size()) - 1);
    for (size_t i = 0; i < slots_.size(); i++) {
        const SaveSlot& s = slots_[i];
        const double y = Y + 58 + i * rowH;
        const bool sel = static_cast<int>(i) == loadSel_;
        row(X + 20, y, W - 40, rowH - 8, sel);
        hit(X + 20, y, W - 40, rowH - 8, [this, i] {
            if (loadSel_ == static_cast<int>(i)) handleKey("Enter", false);
            else {
                loadSel_ = static_cast<int>(i);
                confirmDelete_ = -1;
            }
        });
        text("Slot " + std::to_string(i + 1), X + 36, y + 24, pal::faint, 12, 2);
        if (!s.used) {
            text("Empty", X + 36, y + 50, pal::faint, 16, 1);
            continue;
        }
        text(s.name, X + 36, y + 48, pal::text, 18, 1);
        text("Level " + std::to_string(s.level) + " · " + s.place + (s.victory ? " · King slain" : ""), X + 36, y + 67, pal::gold, 13, 1);
        text("Played " + playTime(s.time) + " · day " + std::to_string(s.day) + " · " + std::to_string(s.deaths) + (s.deaths == 1 ? " death" : " deaths"), X + W - 36, y + 40, pal::muted, 13, 0, Align::Right);
        if (confirmDelete_ == static_cast<int>(i)) text("Press Delete again to erase this save for good", X + W - 36, y + 62, pal::bad, 13, 1, Align::Right);
        else text("Saved " + s.savedAt, X + W - 36, y + 62, pal::faint, 12, 0, Align::Right);
    }
    const double fy = Y + H - 26;
    double fx = X + 24;
    fx += keycap("Enter", fx, fy - 11, 22) + 8;
    text("Load", fx, fy, pal::muted, 13, 0, Align::Left, Baseline::Middle);
    fx += 48;
    const double dx0 = fx;
    fx += keycap("Delete", fx, fy - 11, 22) + 8;
    text("Erase", fx, fy, pal::muted, 13, 0, Align::Left, Baseline::Middle);
    hit(dx0, fy - 13, fx - dx0 + 44, 26, [this] { handleKey("Delete", false); });
    fx += 54;
    const double bx0 = fx;
    fx += keycap("Esc", fx, fy - 11, 22) + 8;
    text("Back", fx, fy, pal::muted, 13, 0, Align::Left, Baseline::Middle);
    hit(bx0, fy - 13, fx - bx0 + 40, 26, [this] { handleKey("Escape", false); });
}

void App::drawNewGame() {
    const double W = std::min(560.0, uiW() - 40), H = 330, X = std::round((uiW() - W) / 2), Y = std::round((uiH() - H) / 2);
    panel(X, Y, W, H);
    text("New game", X + 24, Y + 40, pal::text, 22, 1);
    text("Name your hero", X + 24, Y + 78, pal::muted, 13, 1);
    box(X + 24, Y + 88, W - 48, 44, pal::well, pal::sel, 2);
    const bool caret = (frameNo_ / 30) % 2 == 0;
    text(heroName_ + (caret ? "|" : ""), X + 38, Y + 110, pal::text, 20, 1, Align::Left, Baseline::Middle);
    text("Type to change · Backspace deletes", X + W - 24, Y + 78, pal::faint, 12, 0, Align::Right);

    text("Save slot", X + 24, Y + 166, pal::muted, 13, 1);
    const SaveSlot& s = slots_[newSlot_];
    row(X + 24, Y + 176, W - 48, 44, false);
    text("<", X + 40, Y + 197, pal::text, 18, 2, Align::Left, Baseline::Middle);
    text(">", X + W - 40, Y + 197, pal::text, 18, 2, Align::Right, Baseline::Middle);
    text("Slot " + std::to_string(newSlot_ + 1) + (s.used ? " · " + s.name + ", level " + std::to_string(s.level) : " · empty"), X + W / 2, Y + 197, s.used ? pal::bad : pal::text, 15, 1, Align::Center, Baseline::Middle);
    hit(X + 24, Y + 176, (W - 48) / 2, 44, [this] { handleKey("ArrowLeft", false); });
    hit(X + W / 2, Y + 176, (W - 48) / 2, 44, [this] { handleKey("ArrowRight", false); });
    if (s.used) text(confirmOverwrite_ ? "Press Enter again to overwrite " + s.name + "'s save" : "This slot is in use: starting here erases it", X + W / 2, Y + 240, pal::bad, 13, 1, Align::Center);

    const double bw = 220, bx = X + (W - bw) / 2;
    button(bx, Y + 256, bw, 40, confirmOverwrite_ ? "Overwrite and begin" : "Begin", true, 17);
    hit(bx, Y + 256, bw, 40, [this] { handleKey("Enter", false); });
    text("Left / Right slot · Enter begin · Esc back", X + W / 2, Y + H - 14, pal::muted, 12, 0, Align::Center);
}

void App::drawCredits() {
    const double W = std::min(700.0, uiW() - 40), H = 400, X = std::round((uiW() - W) / 2), Y = std::round((uiH() - H) / 2);
    panel(X, Y, W, H);
    text("Credits", X + 24, Y + 40, pal::text, 22, 1);
    static const char* LINES[][2] = {
        {"Game", "Lucas Ruiz Gomez · lukeda.me"},
        {"Music", "Juhani Junkala, \"GT RPG Soundtrack\" (CC BY 4.0, opengameart.org)"},
        {"Sound effects", "artisticdude, \"RPG Sound Pack\" · rubberduck, \"80 CC0 RPG SFX\" (CC0)"},
        {"Towns, props, water", "ArMM1998, \"Zelda-like tilesets and sprites\" (CC0)"},
        {"Characters, terrain", "CraftPix: medieval fortress kit, hunt animals, rocks and stones"},
        {"Interface art", "CraftPix: basic pixel art UI for RPG"},
        {"Typefaces", "Almendra, Ana Sanfelippo · Pirata One, Rodrigo Fuenzalida & Nicolas Massi (SIL OFL 1.1)"},
        {"Libraries", "SDL2, SDL_mixer, stb, nlohmann/json, doctest, Emscripten"},
    };
    for (size_t i = 0; i < sizeof LINES / sizeof LINES[0]; i++) {
        text(LINES[i][0], X + 28, Y + 84 + i * 34.0, pal::gold, 13, 2);
        text(LINES[i][1], X + 28, Y + 101 + i * 34.0, pal::text, 13);
    }
    text("Full licence details are in CREDITS.md · Esc back", X + W / 2, Y + H - 18, pal::muted, 12, 0, Align::Center);
    hit(0, 0, uiW(), uiH(), [this] { handleKey("Escape", false); });
}

void App::titleKey(const std::string& code, bool shift) {
    switch (titleView_) {
        case TitleView::Main: {
            const auto entries = titleEntries();
            const int n = static_cast<int>(entries.size());
            if (isUp(code)) {
                titleSel_ = (titleSel_ + n - 1) % n;
                audio_.play("uiMove");
            } else if (isDown(code)) {
                titleSel_ = (titleSel_ + 1) % n;
                audio_.play("uiMove");
            } else if (isEnter(code) || code == "Space") {
                audio_.play("ui");
                entries[std::clamp(titleSel_, 0, n - 1)].act();
            }
            break;
        }
        case TitleView::Load:
            if (isUp(code) || isDown(code)) {
                const int n = static_cast<int>(slots_.size());
                loadSel_ = (loadSel_ + (isUp(code) ? n - 1 : 1)) % n;
                confirmDelete_ = -1;
                audio_.play("uiMove");
            } else if (isEnter(code)) {
                if (slots_[loadSel_].used) loadSlot(loadSel_);
                else audio_.play("deny");
            } else if (code == "Delete" || code == "Backspace" || code == "KeyX") {
                if (!slots_[loadSel_].used) audio_.play("deny");
                else if (confirmDelete_ == loadSel_) {
                    deleteSlot(loadSel_);
                    confirmDelete_ = -1;
                    notice("Save erased.");
                    if (newestSlot() < 0) titleView_ = TitleView::Main;
                } else confirmDelete_ = loadSel_;
            } else if (code == "Escape") {
                titleView_ = TitleView::Main;
                titleSel_ = 0;
            }
            break;
        case TitleView::NewGame: {
            const int n = static_cast<int>(slots_.size());
            if (code == "Escape") titleView_ = TitleView::Main;
            else if (code == "ArrowLeft" || code == "ArrowRight" || code == "ArrowUp" || code == "ArrowDown") {
                newSlot_ = (newSlot_ + (code == "ArrowLeft" || code == "ArrowUp" ? n - 1 : 1)) % n;
                confirmOverwrite_ = false;
                audio_.play("uiMove");
            } else if (isEnter(code)) {
                if (slots_[newSlot_].used && !confirmOverwrite_) {
                    confirmOverwrite_ = true;
                    audio_.play("deny");
                } else startNewGame();
            } else if (code == "Backspace") {
                if (!heroName_.empty()) heroName_.pop_back();
            } else if (heroName_.size() < 14) {
                char ch = 0;
                if (code.rfind("Key", 0) == 0 && code.size() == 4) ch = shift || heroName_.empty() || heroName_.back() == ' ' ? code[3] : static_cast<char>(code[3] - 'A' + 'a');
                else if (code.rfind("Digit", 0) == 0 && code.size() == 6) ch = code[5];
                else if (code == "Space" && !heroName_.empty() && heroName_.back() != ' ') ch = ' ';
                else if (code == "Minus") ch = '-';
                else if (code == "Quote") ch = '\'';
                if (ch) heroName_ += ch;
            }
            break;
        }
        case TitleView::Settings:
            if (settingsKey(code)) titleView_ = TitleView::Main;
            break;
        case TitleView::Credits:
            if (code == "Escape" || isEnter(code) || code == "Space") titleView_ = TitleView::Main;
            break;
    }
}

// ───────────────────────────── pause menu ─────────────────────────────

std::vector<App::MenuEntry> App::pauseEntries() {
    return {
        {"Resume", "Back to the game", [this] { closePause(); }},
        {"Save game", "Write your progress to slot " + std::to_string(slot_ + 1) + " now", [this] {
             writeSave();
             notice("Game saved to slot " + std::to_string(slot_ + 1) + ".");
             audio_.play("quest");
         }},
        {"Settings", "Sound, interface size and key bindings", [this] {
             pauseView_ = PauseView::Settings;
             optSel_ = 1;
             optScroll_ = 0;
         }},
        {"Controls", "What every key and button does", [this] { pauseView_ = PauseView::Controls; }},
        {"Save and quit", "Save, then return to the title menu", [this] { quitToTitle(); }},
    };
}

void App::drawPause() {
    Sim& sim = *sim_;
    const Player& P = sim.P;
    dim(0.55);
    // Left drawer: the hero's card, then the entries
    const double DW = std::min(340.0, uiW() * 0.42), DH = uiH() - 32, DX = 16, DY = 16;
    panel(DX, DY, DW, DH);
    text("Paused", DX + 22, DY + 38, pal::faint, 13, 2);
    const Frames& idle = spr_.character("ch8", "").get("idle");
    drawSpr(idle[(frameNo_ / 12) % idle.size()], DX + 12, DY + 44, 72);
    text(P.name, DX + 94, DY + 72, pal::text, 20, 1);
    text("Level " + std::to_string(P.level), DX + 94, DY + 94, pal::gold, 14, 1);
    const bool capped = P.level >= db_.tuning.maxLevel;
    bar(DX + 24, DY + 128, DW - 48, 8, capped ? 1 : static_cast<double>(P.xp) / sim.xpToNext(), pal::xp, Color(190, 170, 140));
    text(capped ? "Highest level reached" : std::to_string(P.xp) + " / " + std::to_string(sim.xpToNext()) + " experience", DX + 24, DY + 154, pal::muted, 12);
    std::string place = sim.map().name;
    if (sim.map().outdoor) {
        const TownDef* t = db_.town(sim.townId);
        const ZoneDef* z = db_.zone(sim.zoneId);
        place = t ? t->name : z ? z->name : place;
    }
    const std::pair<std::string, std::string> facts[] = {{"Where", place}, {"When", sim.clockText()}, {"Played", playTime(sim.time)}, {"Gold", std::to_string(sim.countItem("gold"))}};
    for (size_t i = 0; i < 4; i++) {
        text(facts[i].first, DX + 24, DY + 180 + i * 19.0, pal::muted, 13);
        text(facts[i].second, DX + DW - 24, DY + 180 + i * 19.0, pal::text, 13, 1, Align::Right);
    }
    g_.fillRect(DX + 20, DY + 262, DW - 40, 1, pal::line);

    const auto entries = pauseEntries();
    pauseSel_ = std::clamp(pauseSel_, 0, static_cast<int>(entries.size()) - 1);
    for (size_t i = 0; i < entries.size(); i++) {
        const bool sel = static_cast<int>(i) == pauseSel_ && pauseView_ == PauseView::Main;
        const double y = DY + 278 + i * 46.0;
        button(DX + 22, y, DW - 44, 38, entries[i].label, sel || (i == 2 && pauseView_ == PauseView::Settings) || (i == 3 && pauseView_ == PauseView::Controls), 16);
        hit(DX + 22, y, DW - 44, 38, [this, i] {
            pauseView_ = PauseView::Main;
            pauseSel_ = static_cast<int>(i);
            handleKey("Enter", false);
        }, [this, i] {
            if (pauseView_ == PauseView::Main) pauseSel_ = static_cast<int>(i);
        });
    }
    const double hy = DY + 278 + entries.size() * 46.0 + 14;
    {
        const auto hint = wrap(noticeT_ > 0 ? notice_ : entries[pauseSel_].hint, DW - 48, 13);
        for (size_t i = 0; i < hint.size(); i++) text(hint[i], DX + DW / 2, hy + i * 17.0, noticeT_ > 0 ? pal::ok : pal::muted, 13, noticeT_ > 0 ? 1 : 0, Align::Center);
    }
    text(scheme_ == Scheme::Pad ? "B or Start: resume" : "Esc: resume", DX + DW / 2, DY + DH - 20, pal::gold, 13, 1, Align::Center);

    // Right side: context
    const double RX = DX + DW + 16, RW = uiW() - RX - 16;
    if (RW < 300) return;
    if (pauseView_ == PauseView::Settings) {
        drawSettings(RX, DY, std::min(RW, 700.0), DH);
        return;
    }
    if (pauseView_ == PauseView::Controls) {
        const double W = std::min(RW, 620.0);
        panel(RX, DY, W, DH);
        text("Controls", RX + 24, DY + 38, pal::text, 22, 1);
        text(scheme_ == Scheme::Pad ? "Showing: gamepad" : "Showing: keyboard and mouse", RX + W - 24, DY + 38, pal::faint, 12, 0, Align::Right);
        std::vector<std::pair<std::string, std::string>> lines;
        if (scheme_ == Scheme::Pad)
            lines = {{"Move", "Left stick / d-pad"}, {"Attack", "X"}, {"Skill tree", "RB"}, {"Crossbow", "RT (right stick aims)"}, {"Whirlwind", "LT"}, {"Dodge roll", "B"},
                     {"Use, talk", "A"},             {"Quick heal", "Y"},  {"Bag / crafting", "Back / LB"},       {"Mount", "L3"},     {"World map", "R3"}, {"Menu", "Start"}};
        else {
            lines = {{"Move", "WASD / arrows"}, {"Attack", bindLabel("attack") + "  ·  left click"}, {"Crossbow", bindLabel("shoot") + " (auto-aim)  ·  right click"}};
            for (const auto& a : db_.actions)
                if (a.id != "attack" && a.id != "shoot") lines.push_back({a.label, bindLabel(a.id)});
            lines.push_back({"Menu", "Esc / P"});
        }
        const double lh = std::min(26.0, (DH - 110) / lines.size());
        for (size_t i = 0; i < lines.size(); i++) {
            const double y = DY + 74 + i * lh;
            if (i % 2 == 0) g_.fillRect(RX + 20, y - lh * 0.62, W - 40, lh, Color(168, 134, 90, 40));
            text(lines[i].first, RX + 32, y, pal::muted, 14);
            text(lines[i].second, RX + W - 32, y, pal::text, 14, 1, Align::Right);
        }
        text("Rebind keys under Settings · Esc back", RX + W / 2, DY + DH - 20, pal::muted, 12, 0, Align::Center);
        return;
    }
    // Journal: what you are in the middle of
    const double W = std::min(RW, 520.0);
    const auto quests = sim.activeQuests();
    double H = 76;
    for (const QuestDef* q : quests) H += 34 + q->objectives.size() * 20.0;
    if (quests.empty()) H += 50;
    H = std::min(H, DH);
    panel(RX, DY, W, H);
    text("Journal", RX + 24, DY + 38, pal::text, 22, 1);
    text(bindLabel("quests") + " opens the full quest log", RX + W - 24, DY + 38, pal::faint, 12, 0, Align::Right);
    double y = DY + 70;
    if (quests.empty()) text("Nothing on your list. Look for people with a yellow mark.", RX + 24, y + 8, pal::muted, 14);
    for (const QuestDef* q : quests) {
        if (y + 30 > DY + H - 10) break;
        const bool ready = sim.questReady(*q);
        text(q->title, RX + 24, y, ready ? pal::ok : pal::text, 15, 1);
        text(q->main ? "main" : "side", RX + W - 24, y, pal::faint, 12, 1, Align::Right);
        y += 20;
        for (size_t i = 0; i < q->objectives.size(); i++) {
            const QuestObjective& o = q->objectives[i];
            const int have = sim.objectiveProgress(*q, i);
            text(o.label, RX + 36, y, have >= o.count ? pal::ok : pal::muted, 13);
            if (o.count > 1) text(std::to_string(have) + " / " + std::to_string(o.count), RX + W - 24, y, have >= o.count ? pal::ok : pal::muted, 13, 1, Align::Right);
            y += 20;
        }
        y += 14;
    }
}

void App::pauseKey(const std::string& code) {
    if (pauseView_ == PauseView::Settings) {
        if (settingsKey(code)) pauseView_ = PauseView::Main;
        return;
    }
    if (pauseView_ == PauseView::Controls) {
        if (code == "Escape" || isEnter(code) || isLeft(code)) pauseView_ = PauseView::Main;
        return;
    }
    const auto entries = pauseEntries();
    const int n = static_cast<int>(entries.size());
    if (isUp(code)) {
        pauseSel_ = (pauseSel_ + n - 1) % n;
        audio_.play("uiMove");
    } else if (isDown(code)) {
        pauseSel_ = (pauseSel_ + 1) % n;
        audio_.play("uiMove");
    } else if (isEnter(code) || code == "Space" || isRight(code)) {
        audio_.play("ui");
        entries[pauseSel_].act();
    } else if (code == "Escape" || code == "KeyP") closePause();
}

}   // namespace hc

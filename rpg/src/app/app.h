// The application shell: owns the platform objects and the simulation, runs the frame loop,
// routes input, and draws the interface (HUD, in-game screens, title menu, pause menu).
#pragma once
#include "engine/audio.h"
#include "engine/font.h"
#include "engine/gfx.h"
#include "engine/input.h"
#include "game/sim.h"
#include "view/sprites.h"
#include "view/world_view.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace hc {

struct Settings {
    double sfx = 0.8, music = 0.5, ui = 1.0;
    bool muted = false, fullscreen = false;
    std::map<std::string, std::vector<std::string>> binds;   // action id -> up to two key codes
};

struct SaveSlot {
    bool used = false;
    std::string name, place, savedAt;
    int level = 1, time = 0, day = 1, deaths = 0;
    bool victory = false;
};

class App {
  public:
    bool init(int argc, char** argv);
    void frame();
    void shutdown();
    bool running = true;
    static constexpr int DOLL_SLOTS = EQUIP_SLOTS + 1;   // worn gear plus the mount

  private:
    // ── platform and game ──
    Database db_;
    std::unique_ptr<Sim> sim_;
    Gfx g_;
    Fonts fonts_;
    Audio audio_;
    Input in_;
    Sprites spr_;
    std::unique_ptr<WorldView> world_;
    Settings set_;
    std::string root_;

    // ── frame state ──
    int zoom_ = 3;
    double ui_ = 1, acc_ = 0;
    uint64_t lastCounter_ = 0;
    long frameNo_ = 0;
    enum class Scheme { Keys, Mouse, Pad } scheme_ = Scheme::Keys;
    std::string padDir_;
    int padRepeat_ = 0;

    // ── saves ──
    std::vector<SaveSlot> slots_;
    int slot_ = -1;   // slot the running game saves into
    void readSlots();
    void writeSave();
    bool loadSlot(int i);
    void deleteSlot(int i);
    int freeSlot() const;
    int newestSlot() const;

    // ── settings ──
    void loadSettings();
    void saveSettings();
    void applySettings();
    std::string actionFor(const std::string& code) const;
    bool held(const std::string& action) const;
    std::string bindLabel(const std::string& action) const;
    std::string btnLabel(const std::string& action) const;
    void rebindTo(const std::string& code);

    // ── loop pieces (app.cpp) ──
    void resize();
    void handleInput();
    void handleKey(const std::string& code, bool shift);
    void handlePad();
    void handleClick(const MouseClick& c);
    void playAction(const std::string& action, Scheme from);
    SimInput buildSimInput();
    void updateMusic();
    void render();
    void runDebugSwitches();
    void openPause();
    void closePause();
    void quitToTitle();
    void startNewGame();
    void setMode(Mode m);

    // ── interface helpers (ui.cpp) ──
    struct Hit {
        double x, y, w, h;
        std::function<void()> click, hover;
    };
    std::vector<Hit> hits_;
    double uiW() const { return g_.width() / ui_; }
    double uiH() const { return g_.height() / ui_; }
    void hit(double x, double y, double w, double h, std::function<void()> click, std::function<void()> hover = nullptr);
    const Hit* hitAt(double ux, double uy) const;
    void text(const std::string& s, double x, double y, Color col, double size, int weight = 0, Align align = Align::Left, Baseline base = Baseline::Alphabetic);
    void shadowText(const std::string& s, double x, double y, Color col, double size, int weight = 0, Align align = Align::Left);
    double textW(const std::string& s, double size, int weight = 0);
    std::vector<std::string> wrap(const std::string& s, double maxW, double size, int weight = 0);
    void nine(const Sprite& img, double x, double y, double w, double h, int corner);
    void panel(double x, double y, double w, double h);
    void slotBox(double x, double y, double w, double h, bool dark = false);
    void button(double x, double y, double w, double h, const std::string& label, bool sel, double size = 15);
    void row(double x, double y, double w, double h, bool sel);
    void box(double x, double y, double w, double h, Color fill, Color border = Color(0, 0, 0, 0), double bw = 1);
    void drawSpr(const Sprite& img, double x, double y, double size);
    double keycap(const std::string& label, double x, double y, double h);
    void dim(double alpha);
    void closeButton(double x, double y);
    void bar(double x, double y, double w, double h, double frac, Color fill, Color back);
    Color rarityColor(const Item& it) const;

    // ── HUD and in-game screens (hud.cpp, screens.cpp) ──
    void drawHud();
    void drawMinimap();
    void drawStatus(double right, double top);
    void drawInventory();
    void drawCrafting();
    void drawShop();
    void drawSign();
    void drawQuests();
    void drawWorldMap();
    void drawTravel();
    void drawDead();
    void drawVictory();
    void centerCard(const std::string& title, const std::string& sub, const std::vector<std::pair<std::string, std::string>>& lines, const std::string& foot);
    int invSel_ = 0, craftSel_ = 0, craftScroll_ = 0, shopSel_ = 0, questSel_ = 0, travelSel_ = 0;
    std::string craftCat_ = "all";
    bool shopBuying_ = true;
    void setCraftCat(int step);
    void craftSelected(int times);
    std::vector<const TownDef*> travelList() const;

    // ── bag with paper doll, and the skill tree (bag.cpp) ──
    struct Zone {   // where a dragged item can land: kind 0 = bag cell, 1 = doll slot
        double x, y, w, h;
        int kind, index;
    };
    struct Drag {
        int kind = -1, index = -1;   // what the mouse picked up (same kinds as Zone)
        double x0 = 0, y0 = 0;
        bool moved = false, wasSel = false;
    };
    std::vector<Zone> zones_;
    Drag drag_;
    double invRect_[4] = {0, 0, 0, 0};
    int skillSel_ = 0;
    std::vector<Vec2> skillPos_;
    const Item* dollItem(int k) const;
    int invIndexOf(const Item* it) const;
    int selectedInvIndex() const;
    void invActivate();
    void invDiscard(bool all);
    void invMove(const std::string& dir);
    void invPress(int kind, int index);
    void invRelease(double ux, double uy);
    void drawSkills();
    void skillMove(const std::string& dir);

    // ── menus (menus.cpp) ──
    enum class TitleView { Main, Load, NewGame, Settings, Credits } titleView_ = TitleView::Main;
    enum class PauseView { Main, Settings, Controls } pauseView_ = PauseView::Main;
    int titleSel_ = 0, loadSel_ = 0, pauseSel_ = 0, optSel_ = 0, optScroll_ = 0, confirmDelete_ = -1, newSlot_ = 0;
    std::string heroName_ = "Wanderer", rebind_, notice_;
    int noticeT_ = 0;
    bool confirmOverwrite_ = false;
    struct MenuEntry {
        std::string label, hint;
        std::function<void()> act;
    };
    std::vector<MenuEntry> titleEntries();
    std::vector<MenuEntry> pauseEntries();
    void drawTitle();
    void drawTitleMain();
    void drawLoadMenu();
    void drawNewGame();
    void drawCredits();
    void drawPause();
    void titleKey(const std::string& code, bool shift);
    void pauseKey(const std::string& code);
    struct OptionRow {
        std::string label, value, bind;
        std::function<void(int)> adjust;
        std::function<void()> act;
        bool header = false;
    };
    std::vector<OptionRow> optionRows();
    void drawSettings(double x, double y, double w, double h);
    bool settingsKey(const std::string& code);   // returns true when the settings view should close
    void notice(const std::string& text);
};

}   // namespace hc

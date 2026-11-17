// The game simulation: one fixed 60 Hz tick() advances everything. It knows nothing about SDL,
// drawing or sound devices; the app feeds it input and reads its state (and its sound-event queue).
#pragma once
#include "game/world.h"

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace hc {

enum class Mode { Title, Play, Pause, Inv, Craft, Shop, Sign, Quests, WorldMap, Travel, Dead, Victory };
enum class EquipSlot { Weapon, Shield, Armor, Trinket };

struct Toast {
    std::string text;
    Color col;
    int t = 200;
};
struct Floater {
    double x = 0, y = 0, y0 = 0, vy = 0;
    int pop = 8, t = 56;
    std::string text;
    Color col;
};
struct Particle {
    double x = 0, y = 0, vx = 0, vy = 0, g = 0;
    int t = 20, s = 1;
    Color col;
};
struct Banner {
    std::string icon, title, sub;
    Color col;
    int t = 170;
};
struct AreaTitle {
    std::string text, sub;
    int t = 200;
};
struct Fade {
    int t = 0, dur = 22;
    std::function<void()> mid;
};
struct Focus {
    enum class Kind { None, Object, Tree, Door } kind = Kind::None;
    int obj = -1;   // index into the current map's objects
    int c = 0, r = 0;
    std::string label;
    double x = 0, y = 0;
};
struct QuestState {
    int state = 0;   // 0 = not taken, 1 = active, 2 = finished
    std::vector<int> counts;
};
struct Buff {
    std::string id;   // frenzy | protection | swift | fortune
    int t = 0;
};
struct Trail {
    double x = 0, y = 0;
    int face = 1, t = 10;
};

struct Player {
    std::string name = "Wanderer";
    double x = 0, y = 0;
    Dir dir = Dir::Down;
    int face = 1;
    bool moving = false;
    int animT = 0;
    int hp = 10, level = 1, xp = 0, heartCrystals = 0;
    std::vector<Item> inv;
    int equip[4] = {0, 0, 0, 0};   // item uid per EquipSlot, 0 = empty
    int atkT = 0, atkCd = 0, bowCd = 0, bowT = 0, iframes = 0, actCd = 0, roll = 0, rollCd = 0;
    double kbx = 0, kby = 0, rvx = 0, rvy = 0;
    std::vector<int> hitSet;
    std::vector<Trail> trail;
    std::string mount;          // item id of the mount being ridden, empty on foot
    int mountCast = 0, recallT = 0, whirlCd = 0, whirlT = 0;
    int combatT = 9999;         // ticks since the last blow given or taken
    std::vector<Buff> buffs;
    std::string lastTown = "greenhollow";
};

// What the player is holding down this tick. One-shot presses go through Sim::action().
struct SimInput {
    double moveX = 0, moveY = 0;            // -1..1, any angle
    bool holdAttack = false, holdShoot = false;
    bool mouseHeld = false;                 // the held attack/shot comes from a mouse button
    double aimX = 0, aimY = 0;              // vector from the hero's chest to the cursor (world pixels)
    double padAimX = 0, padAimY = 0;        // right stick
    bool keyboardScheme = true;             // false = mouse or gamepad drives aiming
};

struct ShopRow {
    std::string id;
    int qty = 1, price = 0;
    int bagIndex = -1;   // sell rows: which bag entry
};

class Sim {
  public:
    Sim(const Database& db, uint32_t seed);

    // ── lifecycle ──
    void prepareTitle();                       // builds the world so the title screen has something to show
    void newGame(const std::string& heroName);
    bool loadGame(const json& save);
    json saveGame() const;
    json saveSummary() const;                  // small header shown in the load menu
    void tick(const SimInput& in);
    void action(const std::string& id, bool mouse);   // attack, shoot, dodge, interact, heal, quick1..3, skill, mount, recall
    void transition(std::function<void()> mid);

    // ── state the view reads ──
    const Database& db;
    Rng rng;
    Mode mode = Mode::Title;
    int tickCount = 0;
    int time = 0;                               // play time in ticks, also drives the day
    std::string mapId = "over";
    std::map<std::string, Map> maps;
    Vec2 cam;
    double viewW = 352, viewH = 208;            // visible world size in art pixels, set by the app
    double shake = 0;
    std::vector<Toast> toasts;
    std::vector<Floater> floaters;
    std::vector<Particle> particles;
    std::optional<Banner> banner;
    std::optional<AreaTitle> area;
    std::optional<Fade> fade;
    Focus focus;
    Player P;
    bool doorOpen = false, victorySeen = false;
    int gateAt = -999, deadAt = 0, victoryAt = 0, hitstop = 0, deaths = 0, craftFlash = 0;
    bool nightPack = false;
    std::set<std::string> bossesDefeated, zonesFound, waypoints, shrinesFound;
    std::map<std::string, QuestState> quests;
    std::string signTitle, signText, shopId, zoneId, townId;
    std::vector<std::string> sfx;               // sound events since the last frame
    bool wantSave = false;                      // set when the game should be written to storage
    bool worldReady = false;

    Map& map() { return maps.at(mapId); }
    const Map& map() const { return maps.at(mapId); }
    Map& overworld() { return maps.at("over"); }

    // ── feedback ──
    void toast(const std::string& text, Color col = Color(236, 230, 216));
    void showBanner(const std::string& icon, const std::string& title, const std::string& sub, Color col = Color(138, 90, 12));
    void floater(double x, double y, const std::string& text, Color col);
    void burst(double x, double y, Color col, int n, double speed = 1.2, double grav = 0.06);
    void play(const std::string& sound) { sfx.push_back(sound); }

    // ── inventory and stats (inventory.cpp) ──
    int countItem(const std::string& id) const;
    bool hasItem(const std::string& id) const { return countItem(id) > 0; }
    bool addItem(Item item, bool quiet = false);
    bool removeItem(const std::string& id, int qty);
    const Item* equipped(EquipSlot s) const;
    bool isEquipped(const Item& it) const;
    void useOrEquip(int bagIndex);
    bool useHeal(const std::string& id);
    void quickHeal();
    void quickUse(const std::string& id);
    std::string itemName(const Item& it) const;
    std::vector<std::string> itemLines(const Item& it) const;   // stat and affix lines for tooltips
    int itemValue(const Item& it) const;
    int statBonus(const std::string& stat) const;               // sum of an affix stat over equipped gear
    int maxHp() const;
    int weaponDamage() const;
    int weaponReach() const;
    int weaponCooldown() const;
    int shieldDef() const;
    double speedMul() const;
    bool hasBuff(const std::string& id) const;
    bool canCraft(const RecipeDef& r) const;
    std::string recipeBlocked(const RecipeDef& r) const;
    std::vector<const RecipeDef*> recipes(const std::string& cat) const;
    int craft(const RecipeDef& r, int times);
    bool nearFire() const;
    std::vector<ShopRow> shopRows(bool buying) const;
    int shopTrade(bool buying, int row, int times);

    // ── loot (loot.cpp) ──
    Item makeGear(const std::string& id, int minRarity = 0, double luck = 1);
    Item randomGear(int level, int minRarity = 0);
    void dropItem(Map& m, Item item, double x, double y);

    // ── quests (quests.cpp) ──
    bool questAvailable(const QuestDef& q) const;
    bool questReady(const QuestDef& q) const;
    int objectiveProgress(const QuestDef& q, size_t i) const;
    int npcMarker(const std::string& npcId) const;   // 0 none, 1 new quest, 2 quest in progress, 3 ready to hand in
    std::vector<const QuestDef*> activeQuests() const;
    void questEvent(const std::string& type, const std::string& target, const std::string& alt = "");
    void talkTo(Object& npc);

    // ── levels ──
    void gainXp(int amount);
    int xpToNext() const { return db.xpForLevel(P.level); }
    bool whirlwindUnlocked() const { return P.level >= db.tuning.whirlLevel; }

    // ── world queries ──
    double hourOfDay() const;
    double darkness() const;           // 0 = full day, 1 = deep night
    bool isNight() const { return darkness() > 0.5; }
    std::string clockText() const;
    bool invulnerable() const { return P.iframes > 0 || P.roll > 2; }
    const Enemy* activeBoss() const;
    int combatLevel() const;           // 0 calm, 1 fighting, 2 fighting an elite, 3 boss
    const Enemy* meleeTarget() const;
    const Enemy* bowTarget() const;
    std::array<double, 4> attackBox() const;
    void travelTo(const std::string& townId);
    void respawn();

    // exposed for tests
    void enterMap(const std::string& id, double x, double y);
    Enemy makeEnemy(const EnemyDef& def, double x, double y, bool allowElite = true);
    void damageEnemy(Enemy& e, int dmg, double kx, double ky);
    void hurtPlayer(int dmg, double fromX, double fromY);
    void spawnEnemies(Map& m);
    void updateSpawns(Map& m);

  private:
    SimInput in_;
    std::vector<std::pair<std::string, TilePos>> brokenPots_;   // map id + tile of every pot smashed so far
    int attackBuf_ = 0, shootBuf_ = 0, nextUid_ = 1, nextEnemyUid_ = 1;
    bool attackMouse_ = false, shootMouse_ = false, wantDodge_ = false, wantInteract_ = false, wantHeal_ = false, wantSkill_ = false, wantMount_ = false,
         wantRecall_ = false;

    void buildWorld();
    void applyWorldFlags();
    void updateCamera(bool snap);
    void updateFx();
    void updatePlayer();
    void updateSkills(Map& m);
    void updateEnemies(Map& m);
    void updateBoss(Enemy& e, Map& m, double dx, double dy, double d);
    void fireRing(Map& m, const Enemy& e, int n, double off);
    void updateProjectiles(Map& m);
    void updateDrops(Map& m);
    void updateTraps(Map& m);
    void updateWater(Map& m);
    void updateNodes(Map& m);
    void updateNight(Map& m);
    void updateFocus(Map& m);
    void updatePlace(Map& m);
    void explore(Map& m);
    void interact(Map& m);
    void openChest(Map& m, Object& o);
    void chopTree(Map& m, int c, int r);
    void fellTree(Map& m, int c, int r);
    void harvest(Map& m, Object& o);
    void breakPot(Map& m, Object& o);
    void useShrine(Object& o);
    void openDoor(Map& m, bool silent);
    void killEnemy(Map& m, Enemy& e);
    void waterFx(Map& m, double x, double y, bool splash);
    bool tileOccupied(const Map& m, int c, int r) const;
    void die();
    void dismount(bool quiet = false);
    void sleepUntilMorning();
    void completeQuest(const QuestDef& q);
    void autoEquip(const Item& it);
    Item* findUid(int uid);
    const Item* findUid(int uid) const;
    static EquipSlot slotFor(ItemType t);
};

}   // namespace hc

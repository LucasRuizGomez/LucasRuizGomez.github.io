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

enum class Mode { Title, Play, Pause, Inv, Craft, Shop, Sign, Quests, WorldMap, Travel, Dead, Victory, Skills, Talk };
enum class EquipSlot { Weapon, Shield, Armor, Trinket, Helm, Boots };
constexpr int EQUIP_SLOTS = 6;

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
    int hp = 10, level = 1, xp = 0, heartCrystals = 0, heartShards = 0;
    std::vector<Item> inv;          // everything carried, worn gear included (worn gear does not take a bag slot)
    int equip[EQUIP_SLOTS] = {};    // item uid per EquipSlot, 0 = empty
    std::map<std::string, int> skills;   // skill id -> rank learned
    std::string mountPick;          // mount chosen in the bag's mount slot (empty = the fastest one)
    int regenT = 0;
    int atkT = 0, atkCd = 0, bowCd = 0, bowT = 0, iframes = 0, actCd = 0, roll = 0, rollCd = 0;
    double swingA = 0;              // the direction of the current sword swing (radians)
    int swingSide = 1;              // which way the blade sweeps: alternates, swing after swing
    double kbx = 0, kby = 0, rvx = 0, rvy = 0;
    std::vector<int> hitSet;
    std::vector<Trail> trail;
    std::string mount;          // item id of the mount being ridden, empty on foot
    int mountCast = 0, recallT = 0, whirlCd = 0, whirlT = 0;
    std::map<std::string, int> abilityCd;   // active skills learned in the tree: ticks until each is ready again
    int leapT = 0;                          // Leap Slam: ticks left in the air
    double leapX = 0, leapY = 0, leapTX = 0, leapTY = 0;
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
    void openZoneGates(bool announce);   // lifts every region gate whose boss is dead
    void gateHint(Map& m);
    int gateHintAt_ = 0;
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
    int bagUsed() const;                                          // bag slots taken (worn gear and the chosen mount are not in the bag)
    bool bagFull() const { return bagUsed() >= db.tuning.invSlots; }
    bool inBag(const Item& it) const;                             // false for worn gear and the chosen mount
    std::vector<int> bagView() const;                             // indices into P.inv of what the bag grid shows, in order
    static bool slotAccepts(EquipSlot s, ItemType t);
    static EquipSlot slotFor(ItemType t);
    bool equipTo(int invIndex, EquipSlot s);                      // wear a piece in a slot (swaps out what was there)
    bool unequip(EquipSlot s);                                    // back to the bag, if there is room
    void moveItem(int from, int to);                              // reorder the bag: swap two entries of P.inv
    bool setMountPick(const std::string& id);                     // "" clears the choice
    std::string chosenMount() const;                              // the mount the mount key summons, or ""
    std::string discardBlocked(const Item& it) const;             // why an item cannot be thrown away, or ""
    bool discardItem(int invIndex, int qty);                      // throw on the ground in front of the hero
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
    int bowDamage() const;
    double speedMul() const;
    int gearStat(int ItemDef::*field) const;                      // sum of a base stat over worn gear
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
    // The conversation window: what this person has for you
    std::vector<const QuestDef*> questsOffered(const std::string& npcId) const;    // new, can be accepted
    std::vector<const QuestDef*> questsToHandIn(const std::string& npcId) const;   // done, reward waiting
    std::vector<const QuestDef*> questsUnderway(const std::string& npcId) const;   // accepted, not finished yet
    bool acceptQuest(const std::string& id);
    bool turnInQuest(const std::string& id, int choice);                       // choice: index into QuestDef::choice
    bool abandonQuest(const std::string& id);
    void openShop(const std::string& npcId);
    std::string talkNpc, talkLine;                    // who the hero is talking to, and what they just said
    std::string questGiverTown(const QuestDef& q) const;

    // ── levels and the skill tree ──
    void gainXp(int amount);
    int xpToNext() const { return db.xpForLevel(P.level); }
    bool whirlwindUnlocked() const { return skillStat("whirl") > 0; }
    // The active skills besides Whirlwind: unlocked by a skill-tree effect of the same name.
    struct Ability {
        const char *id, *name, *icon;
        int cooldown;   // ticks
    };
    static const std::vector<Ability>& abilities();
    bool abilityKnown(const std::string& id) const { return skillStat(id) > 0; }
    int abilityCooldown(const std::string& id) const;   // ticks left
    void useAbility(const std::string& id);
    int whirlCooldown() const;
    int skillRank(const std::string& id) const;
    double skillStat(const std::string& stat) const;              // sum of a stat over learned ranks
    int skillPoints() const;                                      // points left to spend
    std::string skillBlocked(const SkillDef& s) const;            // why the next rank cannot be learned now, or ""
    bool learnSkill(const std::string& id);

    // ── mounts and resting ──
    bool flying() const;                                          // riding a flying mount
    std::string restBlocked() const;                              // why the hero cannot rest at a fire now, or ""

    // ── world queries ──
    double hourOfDay() const;
    double darkness() const;           // 0 = full day, 1 = deep night
    bool isNight() const { return darkness() > 0.5; }
    std::string clockText() const;
    bool invulnerable() const { return P.iframes > 0 || P.roll > 2 || P.leapT > 0 || flying(); }
    const Enemy* activeBoss() const;
    int combatLevel() const;           // 0 calm, 1 fighting, 2 fighting an elite, 3 boss
    const Enemy* meleeTarget() const;
    const Enemy* bowTarget() const;
    std::array<double, 4> attackBox() const;
    // A sword swing sweeps from -SWING_SPAN to +SWING_SPAN radians around P.swingA (times P.swingSide).
    // swingOffset: where the blade is at frame f of the swing (0 = it starts): wind-up, a fast slash, follow-through.
    static constexpr double SWING_SPAN = 1.2;
    static double swingOffset(double f);
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
    bool land(bool quiet);                                        // leave a flying mount on the nearest free ground
    void addHeartShard();
    void completeQuest(const QuestDef& q, int choice = -1);
    std::map<std::string, int> chats_;   // how often each person has been spoken to (they cycle through their lines)
    void autoEquip(const Item& it);
    Item* findUid(int uid);
    const Item* findUid(int uid) const;
};

}   // namespace hc

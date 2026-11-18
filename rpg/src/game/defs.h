// Static game data loaded from config/*.json: items, enemies, zones, towns, dungeons, quests, tuning.
// Nothing here changes while the game runs.
#pragma once
#include "core/files.h"
#include "core/types.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace hc {

enum class ItemType { Mat, Heal, Weapon, Shield, Armor, Trinket, Bow, Ammo, Tool, Light, Key, Special, Trophy, Mount, Helm, Boots };

struct ItemDef {
    std::string id, name, desc;
    ItemType type = ItemType::Mat;
    int heal = 0;                       // heal items: half hearts restored
    int dmg = 0, reach = 0, cd = 0;     // weapons
    int def = 0;                        // shields: damage blocked from strong hits
    int hp = 0;                         // armor, helms: extra half hearts
    int speed = 0;                      // boots: percent of movement speed
    double tier = 0;
    int level = 1, value = 0;
    bool unique = false;
    std::string mountSprite, mountTint;
    std::string wear;                   // worn gear: the colour it gives the hero's sprite (hex)
    double mountSpeed = 1;
    bool mountFly = false;              // a flying mount crosses water, forests and rocks
    bool isGear() const {
        return type == ItemType::Weapon || type == ItemType::Shield || type == ItemType::Armor || type == ItemType::Trinket || type == ItemType::Helm || type == ItemType::Boots;
    }
    bool stacks() const { return !isGear(); }
};

struct RarityDef {
    std::string id, name;
    Color color, glow;
    int affixes = 0;
    double weight = 1, valueMul = 1;
};

struct AffixDef {
    std::string id, label, stat, text;
    int min = 1, max = 1;
    std::vector<ItemType> slots;
};

struct RecipeDef {
    std::string out;
    int outQty = 1;
    std::vector<std::pair<std::string, int>> in;
    std::string cat;
    bool fire = false;
};

struct CategoryDef {
    std::string id, label;
};

enum class Ai { Charger, Chaser, Flee, Flier, Guard, Archer, Boss };
enum class Look { Animal, Human, Treant, Mimic };

struct DropDef {
    std::string item;
    double chance = 1;
    int qty = 1;
};

struct EnemyDef {
    std::string id, name, sprite, tag, tint;
    Look look = Look::Animal;
    Ai ai = Ai::Chaser;
    double scale = 1;
    int level = 1, hp = 1, dmg = 1, xp = 1;
    double speed = 0.5, aggro = 120, stalk = 0;
    double r = 6, cy = 6, hw = 5, hh = 3;
    bool fly = false, passive = false;
    std::vector<DropDef> drops;
    int goldMin = 0, goldMax = 0;
    double goldChance = 0;
    struct Charge {
        double range = 110, speed = 1.9;
        int every = 70, wind = 30;
    } charge;
    struct Boss {
        double speed = 0.55, slamRadius = 42;
        int slam = 4, volley = 12;
        std::string summon, roomTitle;
        bool final = false;
    } boss;
    bool isBoss() const { return ai == Ai::Boss; }
};

struct EliteDef {
    std::string id, label;
    Color glow;
    double speedMul = 1;
    int dmgAdd = 0, healOnHit = 0, armor = 0;
};

struct ZoneDef {
    std::string id, name, tree, music;
    int index = 0, levelMin = 1, levelMax = 1;
    std::vector<TilePos> centers;
    double water = 0.25, trees = 0.6, rocks = 0.9, flowers = 0.04, density = 10;
    bool sandy = false, snow = false;
    Color ground, foliage;
    std::vector<std::pair<std::string, double>> spawns;
    std::vector<std::pair<std::string, double>> nodes;
};

struct PropDef {
    std::string id, sprite;
    int w = 1, h = 1, frames = 1;
    int ox = 0;   // horizontal shift in pixels when drawn, for sprites whose foot is not in the middle (a flag pole)
    int oy = 0;   // vertical shift in pixels when drawn (negative = higher), for things that sit on a table
    bool solid = true, flat = false, arch = false;
};

struct NpcDef {
    std::string id, name, sprite, tint, role, shop, text, town;
    std::string building;              // id of the building they live or work in, empty = out in the street
    std::vector<std::string> lines;    // small talk, one per conversation in turn
    TilePos at{-1, -1};                // indoors: where they stand
};

// Furniture and fittings of a building's inside: [prop, column, row] on the interior map.
struct PropPlace {
    std::string prop;
    int c = 0, r = 0;
};

// A kind of building (inn, bakery, home...): the size of its room and how it is furnished.
struct InteriorDef {
    std::string id, label;
    int w = 14, h = 10;
    std::string floor = "wood", wall = "wood";   // floor: wood | tile | dark | stone   wall: wood | brick | yellow | stone
    std::vector<PropPlace> props;
    TilePos hearth{-1, -1};            // a fireplace or oven to rest and cook at
    std::string hearthSprite = "oven";
};

// One enterable building of a town. Its own extra furniture makes it different from others of its kind.
struct BuildingDef {
    std::string id, town, kind, name, sign;
    std::string floor, wall;           // empty = the kind's
    std::vector<PropPlace> props;      // added to the kind's
    std::vector<std::string> remove;   // props of the kind this building does without ("prop@c,r" or "prop")
    std::vector<std::string> npcs;     // ids of the people inside
};

struct TownDef {
    std::string id, name, zone, music;
    TilePos at;
    int radius = 10, houses = 2, stalls = 0;
    std::vector<std::string> features;
    std::vector<NpcDef> npcs;
    std::vector<std::string> buildings;   // building ids, in the order of the plots around the square
    bool has(const std::string& f) const { return std::find(features.begin(), features.end(), f) != features.end(); }
};

struct DungeonDef {
    std::string id, name, kind, entrance, under, music, boss, tint;
    TilePos at;
    int level = 1, w = 60, h = 46, rooms = 8, traps = 0;
    uint32_t seed = 1;
    std::vector<std::string> enemies;
    std::vector<std::pair<std::string, int>> reward;
};

struct ShopDef {
    struct Entry {
        std::string item;
        int qty = 1, price = 1;
    };
    std::string id, title;
    std::vector<Entry> buy;
    std::map<std::string, int> sell;
};

struct QuestObjective {
    std::string type, item, target, label;
    int count = 1;
    bool anyWeapon = false;
};

struct QuestDef {
    std::string id, title, giver, turnIn, offer, progress, done;
    bool main = false;
    std::vector<std::string> requires_;
    std::vector<QuestObjective> objectives;
    int xp = 0, gold = 0, rewardRarity = 0, level = 1;
    std::vector<std::pair<std::string, int>> items;
    std::vector<std::pair<std::string, int>> choice;   // the hero picks one of these on handing in
    std::string objectiveText;                          // the short "what to do" line of the quest window
};

// One node of the skill tree. Every rank adds `effects` (stat -> amount) to the hero.
struct SkillDef {
    std::string id, name, branch, desc;
    int maxRank = 1, level = 1;      // level: hero level needed to learn the first rank
    double col = 1, row = 0;         // position in its branch: columns 0..2, rows from the top
    std::string requires_;           // skill id that must be learned first
    int requiresRank = 1;
    std::string icon;                // sprite name of its icon
    std::vector<std::pair<std::string, double>> effects;
};

struct BranchDef {
    std::string id, label, desc;
    Color color;
};

struct ActionDef {
    std::string id, label;
    std::vector<std::string> keys;
};

struct Tuning {
    int saveSlots = 4;
    int worldW = 256, worldH = 192;
    uint32_t worldSeed = 1337;
    double playerSpeed = 1.3;
    int rollTicks = 16, rollCooldown = 40, attackTicks = 14, inputBuffer = 10, baseHp = 10, invSlots = 30, hitInvuln = 60;
    int recallTicks = 210, mountCastTicks = 45, combatLockTicks = 240;
    int fistDmg = 1, fistReach = 9, fistCd = 18;
    int maxLevel = 12, heartEvery = 0, damageEvery = 3, discoverZoneXp = 25;   // heartEvery 0 = levels give no hearts
    int skillPointsPerLevel = 1, heartShards = 3;
    double xpBase = 40, xpGrowth = 1.5;
    int whirlLevel = 3, whirlCooldown = 360;
    double whirlRadius = 30, whirlMul = 1.5;
    int dayLength = 28800;
    double startHour = 8;
    int spawnActive = 30, spawnDespawn = 46, respawnTicks = 7200;
    double eliteChance = 0.07, globeChance = 0.12;
    int shrineBuff = 3600, shrineRecharge = 18000;
    int treeChops = 3, stumpTicks = 3000, saplingTicks = 2400;
    double goldLost = 0.25;
    double restEnemyRadius = 160;   // no resting or sleeping at a fire with a hostile monster this close
    int autosaveTicks = 1800;
};

constexpr int TILE = 16;

class Database {
  public:
    // Reads every config file from `dir` (must end with a slash). Throws std::runtime_error on bad data.
    static Database load(const std::string& dir);

    const ItemDef& item(const std::string& id) const;
    const ItemDef* findItem(const std::string& id) const;
    const EnemyDef& enemy(const std::string& id) const;
    const ZoneDef* zone(const std::string& id) const;
    const TownDef* town(const std::string& id) const;
    const DungeonDef* dungeon(const std::string& id) const;
    const QuestDef* quest(const std::string& id) const;
    const ShopDef* shop(const std::string& id) const;
    const NpcDef* npc(const std::string& id) const;
    const PropDef* prop(const std::string& id) const;
    const AffixDef* affix(const std::string& id) const;
    const SkillDef* skill(const std::string& id) const;
    const BuildingDef* building(const std::string& id) const;
    const InteriorDef* interior(const std::string& kind) const;
    int xpForLevel(int level) const;   // xp needed to go from `level` to the next

    Tuning tuning;
    std::vector<ItemDef> items;
    std::vector<RarityDef> rarities;
    std::vector<AffixDef> affixes;
    std::vector<RecipeDef> recipes;
    std::vector<CategoryDef> categories;
    std::vector<EnemyDef> enemies;
    std::vector<EliteDef> elites;
    std::vector<ZoneDef> zones;
    std::vector<TownDef> towns;
    std::vector<DungeonDef> dungeons;
    std::vector<QuestDef> quests;
    std::vector<ActionDef> actions;
    std::vector<BranchDef> branches;
    std::vector<BuildingDef> buildings;
    std::map<std::string, InteriorDef> interiors;
    std::vector<SkillDef> skills;
    std::map<std::string, ShopDef> shops;
    std::map<std::string, PropDef> props;
    std::map<std::string, int> pois;
    std::map<std::string, std::string> gamepad;
    double defaultSfx = 0.8, defaultMusic = 0.5, defaultUi = 1;
    json audio;     // raw audio.json, read by the app's audio layer
    json sprites;   // raw sprites.json, read by the view

  private:
    std::map<std::string, size_t> itemIndex_, enemyIndex_, questIndex_;
    std::map<std::string, NpcDef> npcs_;
};

}   // namespace hc

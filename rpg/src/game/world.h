// Runtime world data: tiles, maps and everything that lives on them. Plain structs, no behaviour
// beyond small helpers; the rules that move them are in sim*.cpp and worldgen.cpp.
#pragma once
#include "core/rng.h"
#include "core/types.h"
#include "game/defs.h"

#include <array>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace hc {

enum Tile : uint8_t { GRASS, WATER, TREE, PATH, SAND, ROCK, FLOWERS, BRIDGE, FLOOR, WALL, DOOR, STAIRS_DOWN, STAIRS_UP, PLANKS, PLAZA, COBBLE, TILE_COUNT };

inline bool walkable(uint8_t t) {
    static const bool w[TILE_COUNT] = {true, false, false, true, true, false, true, true, true, false, false, true, true, true, true, true};
    return t < TILE_COUNT && w[t];
}
inline bool blocksFlight(uint8_t t) { return t == TREE || t == ROCK || t == WALL || t == DOOR; }
inline bool isGround(uint8_t t) { return t == GRASS || t == FLOWERS || t == SAND; }

enum class Dir : uint8_t { Up, Right, Down, Left };
inline Vec2 dirVec(Dir d) {
    static const Vec2 v[4] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    return v[static_cast<int>(d)];
}
inline Dir snap4(double x, double y) { return std::abs(x) > std::abs(y) ? (x < 0 ? Dir::Left : Dir::Right) : (y < 0 ? Dir::Up : Dir::Down); }

// One inventory entry. Stackable things use qty; gear is always qty 1 with its own uid, rarity and affixes.
struct Item {
    std::string id;
    int qty = 1;
    int rarity = 0;
    int uid = 0;
    std::vector<std::pair<std::string, int>> affixes;   // affix id, rolled value
};

enum class ObjKind : uint8_t { Chest, Node, Campfire, Npc, Prop, WallTorch, Gate, Entrance, Trap, Stump, Pot, Shrine, Waypoint, Sign };

struct Object {
    ObjKind kind = ObjKind::Prop;
    int c = 0, r = 0;
    bool solid = false;
    bool gone = false;
    std::string id;       // chest id, npc id, dungeon id (entrance), town id (waypoint), prop id
    std::string sprite;   // prop / entrance sprite name
    std::string text;     // signs
    int w = 1, h = 1;     // blocked footprint, bottom row = r, centred on c
    // chest
    bool opened = false, mimic = false;
    int openAt = -1, lootLevel = 1;
    std::vector<Item> loot;
    // node
    std::string res;
    int left = 0, max = 0, respawn = 0, hitAt = -999, grewAt = -999;
    // stump: stage 0 = stump, 1 = sapling; `at` = tick when it grows on
    int stage = 0, at = 0;
    // trap / shrine timers
    int t = 0, charge = 0;
    // npc
    int face = 1;
};

struct Enemy {
    const EnemyDef* def = nullptr;
    const EliteDef* elite = nullptr;
    int uid = 0;
    double x = 0, y = 0, vx = 0, vy = 0, kbx = 0, kby = 0;
    int hp = 1, maxHp = 1, dmg = 1;
    double r = 6, cy = 6, hw = 5, hh = 3;
    int flash = 0, t = 0, hop = 0, st = 0, act = 0, wind = 0, aim = 0, summons = 0, wave = 0;
    double ax = 0, ay = 0, slamX = 0, slamY = 0;
    bool alive = true, moving = false, fast = false, active = false, summoned = false, night = false, sank = false;
    int face = 1;
    Dir dir = Dir::Down;
    enum class State : uint8_t { Idle, Roar, Chase, SlamWind, VolleyWind, Summon, Recover } state = State::Idle;
    int spawn = -1;       // index into Map::spawns, or -1
    int aggroT = 0;       // ticks since it last chased or hit the player (for combat music)
};

struct Projectile {
    double x = 0, y = 0, vx = 0, vy = 0;
    bool fromPlayer = false, hitSomething = false;
    int dmg = 1, life = 60;
    enum class Kind : uint8_t { Arrow, Bolt, Knife } kind = Kind::Arrow;
};

struct Drop {
    Item item;
    double x = 0, y = 0, vx = 0, vy = 0, z = 0, vz = 0;
    int t = 0;
    bool gone = false, globe = false;
};

struct Corpse {
    Enemy e;
    int t = 0;
};

struct WaterFx {
    bool splash = false;
    double x = 0, y = 0;
    int t = 0;
};

struct FallingTree {
    int c = 0, r = 0, t = 0, dir = 1;
};

struct SpawnPoint {
    int c = 0, r = 0;
    const EnemyDef* def = nullptr;
    int respawnAt = 0;
    int enemyUid = 0;      // 0 = nothing alive from this point
    bool forceElite = false;
};

struct Link {
    int c = 0, r = 0;
    std::string toMap;
    int toC = 0, toR = 0;
    Dir face = Dir::Down;
};

struct Room {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(double px, double py) const { return px >= x * TILE && px < (x + w) * TILE && py >= y * TILE && py < (y + h) * TILE; }
    TilePos center() const { return {x + w / 2, y + h / 2}; }
};

struct TreeHit {
    int hits = 0, at = 0;
};

struct Map {
    std::string id, name;
    int w = 0, h = 0;
    bool dark = false, outdoor = false;
    const DungeonDef* dungeon = nullptr;
    std::vector<uint8_t> t, block, explored, zone, reach;
    std::vector<Object> objects;
    std::vector<Enemy> enemies;
    std::vector<Drop> drops;
    std::vector<Projectile> proj;
    std::vector<Corpse> corpses;
    std::vector<WaterFx> wfx;
    std::vector<FallingTree> falling;
    std::vector<SpawnPoint> spawns;
    std::vector<Link> links;
    std::vector<Room> rooms;
    Room bossRoom;
    TilePos start;
    bool hasWhirl = false;
    TilePos whirl;
    std::unordered_map<int, TreeHit> treeHit;
    // Tiles changed since the view last looked (it re-bakes them and rebuilds props).
    std::vector<TilePos> dirty;
    bool exploredDirty = true;

    int idx(int c, int r) const { return r * w + c; }
    bool inside(int c, int r) const { return c >= 0 && r >= 0 && c < w && r < h; }
    uint8_t get(int c, int r) const { return inside(c, r) ? t[idx(c, r)] : (outdoor ? TREE : WALL); }
    void set(int c, int r, uint8_t v) {
        if (inside(c, r)) t[idx(c, r)] = v;
    }
    void change(int c, int r, uint8_t v) {   // set + tell the view
        if (inside(c, r)) {
            t[idx(c, r)] = v;
            dirty.push_back({c, r});
        }
    }
    bool tileFree(int c, int r, bool fly) const;
    bool boxFree(double x, double y, double hw, double hh, bool fly) const;
    // Moves x/y by dx/dy on each axis separately if the box stays free. Returns {hitX, hitY}.
    std::pair<bool, bool> moveBox(double& x, double& y, double dx, double dy, double hw, double hh, bool fly) const;
    void addObject(Object o);
    void setBlock(const Object& o, bool on);
    Object* objectAt(int c, int r);
    const Object* objectAt(int c, int r) const;
};

Map makeMap(const std::string& id, int w, int h, uint8_t fill);

// A* over tiles (4 directions). cost() returns the price of stepping onto a tile, or a negative number if impassable.
std::vector<TilePos> astar(const Map& m, TilePos from, TilePos to, const std::function<double(int, int)>& cost);
// Flood fill of walkable tiles from a start tile (1 = reachable).
std::vector<uint8_t> reachMask(const Map& m, int c, int r);
// Carves a way to (c, r) through trees, rocks and water if it is not already next to a reachable tile.
void ensureReach(Map& m, TilePos from, int c, int r, std::vector<uint8_t>* mask = nullptr);

// World generation (worldgen.cpp)
struct WorldGenResult {
    std::map<std::string, Map> maps;
};
Map generateOverworld(const Database& db);
Map generateDungeon(const Database& db, const DungeonDef& def, const Map& overworld);
// Zone that a tile of the overworld belongs to.
const ZoneDef& zoneAt(const Database& db, const Map& over, int c, int r);
// Town whose grounds contain the tile, or null.
const TownDef* townAt(const Database& db, int c, int r, int margin = 0);

}   // namespace hc

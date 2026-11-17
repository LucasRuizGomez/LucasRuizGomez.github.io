// Draws the world: terrain baked into chunk textures, y-sorted props and actors, effects, lighting.
#pragma once
#include "engine/font.h"
#include "engine/gfx.h"
#include "game/sim.h"
#include "view/sprites.h"

#include <map>
#include <vector>

namespace hc {

class WorldView {
  public:
    WorldView(Gfx& g, Sprites& spr, const Database& db) : g_(g), spr_(spr), db_(db) {}

    // Draws the current map with the camera in sim.cam. zoom = device pixels per art pixel.
    void render(Sim& sim, int zoom, bool withActors);
    // Device-pixel offset of the world's origin, valid after render() (used to map the mouse into the world).
    double offX = 0, offY = 0;
    bool showTargetMark = false;   // mark the crossbow's auto-aim target (keyboard and gamepad play)

    // One pixel per tile, with unexplored ground hidden. Refreshed when the map changes.
    const Sprite& minimap(Sim& sim, Map& m, bool fog = true);
    void forget();   // drop every cached texture (new world)

  private:
    struct Prop {
        int c = 0;
        const Sprite* img = nullptr;
        double x = 0, y = 0, base = 0;
        bool tree = false;
    };
    struct MapView {
        int cw = 0, ch = 0;
        std::vector<Sprite> chunks;
        std::vector<uint8_t> baked;
        std::vector<std::vector<Prop>> props;
        Sprite mini, miniFull;
        bool fullDirty = true;
        std::vector<uint8_t> miniPx;
        std::map<int, const Sprite*> fallingImg;
        std::vector<const TileSet*> zoneTiles;
        std::vector<const TreeSet*> zoneTrees;
    };
    static constexpr int CHUNK = 32;   // tiles per chunk side

    MapView& view(Sim& sim, Map& m);
    void sync(Sim& sim, Map& m, MapView& v);
    void bakeChunk(Map& m, MapView& v, int cx, int cy);
    void drawTile(Map& m, MapView& v, int c, int r);
    bool propFor(Map& m, MapView& v, int c, int r, Prop& out);
    void refreshProps(Map& m, MapView& v, int c, int r);
    const TileSet& tilesAt(Map& m, MapView& v, int c, int r);
    void drawObject(Sim& sim, Map& m, const Object& o);
    void drawPlayer(Sim& sim);
    void drawEnemy(Sim& sim, const Enemy& e);
    void drawCorpse(const Corpse& c);
    const Sprite& enemyFrame(Sim& sim, const Enemy& e, bool& flip);
    void drawLighting(Sim& sim, Map& m, int zoom, double alpha, Color col);
    void pixelArc(double cx, double cy, double R, double a, double b, Color col);

    Gfx& g_;
    Sprites& spr_;
    const Database& db_;
    std::map<std::string, MapView> views_;
    Sprite light_;
    std::vector<uint8_t> lightPx_;
    std::vector<float> lightA_;
};

std::string toHex(Color c);

}   // namespace hc

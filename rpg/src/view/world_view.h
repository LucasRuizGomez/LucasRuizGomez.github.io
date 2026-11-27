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
    // Debug (gaitsheet=id,id...): each monster's stride as eight poses in a row, then standing, on a plain ground
    void drawGaitSheet(Sim& sim, const std::vector<std::string>& ids, int zoom);
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
        int sway = 0;   // pixels its top leans in the wind
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
        std::vector<uint8_t> masonry;   // tiles under a region gate's walls: no boulders or trees drawn there
    };
    static constexpr int CHUNK = 32;   // tiles per chunk side

    MapView& view(Sim& sim, Map& m);
    void sync(Sim& sim, Map& m, MapView& v);
    void bakeChunk(Map& m, MapView& v, int cx, int cy);
    void drawInteriorTile(Map& m, const TileSet& T, int c, int r);
    void drawTile(Map& m, MapView& v, int c, int r);
    void drawCliff(Map& m, const TileSet& T, int c, int r);
    void drawElevation(Map& m, int c, int r);
    void drawDetail(Map& m, int c, int r);
    bool propFor(Map& m, MapView& v, int c, int r, Prop& out);
    bool decorFor(Map& m, int c, int r, Prop& out);
    void drawTravel(Sim& sim, const Object& o);
  public:
    // what the hero is wearing, as knight layers (assets/lpc), in drawing order
    std::vector<std::string> heroLayers(const Sim& sim) const;
  private:
    void refreshProps(Map& m, MapView& v, int c, int r);
    const TileSet& tilesAt(Map& m, MapView& v, int c, int r);
    void drawObject(Sim& sim, Map& m, const Object& o);
    void drawRegionGate(const Map& m, const Object& o);
    void exitArch(const TileSet& T, double x, double y, int half);
    void drawPlayer(Sim& sim);
    void drawEnemy(Sim& sim, const Enemy& e);
    void drawCorpse(const Corpse& c);
    const Sprite& enemyFrame(Sim& sim, const Enemy& e, bool& flip);
    void drawLighting(Sim& sim, Map& m, int zoom, double alpha, Color col);
    void pixelArc(double cx, double cy, double R, double a, double b, Color col);
    // Draws a sprite sheared by `lean` pixels at the top and none at the base, in whole-pixel strips.
    void drawSway(const Sprite& img, double x, double y, double lean);
    // A still picture given a body: `lean` shears the top (pixels), `foot` slides the bottom rows (a step),
    // `squash` < 0 flattens and > 0 stretches it, always standing on the bottom of the box (x, y, w, h).
    // With a rig, the two halves of its lower part step on their own (stepA/B: forward, liftA/B: up, in pixels)
    // and its sides beat as wings (flap: how far the wing tips rise, negative = down).
    struct Warp {
        double lean = 0, foot = 0, squash = 0;
        double stepA = 0, stepB = 0, liftA = 0, liftB = 0, flap = 0;
        double bob = 0;   // the whole figure up (-) or down, art pixels: highest as the legs pass each other
    };
    // Where each still-picture monster is in its stride. Kept by the view, driven by the ground it covers.
    struct GaitState {
        double x = 0, y = 0, phase = 0, walk = 0;
        bool seen = false;
    };
    std::map<int, GaitState> gaits_;
    void drawWarp(const Sprite& img, double x, double y, double w, double h, bool flip, const Warp& wp, Color tint = Color(255, 255, 255), bool additive = false,
                  const Sprites::Rig* rig = nullptr);
    Warp gait(const Enemy& e, int tick, const Sprites::Rig* rig, const GaitState& g) const;

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

// Every sprite the game draws, cut from the asset packs at start-up. Tinted variants (zone ground,
// zone foliage, recoloured monsters and townsfolk) are made on first use and cached.
#pragma once
#include "engine/gfx.h"
#include "game/defs.h"

#include <array>
#include <map>
#include <string>
#include <vector>

namespace hc {

using Frames = std::vector<Sprite>;

struct CharSet {   // fortress-kit character, frames face right
    std::map<std::string, Frames> anims;
    const Frames& get(const std::string& name) const;
};

struct AnimalSet {   // hunt-pack animal: rows are down, up, left, right
    std::map<std::string, std::array<Frames, 4>> anims;
    const std::array<Frames, 4>& get(const std::string& name) const;
};

struct TileSet {
    Sprite sheet;   // the whole tinted tileset, for the grass autotile
    Sprite dirt[4], slab[9], brickHi[4], brickLo[4], wallTop[4];
    Sprite stairsUp, stairsDown, slit, deck, deckH, bridgeV, bridgeH;
};

struct TreeSet {
    Sprite oak, oakM, pine, pineS, small;
};

class Sprites {
  public:
    void build(Gfx& g, const Database& db, const std::string& root);

    const Sprite& get(const std::string& name) const;        // empty sprite if unknown
    const Frames& frames(const std::string& name) const;     // animation strips from sprites.json
    const Sprite& icon(const std::string& id) const;         // item icons and a few banner icons
    const CharSet& character(const std::string& sprite, const std::string& tint);
    // The hero, dressed in what is worn: colours (hex) for the helm, body armour, boots and shield; empty = none.
    // The kit's painted sword is taken out, so the real weapon can be drawn in the hand.
    const CharSet& hero(const std::string& helm, const std::string& body, const std::string& feet, const std::string& shield);
    const AnimalSet& animal(const std::string& sprite, const std::string& tint);
    const std::array<Frames, 4>& treant(const std::string& tint);   // down, right, up, left
    const TileSet& tiles(const std::string& tint);
    const TreeSet& trees(const std::string& tint);

    Frames chest, campMeat, torch, gate, knife, wRipple, wSplash, wWhirl, waterRocks, water, waves, flowers, decals;
    Sprite bolt8[8], heart[3], alert, glow, plaza, cobble;
    std::map<std::string, Frames> boulders, bigBoulders, nodes;
    std::map<std::string, std::array<Sprite, 4>> weaponRot;   // up, right, down, left
    struct {
        Sprite panel, slot, slotDark, btn, btnSel, close;
        Sprite orbBack, orbFrame, orbGlass, orbRed, orbViolet;   // the health and experience globes (48 art px)
    } ui;

  private:
    Gfx* g_ = nullptr;
    std::map<std::string, Sprite> named_;
    std::map<std::string, Frames> strips_;
    std::map<std::string, Sprite> icons_;
    std::map<std::string, Image> charSheets_;     // "ch4.idle"
    std::map<std::string, Image> animalSheets_;   // "boar.walk"
    Image tileSheet_, treantSheet_;
    std::map<std::string, CharSet> chars_;
    std::map<std::string, AnimalSet> animals_;
    std::map<std::string, std::array<Frames, 4>> treants_;
    std::map<std::string, TileSet> tileSets_;
    std::map<std::string, TreeSet> treeSets_;
    Frames strip(const Image& im, int fw, int fh, int row = 0) const;
};

}   // namespace hc

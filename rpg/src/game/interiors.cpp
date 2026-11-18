// The insides of town buildings: one room with walls on three sides, a door at the bottom,
// furniture from the building's kind (config/buildings.json) plus its own pieces, and the people who live there.
#include "game/world.h"

#include <cmath>

namespace hc {

namespace {

bool removed(const BuildingDef& b, const PropPlace& p) {
    for (const auto& r : b.remove) {
        if (r == p.prop) return true;
        if (r == p.prop + "@" + std::to_string(p.c) + "," + std::to_string(p.r)) return true;
    }
    return false;
}

}   // namespace

Map generateInterior(const Database& db, const BuildingDef& b, TilePos door) {
    const InteriorDef& k = *db.interior(b.kind);
    Map m = makeMap("b_" + b.id, k.w, k.h, WALL);
    m.name = b.name;
    m.subtitle = k.label;
    m.interior = true;
    m.town = b.town;
    m.floorStyle = b.floor.empty() ? k.floor : b.floor;
    m.wallStyle = b.wall.empty() ? k.wall : b.wall;
    // Rows 0..2 are the back wall (its top and a two-tile face), the floor runs to row h-2, row h-1 is the front wall
    for (int r = 3; r < k.h - 1; r++)
        for (int c = 1; c < k.w - 1; c++) m.set(c, r, PLANKS);
    const int exitC = k.w / 2;
    m.set(exitC, k.h - 1, STAIRS_DOWN);
    m.links.push_back({exitC, k.h - 1, "over", door.c, door.r + 1, Dir::Down});
    m.start = {exitC, k.h - 2};

    auto place = [&](const PropPlace& p) {
        const PropDef* pd = db.prop(p.prop);
        if (!pd) return;
        Object o;
        o.kind = ObjKind::Prop;
        o.id = p.prop;
        o.sprite = pd->sprite;
        o.c = p.c;
        o.r = p.r;
        o.w = pd->w;
        o.h = pd->h;
        o.solid = pd->solid && p.r >= 3;   // things hung on the back wall do not need to block anything
        // never wall in the doorway or the spot inside it
        if (o.solid && std::abs(o.c - exitC) <= o.w / 2 && o.r >= k.h - 3) return;
        m.addObject(std::move(o));
    };
    for (const auto& p : k.props)
        if (!removed(b, p)) place(p);
    for (const auto& p : b.props) place(p);
    if (k.hearth.c >= 0) {
        Object fire;
        fire.kind = ObjKind::Campfire;
        fire.c = k.hearth.c;
        fire.r = k.hearth.r;
        fire.w = 2;
        fire.h = 1;
        fire.solid = true;
        fire.id = b.town;
        fire.sprite = k.hearthSprite;
        m.addObject(std::move(fire));
    }
    for (const auto& id : b.npcs) {
        const NpcDef* n = db.npc(id);
        if (!n) continue;
        Object o;
        o.kind = ObjKind::Npc;
        o.id = n->id;
        o.sprite = n->sprite;
        o.c = n->at.c >= 0 ? n->at.c : exitC;
        o.r = n->at.r >= 0 ? n->at.r : 4;
        o.solid = true;
        o.face = o.c > exitC ? -1 : 1;
        m.addObject(std::move(o));
    }
    // Whatever the layout, the way from the door to every free tile must stay open
    m.block[m.idx(exitC, k.h - 2)] = 0;
    m.block[m.idx(exitC, k.h - 1)] = 0;
    m.explored.assign(static_cast<size_t>(m.w) * m.h, 1);
    return m;
}

}   // namespace hc

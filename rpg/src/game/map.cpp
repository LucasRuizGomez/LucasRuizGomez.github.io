#include "game/world.h"

#include <cmath>
#include <limits>
#include <queue>

namespace hc {

Map makeMap(const std::string& id, int w, int h, uint8_t fill) {
    Map m;
    m.id = id;
    m.w = w;
    m.h = h;
    m.t.assign(static_cast<size_t>(w) * h, fill);
    m.block.assign(static_cast<size_t>(w) * h, 0);
    return m;
}

bool Map::tileFree(int c, int r, bool fly) const {
    if (!inside(c, r)) return false;
    const uint8_t v = t[idx(c, r)];
    if (fly ? blocksFlight(v) : !walkable(v)) return false;
    return !block[idx(c, r)];
}

bool Map::boxFree(double x, double y, double hw, double hh, bool fly) const {
    const int x0 = ifloor((x - hw) / TILE), x1 = ifloor((x + hw - 0.01) / TILE);
    const int y0 = ifloor((y - hh) / TILE), y1 = ifloor((y + hh - 0.01) / TILE);
    for (int r = y0; r <= y1; r++)
        for (int c = x0; c <= x1; c++)
            if (!tileFree(c, r, fly)) return false;
    return true;
}

std::pair<bool, bool> Map::moveBox(double& x, double& y, double dx, double dy, double hw, double hh, bool fly) const {
    bool hitX = false, hitY = false;
    if (dx != 0) {
        if (boxFree(x + dx, y, hw, hh, fly)) x += dx;
        else hitX = true;
    }
    if (dy != 0) {
        if (boxFree(x, y + dy, hw, hh, fly)) y += dy;
        else hitY = true;
    }
    return {hitX, hitY};
}

void Map::setBlock(const Object& o, bool on) {
    for (int r = o.r - o.h + 1; r <= o.r; r++)
        for (int c = o.c - o.w / 2; c <= o.c - o.w / 2 + o.w - 1; c++)
            if (inside(c, r)) block[idx(c, r)] = on ? 1 : 0;
}

void Map::addObject(Object o) {
    if (o.solid) setBlock(o, true);
    objects.push_back(std::move(o));
}

Object* Map::objectAt(int c, int r) {
    for (auto& o : objects)
        if (!o.gone && c >= o.c - o.w / 2 && c <= o.c - o.w / 2 + o.w - 1 && r >= o.r - o.h + 1 && r <= o.r) return &o;
    return nullptr;
}
const Object* Map::objectAt(int c, int r) const { return const_cast<Map*>(this)->objectAt(c, r); }

std::vector<TilePos> astar(const Map& m, TilePos from, TilePos to, const std::function<double(int, int)>& cost) {
    const int W = m.w, H = m.h, N = W * H;
    std::vector<float> g(N, std::numeric_limits<float>::infinity());
    std::vector<int> came(N, -1);
    std::vector<uint8_t> closed(N, 0);
    using Node = std::pair<float, int>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    const int si = from.r * W + from.c, gi = to.r * W + to.c;
    g[si] = 0;
    open.push({0, si});
    static const int DC[4] = {1, -1, 0, 0}, DR[4] = {0, 0, 1, -1};
    while (!open.empty()) {
        const int i = open.top().second;
        open.pop();
        if (closed[i]) continue;
        closed[i] = 1;
        if (i == gi) break;
        const int c = i % W, r = i / W;
        for (int k = 0; k < 4; k++) {
            const int nc = c + DC[k], nr = r + DR[k];
            if (nc < 1 || nr < 1 || nc >= W - 1 || nr >= H - 1) continue;
            const int ni = nr * W + nc;
            if (closed[ni]) continue;
            const double cs = cost(nc, nr);
            if (cs < 0) continue;
            const float ng = g[i] + static_cast<float>(cs);
            if (ng < g[ni]) {
                g[ni] = ng;
                came[ni] = i;
                open.push({ng + static_cast<float>(std::abs(nc - to.c) + std::abs(nr - to.r)), ni});
            }
        }
    }
    std::vector<TilePos> path;
    if (came[gi] == -1 && gi != si) return path;
    for (int i = gi; i != -1; i = came[i]) path.push_back({i % W, i / W});
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<uint8_t> reachMask(const Map& m, int sc, int sr) {
    std::vector<uint8_t> seen(static_cast<size_t>(m.w) * m.h, 0);
    if (!m.inside(sc, sr)) return seen;
    std::vector<int> stack{sr * m.w + sc};
    seen[stack[0]] = 1;
    static const int DC[4] = {1, -1, 0, 0}, DR[4] = {0, 0, 1, -1};
    while (!stack.empty()) {
        const int i = stack.back();
        stack.pop_back();
        const int c = i % m.w, r = i / m.w;
        for (int k = 0; k < 4; k++) {
            const int nc = c + DC[k], nr = r + DR[k];
            if (!m.tileFree(nc, nr, false)) continue;
            const int ni = nr * m.w + nc;
            if (!seen[ni]) {
                seen[ni] = 1;
                stack.push_back(ni);
            }
        }
    }
    return seen;
}

static bool touchesMask(const std::vector<uint8_t>& mask, const Map& m, int c, int r) {
    static const int DC[5] = {0, 1, -1, 0, 0}, DR[5] = {0, 0, 0, 1, -1};
    for (int k = 0; k < 5; k++) {
        const int nc = c + DC[k], nr = r + DR[k];
        if (m.inside(nc, nr) && mask[nr * m.w + nc]) return true;
    }
    return false;
}

void ensureReach(Map& m, TilePos from, int c, int r, std::vector<uint8_t>* mask) {
    std::vector<uint8_t> local;
    if (!mask) {
        local = reachMask(m, from.c, from.r);
        mask = &local;
    }
    if (touchesMask(*mask, m, c, r)) return;
    const auto path = astar(m, from, {c, r}, [&](int nc, int nr) -> double {
        if (nc == c && nr == r) return 1;
        if (m.block[m.idx(nc, nr)]) return -1;
        const uint8_t t = m.get(nc, nr);
        if (walkable(t)) return 1;
        return t == TREE ? 3 : t == ROCK ? 4 : t == WATER ? 5 : -1;
    });
    for (const auto& p : path) {
        if (p.c == c && p.r == r) continue;
        const uint8_t t = m.get(p.c, p.r);
        if (t == TREE || t == ROCK) m.set(p.c, p.r, GRASS);
        else if (t == WATER) m.set(p.c, p.r, BRIDGE);
    }
    if (!path.empty()) *mask = reachMask(m, from.c, from.r);   // the carved corridor opened new ground
}

}   // namespace hc

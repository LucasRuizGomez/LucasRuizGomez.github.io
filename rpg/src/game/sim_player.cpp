// The hero: movement, dodge roll, melee, crossbow, whirlwind, mounts, recall, taking damage.
#include "game/sim.h"

#include <cmath>

namespace hc {

namespace {

bool rectCircle(double rx, double ry, double rw, double rh, double cx, double cy, double cr) {
    const double nx = std::max(rx, std::min(cx, rx + rw)), ny = std::max(ry, std::min(cy, ry + rh));
    return (nx - cx) * (nx - cx) + (ny - cy) * (ny - cy) <= cr * cr;
}

std::string hearts(int halves) { return halves % 2 ? std::to_string(halves / 2) + ".5" : std::to_string(halves / 2); }

}   // namespace

std::array<double, 4> Sim::attackBox() const {
    const double R = weaponReach(), cx = P.x, cy = P.y - 6;
    switch (P.dir) {
        case Dir::Right: return {cx + 2, cy - 9, R, 18};
        case Dir::Left: return {cx - 2 - R, cy - 9, R, 18};
        case Dir::Down: return {cx - 9, cy + 1, 18, R};
        default: return {cx - 9, cy - 1 - R, 18, R};
    }
}

// Keyboard and gamepad play: the nearest monster in reach of a swing...
double Sim::swingOffset(double f) {
    if (f < 2) return -SWING_SPAN - 0.3 * f / 2;   // drawn back
    if (f <= 8) {
        const double t = (f - 2) / 6;
        return -SWING_SPAN - 0.3 + (2 * SWING_SPAN + 0.3) * (1 - (1 - t) * (1 - t));
    }
    return SWING_SPAN + 0.2 * std::min(1.0, (f - 8) / 4);   // follow-through
}

const Enemy* Sim::meleeTarget() const {
    const Enemy* best = nullptr;
    double bd = 1e18;
    for (const auto& e : map().enemies) {
        if (!e.alive) continue;
        const double d = hypot2(e.x - P.x, (e.y - e.cy) - (P.y - 6));
        if (d < weaponReach() + e.r + 8 && d < bd) {
            bd = d;
            best = &e;
        }
    }
    return best;
}

// ...and the crossbow target: nearest monster ahead within about 60 degrees, else anything close by.
const Enemy* Sim::bowTarget() const {
    const Vec2 f = dirVec(P.dir);
    const Enemy *ahead = nullptr, *near = nullptr;
    double ad = 1e18, nd = 1e18;
    for (const auto& e : map().enemies) {
        if (!e.alive || e.def->passive) continue;
        const double vx = e.x - P.x, vy = (e.y - e.cy) - (P.y - 6), d = hypot2(vx, vy);
        if (d > 160 || d < 1) continue;
        if ((vx * f.x + vy * f.y) / d > 0.5 && d < ad) {
            ad = d;
            ahead = &e;
        }
        if (d < 70 && d < nd) {
            nd = d;
            near = &e;
        }
    }
    return ahead ? ahead : near;
}

void Sim::updatePlayer() {
    Map& m = map();
    const Tuning& T = db.tuning;
    double mx = in_.moveX, my = in_.moveY, mag = std::min(1.0, hypot2(mx, my));
    const double ml = hypot2(mx, my);
    if (ml > 0) {
        mx /= ml;
        my /= ml;
    }
    // Facing uses only clear components, so a slightly tilted stick does not flip the hero
    const int dx = std::abs(mx) > 0.38 ? sign(mx) : 0, dy = std::abs(my) > 0.38 ? sign(my) : 0;
    P.moving = ml > 0;
    const bool busy = P.atkT > 0 || P.bowT > 0;
    if (dx && !busy) P.face = dx;
    if (P.moving && !busy) {
        if (dx && dy) {
            const bool keeps = (P.dir == Dir::Left && dx < 0) || (P.dir == Dir::Right && dx > 0) || (P.dir == Dir::Up && dy < 0) || (P.dir == Dir::Down && dy > 0);
            if (!keeps) P.dir = dx > 0 ? Dir::Right : Dir::Left;
        } else if (dx || dy) P.dir = dx ? (dx > 0 ? Dir::Right : Dir::Left) : (dy > 0 ? Dir::Down : Dir::Up);
    }
    const double mountMul = P.mount.empty() ? 1.0 : db.item(P.mount).mountSpeed;
    const double sp = T.playerSpeed * mag * (P.atkT > 0 ? 0.45 : 1) * speedMul() * mountMul;
    const bool fly = flying();
    if (fly) {   // in the air nothing blocks the way and there is nothing to fight or pick up
        attackBuf_ = shootBuf_ = 0;
        wantDodge_ = wantInteract_ = wantSkill_ = false;
    }

    // Dodge roll: a quick dash the way you move (or face) that lets hits and bolts pass through
    if (wantDodge_) {
        wantDodge_ = false;
        if (P.rollCd <= 0 && P.roll <= 0 && P.atkT <= 0) {
            dismount();
            const Vec2 f = dirVec(P.dir);
            P.rvx = ml > 0 ? mx : f.x;
            P.rvy = ml > 0 ? my : f.y;
            P.roll = T.rollTicks;
            P.rollCd = static_cast<int>(std::lround(T.rollCooldown * (1 - std::clamp(skillStat("roll"), 0.0, 70.0) / 100)));
            play("dodge");
            burst(P.x, P.y, Color(200, 176, 144), 6, 0.7, 0.02);
        }
    }
    if (P.leapT > 0) {   // Leap Slam: an arc through the air, then the blow lands
        const double k = 1 - (P.leapT - 1) / 22.0;
        double nx = P.leapX + (P.leapTX - P.leapX) * k, ny = P.leapY + (P.leapTY - P.leapY) * k;
        P.x = nx;
        P.y = ny;
        if (--P.leapT == 0) {
            shake = std::max(shake, 9.0);
            hitstop = std::max(hitstop, 5);
            play("whirl");
            const int dmg = weaponDamage() * 2 + static_cast<int>(skillStat("dmg"));
            for (auto& e : m.enemies) {
                if (!e.alive) continue;
                const double ex = e.x - P.x, ey = (e.y - e.cy) - (P.y - 6), d = hypot2(ex, ey);
                if (d > 38 + e.r) continue;
                const double kk = d > 0 ? 4 / d : 0;
                damageEnemy(e, dmg, ex * kk, ey * kk);
            }
            for (int i = 0; i < 36; i++) {
                const double a = i / 36.0 * PI * 2;
                particles.push_back({P.x + std::cos(a) * 6, P.y + std::sin(a) * 3, std::cos(a) * 2.2, std::sin(a) * 1.1, -0.3, 18, 2, Color(150, 130, 100)});
            }
        }
    } else if (P.roll > 0) {
        const double k = static_cast<double>(P.roll) / T.rollTicks;
        m.moveBox(P.x, P.y, P.rvx * (1.1 + 2.3 * k), P.rvy * (1.1 + 2.3 * k), 4, 3, false);
        if (P.roll % 3 == 0) P.trail.push_back({P.x, P.y, P.face, 10});
        P.roll--;
        P.animT += 2;
    } else if (P.moving && fly) {
        P.x = std::clamp(P.x + mx * sp, 12.0, m.w * TILE - 12.0);
        P.y = std::clamp(P.y + my * sp, 16.0, m.h * TILE - 8.0);
        P.animT++;
    } else if (P.moving) {
        // Walking up to a doorway: ease the hero sideways into it so a door one tile wide is easy to hit
        if (std::abs(my) > 0.5 && std::abs(mx) < 0.5)
            for (const auto& l : m.links) {
                const double dx = l.c * TILE + 8 - P.x;
                const int tr = ifloor((P.y - 4) / TILE), ahead = my < 0 ? tr - 1 : tr + 1;
                if ((l.r == ahead || l.r == tr) && std::abs(dx) < 11 && std::abs(dx) > 0.2 && m.get(l.c, l.r) == STAIRS_DOWN) P.x += std::clamp(dx, -1.0, 1.0);
            }
        m.moveBox(P.x, P.y, mx * sp, my * sp, 4, 3, false);
        P.animT++;
        if (!P.mount.empty() && P.animT % 12 == 0) burst(P.x, P.y, Color(184, 154, 112), 2, 0.5, 0.03);
    } else if (fly) P.animT++;   // wings keep beating while hovering
    else P.animT = 0;
    for (auto& t : P.trail) t.t--;
    std::erase_if(P.trail, [](const Trail& t) { return t.t <= 0; });
    if (P.kbx != 0 || P.kby != 0) {
        m.moveBox(P.x, P.y, P.kbx, P.kby, 4, 3, false);
        P.kbx *= 0.78;
        P.kby *= 0.78;
        if (std::abs(P.kbx) < 0.05) P.kbx = 0;
        if (std::abs(P.kby) < 0.05) P.kby = 0;
    }
    if (P.iframes > 0) P.iframes--;
    if (P.atkCd > 0) P.atkCd--;
    if (P.bowCd > 0) P.bowCd--;
    if (P.bowT > 0) P.bowT--;
    if (P.actCd > 0) P.actCd--;
    if (P.rollCd > 0) P.rollCd--;
    if (P.whirlCd > 0) P.whirlCd--;
    for (auto& [id, cd] : P.abilityCd)
        if (cd > 0) cd--;
    if (P.whirlT > 0) P.whirlT--;
    if (P.combatT < 100000) P.combatT++;
    // Second Wind: out of combat, half a heart back every few seconds
    if (const double regen = skillStat("regen"); regen > 0 && P.combatT > 300 && P.hp < maxHp() && mode == Mode::Play) {
        if (++P.regenT >= static_cast<int>(600 / regen)) {
            P.regenT = 0;
            P.hp++;
            floater(P.x, P.y - 20, "+0.5", Color(123, 224, 138));
        }
    } else P.regenT = 0;
    for (auto& b : P.buffs) b.t--;
    std::erase_if(P.buffs, [](const Buff& b) { return b.t <= 0; });

    double aimA = std::atan2(dirVec(P.dir).y, dirVec(P.dir).x);
    auto aimAt = [&](double ax, double ay) {
        if (std::abs(ax) + std::abs(ay) > 1e-6) aimA = std::atan2(ay, ax);
        P.dir = snap4(ax, ay);
        if (std::abs(ax) > 1) P.face = ax < 0 ? -1 : 1;
    };

    // Melee: tap (buffered) or hold to keep swinging
    if (attackBuf_ > 0) attackBuf_--;
    const bool holdAtk = in_.holdAttack;
    if ((attackBuf_ > 0 || holdAtk) && P.atkCd <= 0 && P.bowT <= 0 && P.roll <= 0) {
        dismount();
        P.recallT = 0;
        const bool byMouse = (holdAtk && in_.mouseHeld) || (attackBuf_ > 0 && attackMouse_);
        if (byMouse) aimAt(in_.aimX, in_.aimY);
        else if (const Enemy* t = meleeTarget()) aimAt(t->x - P.x, (t->y - t->cy) - (P.y - 6));   // turn toward a monster right next to you
        attackBuf_ = 0;
        P.swingA = byMouse || meleeTarget() ? aimA : std::atan2(dirVec(P.dir).y, dirVec(P.dir).x);
        P.swingSide = -P.swingSide;
        P.atkT = T.attackTicks;
        P.atkCd = weaponCooldown();
        P.hitSet.clear();
        play("swing");
    }
    if (P.atkT > 0) {
        const int f = T.attackTicks - P.atkT;
        if (f >= 2 && f <= 8) {
            // The blade sweeps an arc in front of the hero; it strikes what it has passed over so far
            const auto box = attackBox();
            const double sweep = swingOffset(f), reach = weaponReach() + 5, cx = P.x, cy = P.y - 6;
            auto inArc = [&](double tx, double ty, double r) {
                const double dx = tx - cx, dy = ty - cy, d = std::hypot(dx, dy);
                if (d > reach + r) return false;
                if (d < 6 + r) return true;   // right on top of the hero
                double off = std::remainder(std::atan2(dy, dx) - P.swingA, 2 * PI) * P.swingSide;
                const double slack = std::asin(std::min(1.0, r / d));   // a big body is hit by its edge
                return off >= -SWING_SPAN - slack && off <= sweep + slack;
            };
            const Vec2 v{std::cos(P.swingA), std::sin(P.swingA)};
            for (auto& e : m.enemies) {
                if (!e.alive || std::find(P.hitSet.begin(), P.hitSet.end(), e.uid) != P.hitSet.end()) continue;
                if (!inArc(e.x, e.y - e.cy, e.r)) continue;
                P.hitSet.push_back(e.uid);
                damageEnemy(e, weaponDamage(), v.x * 2.6, v.y * 2.6);
                hitstop = std::max(hitstop, e.alive ? 3 : 5);
            }
            for (auto& p : m.proj)
                if (!p.fromPlayer && (inArc(p.x, p.y, 3) || rectCircle(box[0], box[1], box[2], box[3], p.x, p.y, 3))) {
                    p.life = 0;
                    p.hitSomething = true;
                    burst(p.x, p.y, Color(236, 230, 216), 5, 1);
                    play("hit");
                }
            for (auto& o : m.objects)
                if (o.kind == ObjKind::Pot && !o.gone && inArc(o.c * TILE + 8, o.r * TILE + 8, 6)) breakPot(m, o);
        }
        P.atkT--;
    }

    // Crossbow: at the cursor (any angle), along the right stick, at the marked target, or straight ahead
    if (shootBuf_ > 0) shootBuf_--;
    const bool fresh = shootBuf_ == T.inputBuffer - 1, holdShot = in_.holdShoot;
    if ((shootBuf_ > 0 || holdShot) && P.atkT <= 0 && P.roll <= 0) {
        if (!hasItem("bow") || !hasItem("arrows")) {
            if (fresh) {
                toast(!hasItem("bow") ? "You need a crossbow. Craft one, or earn one from Huntress Mira." : "Out of bolts. Craft more from wood and stone.");
                play("deny");
            }
            shootBuf_ = 0;
        } else if (P.bowCd <= 0) {
            dismount();
            P.recallT = 0;
            double vx, vy;
            const bool byMouse = (holdShot && in_.mouseHeld) || (shootBuf_ > 0 && shootMouse_);
            if (byMouse) {
                const double l = std::max(1e-6, hypot2(in_.aimX, in_.aimY));
                vx = in_.aimX / l;
                vy = in_.aimY / l;
                aimAt(in_.aimX, in_.aimY);
            } else if (hypot2(in_.padAimX, in_.padAimY) > 0.4) {
                const double l = hypot2(in_.padAimX, in_.padAimY);
                vx = in_.padAimX / l;
                vy = in_.padAimY / l;
                P.dir = snap4(vx, vy);
                if (std::abs(vx) > 0.2) P.face = vx < 0 ? -1 : 1;
            } else if (const Enemy* t = bowTarget()) {
                const double ax = t->x - P.x, ay = (t->y - t->cy) - (P.y - 6), l = std::max(1e-6, hypot2(ax, ay));
                vx = ax / l;
                vy = ay / l;
                aimAt(ax, ay);
            } else {
                const Vec2 f = dirVec(P.dir);
                vx = f.x;
                vy = f.y;
            }
            shootBuf_ = 0;
            Projectile p;
            p.x = P.x + vx * 6;
            p.y = P.y - 6 + vy * 4;
            p.vx = vx * 3.2;
            p.vy = vy * 3.2;
            p.fromPlayer = true;
            p.dmg = bowDamage();
            p.life = 75;
            p.kind = Projectile::Kind::Arrow;
            m.proj.push_back(p);
            removeItem("arrows", 1);
            P.bowCd = 26;
            P.bowT = 10;
            play("shoot");
        }
    }

    updateSkills(m);
    gateHint(m);
    if (wantHeal_) {
        wantHeal_ = false;
        quickHeal();
    }
    if (wantInteract_) {
        wantInteract_ = false;
        interact(m);
    }

    // Stairs and doorways (not from the saddle of a flying mount)
    if (!fade && !fly) {
        const int tc = ifloor(P.x / TILE), tr = ifloor(P.y / TILE);
        const uint8_t under = m.get(tc, tr);
        if (under == STAIRS_DOWN || under == STAIRS_UP)
            for (const auto& link : m.links) {
                if (link.c != tc || link.r != tr) continue;
                const Link l = link;
                transition([this, l] {
                    const Map& to = maps.at(l.toMap);
                    const TilePos p = l.toC >= 0 ? TilePos{l.toC, l.toR} : to.start;
                    enterMap(l.toMap, p.c * TILE + 8, p.r * TILE + 10);
                    P.dir = l.face;
                    wantSave = true;
                });
                break;
            }
    }
}

const std::vector<Sim::Ability>& Sim::abilities() {
    static const std::vector<Ability> A = {
        {"warcry", "War Cry", "owl_brave", 1200},
        {"leap", "Leap Slam", "owl_strengthen", 600},
        {"stoneskin", "Stone Skin", "owl_stone", 1500},
        {"volley", "Volley", "owl_crossbow", 420},
    };
    return A;
}

int Sim::abilityCooldown(const std::string& id) const {
    const auto it = P.abilityCd.find(id);
    return it == P.abilityCd.end() ? 0 : std::max(0, it->second);
}

void Sim::useAbility(const std::string& id) {
    const Ability* ab = nullptr;
    for (const auto& a : abilities())
        if (id == a.id) ab = &a;
    if (!ab || mode != Mode::Play || flying()) return;
    if (!abilityKnown(id)) {
        toast("Learn " + std::string(ab->name) + " in the skill tree first.");
        play("deny");
        return;
    }
    if (abilityCooldown(id) > 0 || P.roll > 0 || P.leapT > 0) {
        play("deny");
        return;
    }
    Map& m = map();
    const Vec2 f = dirVec(P.dir);
    if (id == "volley") {
        if (!hasItem("bow") || !hasItem("arrows")) {
            toast("Volley needs a crossbow and bolts.");
            play("deny");
            return;
        }
        // five bolts in a fan, for the price of two
        const double a0 = std::atan2(f.y, f.x);
        for (int i = -2; i <= 2; i++) {
            Projectile p;
            const double a = a0 + i * 0.2;
            p.x = P.x + std::cos(a) * 6;
            p.y = P.y - 6 + std::sin(a) * 4;
            p.vx = std::cos(a) * 3.4;
            p.vy = std::sin(a) * 3.4;
            p.fromPlayer = true;
            p.dmg = bowDamage();
            p.life = 70;
            m.proj.push_back(p);
        }
        removeItem("arrows", std::min(2, countItem("arrows")));
        P.bowT = 10;
        play("shoot");
    } else if (id == "warcry") {
        // a roar: fury for eight seconds, and everything close is thrown back
        std::erase_if(P.buffs, [](const Buff& b) { return b.id == "frenzy"; });
        P.buffs.push_back({"frenzy", 480});
        for (auto& e : m.enemies) {
            if (!e.alive) continue;
            const double ex = e.x - P.x, ey = e.y - P.y, d = hypot2(ex, ey);
            if (d > 56 || d < 1) continue;
            e.kbx += ex / d * 4;
            e.kby += ey / d * 4;
        }
        shake = std::max(shake, 5.0);
        burst(P.x, P.y - 10, Color(255, 120, 80), 24, 2.0);
        play("whirl");
        floater(P.x, P.y - 26, "War Cry", Color(255, 150, 110));
    } else if (id == "stoneskin") {
        std::erase_if(P.buffs, [](const Buff& b) { return b.id == "protection"; });
        P.buffs.push_back({"protection", 420});
        burst(P.x, P.y - 8, Color(170, 160, 140), 20, 1.2);
        play("shrine");
        floater(P.x, P.y - 26, "Stone Skin", Color(200, 190, 170));
    } else if (id == "leap") {
        // leap up to four tiles the way you face, landing on the furthest open ground
        dismount();
        double tx = P.x, ty = P.y;
        for (int step = 1; step <= 16; step++) {
            const double nx = P.x + f.x * step * 4, ny = P.y + f.y * step * 4;
            if (!m.boxFree(nx, ny, 4, 3, false)) break;
            tx = nx;
            ty = ny;
        }
        P.leapX = P.x;
        P.leapY = P.y;
        P.leapTX = tx;
        P.leapTY = ty;
        P.leapT = 22;
        play("dodge");
    }
    P.abilityCd[id] = ab->cooldown;
    P.combatT = std::min(P.combatT, 60);
}

void Sim::gateHint(Map& m) {
    if (!m.outdoor || tickCount % 15 != 0 || tickCount < gateHintAt_) return;
    for (const auto& o : m.objects) {
        if (o.kind != ObjKind::Gate || o.id != "zone_gate" || o.gone) continue;
        if (std::abs(o.c * TILE + 8 - P.x) > 30 || std::abs(o.r * TILE + 8 - P.y) > 30) continue;
        std::string boss = "its guardian", place = o.res;
        for (const auto& d : db.dungeons)
            if (d.id == o.res) {
                place = d.name;
                if (const EnemyDef* e = db.findEnemy(d.boss)) boss = e->name;
            }
        toast("The gate is sealed. It opens when " + boss + " falls in " + place + ".", Color(255, 190, 120));
        play("deny");
        gateHintAt_ = tickCount + 300;
        return;
    }
}

void Sim::updateSkills(Map& m) {
    const Tuning& T = db.tuning;
    // Whirlwind: one spinning blow that hits everything around you
    if (wantSkill_) {
        wantSkill_ = false;
        if (!whirlwindUnlocked()) {
            toast("Learn Whirlwind in the skill tree first.");
            play("deny");
        } else if (P.whirlCd > 0) play("deny");
        else if (P.roll <= 0) {
            dismount();
            P.recallT = 0;
            P.whirlCd = whirlCooldown();
            P.whirlT = 18;
            play("whirl");
            play("swing");
            shake = std::max(shake, 4.0);
            const int dmg = static_cast<int>(std::ceil(weaponDamage() * T.whirlMul * (1 + skillStat("whirldmg") / 100)));
            for (auto& e : m.enemies) {
                if (!e.alive) continue;
                const double ex = e.x - P.x, ey = (e.y - e.cy) - (P.y - 6), d = hypot2(ex, ey);
                if (d > T.whirlRadius + e.r) continue;
                const double k = d > 0 ? 3.2 / d : 0;
                damageEnemy(e, dmg, ex * k, ey * k);
            }
            for (auto& o : m.objects)
                if (o.kind == ObjKind::Pot && !o.gone && hypot2(o.c * TILE + 8 - P.x, o.r * TILE + 8 - P.y) < T.whirlRadius + 6) breakPot(m, o);
            for (auto& p : m.proj)
                if (!p.fromPlayer && hypot2(p.x - P.x, p.y - (P.y - 6)) < T.whirlRadius) {
                    p.life = 0;
                    p.hitSomething = true;
                }
            for (int i = 0; i < 28; i++) {
                const double a = i / 28.0 * PI * 2;
                particles.push_back({P.x + std::cos(a) * 12, P.y - 6 + std::sin(a) * 12, std::cos(a) * 1.6, std::sin(a) * 1.6, 0, 14, 2, Color(255, 255, 255)});
            }
        }
    }

    // Mount: a short whistle, then ride. Outdoors only, and not in the middle of a fight.
    if (wantMount_) {
        wantMount_ = false;
        if (!P.mount.empty()) dismount();
        else {
            const std::string pick = chosenMount();
            if (pick.empty()) {
                toast("You have no mount. Stablemasters sell them in Greenhollow and Highwatch.");
                play("deny");
            } else if (!m.outdoor) {
                toast("You cannot ride in here.");
                play("deny");
            } else if (P.combatT < T.combatLockTicks) {
                toast("You cannot mount up in the middle of a fight.");
                play("deny");
            } else P.mountCast = T.mountCastTicks;
        }
    }
    if (P.mountCast > 0) {
        if (P.atkT > 0 || P.roll > 0 || P.combatT < 30) P.mountCast = 0;
        else if (--P.mountCast == 0) {
            const std::string pick = chosenMount();
            if (!pick.empty()) {
                P.mount = pick;
                const bool air = db.item(pick).mountFly;
                play(air ? "fly" : "mount");
                burst(P.x, P.y, air ? Color(240, 220, 170) : Color(200, 176, 144), air ? 18 : 10, air ? 1.4 : 0.9, 0.02);
                if (air) toast("You take to the air. Press the mount key over open ground to land.", Color(159, 210, 255));
            }
        }
    }

    // Recall: stand still for a few seconds to return to the last town you visited
    if (wantRecall_) {
        wantRecall_ = false;
        if (P.recallT > 0) P.recallT = 0;
        else if (P.combatT < T.combatLockTicks) {
            toast("You cannot recall in the middle of a fight.");
            play("deny");
        } else {
            P.recallT = T.recallTicks;
            const TownDef* t = db.town(P.lastTown);
            toast("Recalling to " + (t ? t->name : std::string("town")) + "... stand still.", Color(159, 210, 255));
        }
    }
    if (P.recallT > 0) {
        if (P.moving || P.roll > 0 || P.combatT < 30) {
            P.recallT = 0;
            toast("Recall interrupted.");
        } else {
            if (P.recallT % 8 == 0) burst(P.x, P.y - 6, Color(159, 210, 255), 3, 0.9, -0.05);
            if (--P.recallT == 0) travelTo(P.lastTown);
        }
    }
}

void Sim::hurtPlayer(int dmg, double fx, double fy) {
    if (invulnerable() || mode != Mode::Play || dmg <= 0) return;
    hitstop = std::max(hitstop, 4);
    const int def = shieldDef();
    // Armour softens strong hits but never blocks more than half of one
    int d = dmg > 1 ? std::max((dmg + 1) / 2, dmg - def) : dmg;
    if (hasBuff("protection")) d = std::max(1, d / 2);
    P.hp = std::max(0, P.hp - d);
    P.iframes = db.tuning.hitInvuln;
    P.combatT = 0;
    dismount();
    const double a = std::atan2(P.y - fy, P.x - fx);
    P.kbx = std::cos(a) * 2.6;
    P.kby = std::sin(a) * 2.6;
    shake = std::max(shake, 6.0);
    floater(P.x, P.y - 20, "-" + hearts(d), Color(255, 122, 106));
    burst(P.x, P.y - 8, Color(224, 80, 60), 8, 1);
    play("hurt");
    if (def && dmg > d) floater(P.x + 8, P.y - 26, "blocked", Color(159, 210, 255));
    if (P.hp <= 0) die();
}

void Sim::die() {
    deaths++;
    play("die");
    deadAt = tickCount;
    burst(P.x, P.y - 8, Color(224, 80, 60), 24, 1.6);
    P.recallT = 0;
    mode = Mode::Dead;
}

}   // namespace hc

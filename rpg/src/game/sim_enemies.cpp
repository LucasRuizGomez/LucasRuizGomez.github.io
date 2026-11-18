// Monsters: AI per archetype, bosses, projectiles, traps, damage, death and what they drop.
#include "game/sim.h"

#include <cmath>

namespace hc {

void Sim::damageEnemy(Enemy& e, int dmg, double kx, double ky) {
    if (!e.alive) return;
    bool crit = false;
    if (hasBuff("frenzy")) dmg = static_cast<int>(std::ceil(dmg * 1.5));
    if (rng.chance(statBonus("crit") / 100.0)) {
        dmg *= 2;
        crit = true;
    }
    if (e.elite && e.elite->armor) dmg = std::max(1, dmg - e.elite->armor);
    e.hp -= dmg;
    e.flash = 10;
    e.aggroT = 0;
    P.combatT = 0;
    const double k = e.def->isBoss() ? 0.12 : e.elite ? 0.6 : 1;
    e.kbx = kx * k;
    e.kby = ky * k;
    floater(e.x, e.y - e.cy - 12, std::to_string(dmg) + (crit ? "!" : ""), crit ? Color(255, 226, 110) : Color(255, 255, 255));
    burst(e.x, e.y - e.cy, e.def->look == Look::Animal ? Color(200, 56, 74) : Color(210, 215, 222), 6, 1.2);
    play("hit");
    shake = std::max(shake, e.def->isBoss() ? 3.0 : 2.0);
    if (e.hp <= 0) killEnemy(map(), e);
}

void Sim::killEnemy(Map& m, Enemy& e) {
    if (!e.alive) return;
    e.alive = false;
    const EnemyDef& def = *e.def;
    if (e.def->fly && m.outdoor && m.get(ifloor(e.x / TILE), ifloor(e.y / TILE)) == WATER) {
        waterFx(m, e.x, e.y, true);
        e.sank = true;
    }
    m.corpses.push_back({e, 0});
    burst(e.x, e.y - e.cy, Color(236, 230, 216), 14, 1.4);
    burst(e.x, e.y - e.cy, Color(107, 101, 120), 10, 0.8);
    play("kill");
    if (e.spawn >= 0 && e.spawn < static_cast<int>(m.spawns.size())) {
        m.spawns[e.spawn].enemyUid = 0;
        m.spawns[e.spawn].respawnAt = tickCount + db.tuning.respawnTicks;
    }
    if (e.summoned) return;   // conjured helpers leave nothing behind

    const bool fortune = hasBuff("fortune");
    for (const auto& d : def.drops)
        if (rng.chance(d.chance)) dropItem(m, {d.item, d.qty}, e.x, e.y);
    const double goldMul = (fortune ? 1.5 : 1) * (e.elite ? 2 : 1) * (1 + skillStat("gold") / 100);
    if (def.goldMax > 0 && rng.chance(def.goldChance)) dropItem(m, {"gold", std::max(1, static_cast<int>(rng.between(def.goldMin, def.goldMax) * goldMul))}, e.x, e.y);
    if (def.isBoss()) {
        dropItem(m, randomGear(def.level, 2), e.x - 12, e.y);
        dropItem(m, randomGear(def.level, 1), e.x + 12, e.y);
        dropItem(m, {"heart_shard", 1}, e.x, e.y + 10);
        dropItem(m, {"potion", 2}, e.x, e.y - 8);
    } else if (e.elite) dropItem(m, randomGear(def.level, 1), e.x, e.y);
    else if (!def.passive && rng.chance((fortune ? 0.07 : 0.035) * (1 + skillStat("loot") / 100))) dropItem(m, randomGear(def.level, 0), e.x, e.y);
    if (!def.passive && (e.elite || rng.chance(db.tuning.globeChance))) {
        Drop g;
        g.globe = true;
        g.item = {"bandage", 1};
        g.x = e.x;
        g.y = e.y;
        g.vz = 2.0;
        m.drops.push_back(g);
    }
    const int leech = statBonus("leech");
    if (leech && P.hp < maxHp() && rng.chance(leech / 100.0)) {
        P.hp++;
        floater(P.x, P.y - 20, "+0.5", Color(123, 224, 138));
    }
    int xp = def.xp * (e.elite ? 3 : 1);
    // Experience follows the gap in levels: little from monsters far beneath you, a bonus for those above
    if (const int gap = P.level - def.level; gap > 6) xp = std::max(1, static_cast<int>(xp * std::max(0.05, 1 - (gap - 6) * 0.07)));
    else if (gap < 0) xp = static_cast<int>(xp * (1 + std::min(0.5, -gap * 0.03)));
    gainXp(xp);
    questEvent("kill", def.id, def.tag);

    if (def.isBoss()) {
        shake = 20;
        play("boom");
        burst(e.x, e.y - 14, Color(242, 210, 122), 40, 2.2);
        for (auto& o : m.enemies)
            if (o.summoned && o.alive) killEnemy(m, o);
        if (m.dungeon) {
            bossesDefeated.insert(m.dungeon->id);
            questEvent("boss", m.dungeon->id);
            openZoneGates(true);
        }
        if (def.boss.final) {
            dropItem(m, {"crown", 1}, e.x - 10, e.y + 4);
            victoryAt = tickCount + 140;
            m.change(32, 3, STAIRS_DOWN);
            toast("A stairway opens beneath the throne...", Color(242, 210, 122));
        } else toast(def.name + " has fallen!", Color(242, 210, 122));
        wantSave = true;
    }
}

namespace {

// Facing comes from the distance a mob really travelled this tick, so it can never walk backwards.
// Animals keep their current 4-way facing while it still points within ~50 degrees of the motion.
void faceMotion(Enemy& e, double mx, double my) {
    const double len = hypot2(mx, my);
    e.moving = len > 0.05;
    if (!e.moving) return;
    if (std::abs(mx) > 0.05) e.face = mx < 0 ? -1 : 1;
    const Vec2 v = dirVec(e.dir);
    if ((v.x * mx + v.y * my) / len < 0.64) e.dir = snap4(mx, my);
}

}   // namespace

void Sim::updateEnemies(Map& m) {
    auto go = [&](Enemy& e, double mx, double my) { return m.moveBox(e.x, e.y, mx, my, e.hw, e.hh, e.def->fly); };
    auto chase = [&](Enemy& e, double dx, double dy, double d, double sp) {
        const double mx = dx / d * sp, my = dy / d * sp;
        const auto [hx, hy] = go(e, mx, my);
        if (hx && !hy) go(e, 0, (my < 0 ? -1 : 1) * sp);
        if (hy && !hx) go(e, (mx < 0 ? -1 : 1) * sp, 0);
    };
    auto wander = [&](Enemy& e, double sp) {
        if (e.t % 150 == 0) {
            const double a = rng.next() * PI * 2;
            e.vx = std::cos(a) * sp;
            e.vy = std::sin(a) * sp;
            e.st = 30 + rng.range(40);
        }
        if (e.st > 0) {
            e.st--;
            const auto [hx, hy] = go(e, e.vx, e.vy);
            if (hx) e.vx *= -1;
            if (hy) e.vy *= -1;
        }
    };
    const bool night = m.outdoor && isNight();
    const bool inTown = m.outdoor && !townId.empty();
    std::vector<Enemy> born;

    for (auto& e : m.enemies) {
        if (!e.alive) continue;
        const EnemyDef& def = *e.def;
        e.t++;
        if (e.flash > 0) e.flash--;
        if (e.act > 0) e.act--;
        if (e.aggroT < 100000) e.aggroT++;
        if (e.kbx != 0 || e.kby != 0) {
            m.moveBox(e.x, e.y, e.kbx, e.kby, e.hw, e.hh, def.fly);
            e.kbx *= 0.8;
            e.kby *= 0.8;
            if (std::abs(e.kbx) < 0.05) e.kbx = 0;
            if (std::abs(e.kby) < 0.05) e.kby = 0;
        }
        const double dx = P.x - e.x, dy = (P.y - 5) - (e.y - e.cy), d = std::max(1e-6, hypot2(dx, dy));
        e.moving = e.fast = false;
        const double ox = e.x, oy = e.y;
        const double sm = e.elite ? e.elite->speedMul : 1.0;
        // While being knocked back a mob is stunned: it slides but does not walk or turn.
        // Monsters also leave you alone inside a town.
        const bool stunned = !def.isBoss() && std::abs(e.kbx) + std::abs(e.kby) > 0.35;
        const bool calm = (inTown || flying()) && !def.isBoss();   // nothing can reach a hero on a flying mount
        if (!stunned) switch (def.ai) {
                case Ai::Charger: {
                    const auto& ch = def.charge;
                    if (e.wind > 0) {   // wind-up: turns on the spot to face you, then goes
                        e.wind--;
                        e.face = dx < 0 ? -1 : 1;
                        e.dir = snap4(dx, dy);
                        e.aggroT = 0;
                        if (def.look == Look::Animal && e.wind % 6 == 0) burst(e.x - e.face * 6, e.y, Color(184, 154, 112), 3, 0.6, 0.03);
                        if (e.wind == 0) {
                            e.vx = dx / d * ch.speed * sm;
                            e.vy = dy / d * ch.speed * sm;
                            e.hop = 24;
                            play("charge");
                        }
                    } else if (e.hop > 0) {
                        e.hop--;
                        e.fast = true;
                        e.aggroT = 0;
                        const auto [hx, hy] = go(e, e.vx, e.vy);
                        if (hx || hy) e.hop = 0;
                        e.vx *= 0.985;
                        e.vy *= 0.985;
                    } else if (!calm && d < ch.range && e.t % ch.every == 0) {
                        e.wind = ch.wind;
                        if (d < 140) play("alert");
                    } else if (!calm && def.stalk > 0 && d < 150) {
                        chase(e, dx, dy, d, def.stalk * sm);
                        e.aggroT = 0;
                    } else wander(e, def.speed);
                    break;
                }
                case Ai::Chaser:
                    if (!calm && d < def.aggro + (night ? 40 : 0)) {   // bolder and faster at night
                        chase(e, dx, dy, d, def.speed * sm * (night ? 1.15 : 1));
                        e.fast = true;
                        e.aggroT = 0;
                    } else wander(e, 0.4);
                    break;
                case Ai::Flee:
                    if (d < 72) {
                        chase(e, -dx, -dy, d, def.speed);
                        e.fast = true;
                    } else wander(e, 0.3);
                    break;
                case Ai::Flier: {
                    const bool near = !calm && d < 130;
                    if (near) {
                        const double px = -dy / d, py = dx / d, wob = std::sin(e.t * 0.12) * 0.7;
                        e.vx += (dx / d + px * wob) * 0.06;
                        e.vy += (dy / d + py * wob) * 0.06;
                        e.aggroT = 0;
                    } else {
                        e.vx += (rng.next() - 0.5) * 0.08;
                        e.vy += (rng.next() - 0.5) * 0.08;
                    }
                    const double sp = hypot2(e.vx, e.vy), max = near ? def.speed * sm : 0.45;
                    if (sp > max) {
                        e.vx *= max / sp;
                        e.vy *= max / sp;
                    }
                    const auto [hx, hy] = go(e, e.vx, e.vy);
                    if (hx) e.vx *= -1;
                    if (hy) e.vy *= -1;
                    break;
                }
                case Ai::Guard:
                    if (!calm && d < def.aggro) {
                        chase(e, dx, dy, d, def.speed * sm);
                        e.aggroT = 0;
                        if (d < 24 && e.act <= 0) e.act = 24;
                    } else wander(e, 0.3);
                    break;
                case Ai::Archer:   // keeps its distance; a red line tracks you, then locks so you can step aside
                    if (!calm && d < def.aggro) {
                        e.aggroT = 0;
                        if (e.aim > 0) {
                            e.aim--;
                            e.face = dx < 0 ? -1 : 1;
                            if (e.aim > 12) {
                                e.ax = P.x;
                                e.ay = P.y - 6;
                            }
                            if (e.aim == 0) {
                                e.act = 50;
                                const double ax = e.ax - e.x, ay = e.ay - (e.y - 8), ad = std::max(1e-6, hypot2(ax, ay));
                                Projectile p;
                                p.x = e.x;
                                p.y = e.y - 8;
                                p.vx = ax / ad * 2.3;
                                p.vy = ay / ad * 2.3;
                                p.dmg = e.dmg;
                                p.life = 120;
                                p.kind = Projectile::Kind::Bolt;
                                m.proj.push_back(p);
                                play("shoot");
                            }
                        } else if (d < 56) chase(e, -dx, -dy, d, def.speed * sm);
                        else if (e.act <= 0 && e.t % 20 == 0 && rng.chance(0.35)) {
                            e.aim = 40;
                            e.ax = P.x;
                            e.ay = P.y - 6;
                        }
                    } else wander(e, 0.3);
                    break;
                case Ai::Boss: {
                    const size_t before = born.size();
                    updateBoss(e, m, dx, dy, d);
                    if (e.state == Enemy::State::Summon && e.st <= 0) {   // handled here so the enemy list is not resized mid-loop
                        e.summons++;
                        for (int i = 0; i < 2 && !def.boss.summon.empty(); i++) {
                            Enemy b = makeEnemy(db.enemy(def.boss.summon), e.x + std::cos(i * PI) * 30, e.y + 10, false);
                            b.t = 0;
                            b.summoned = true;
                            burst(b.x, b.y - 8, Color(200, 56, 74), 12, 1);
                            born.push_back(b);
                        }
                        play("roar");
                        e.state = Enemy::State::Recover;
                        e.st = 40;
                    }
                    (void)before;
                    break;
                }
            }
        faceMotion(e, e.x - ox, e.y - oy);
        // Soldiers standing still turn on the spot to face you
        if (!e.moving && !stunned && def.look == Look::Human && d < 170 && (!def.isBoss() || e.active)) e.face = dx < 0 ? -1 : 1;

        if (!invulnerable() && e.alive && !def.passive && !calm) {
            const double cd = hypot2(P.x - e.x, (P.y - 5) - (e.y - e.cy));
            if (cd < e.r + 5) {
                const int before = P.hp;
                hurtPlayer(e.dmg, e.x, e.y);
                e.aggroT = 0;
                if (e.elite && e.elite->healOnHit && P.hp < before) e.hp = std::min(e.maxHp, e.hp + e.elite->healOnHit);
            }
        }
    }
    for (auto& b : born) m.enemies.push_back(b);

    // Keep enemies from stacking on top of each other
    auto& es = m.enemies;
    for (size_t i = 0; i < es.size(); i++)
        for (size_t j = i + 1; j < es.size(); j++) {
            Enemy &a = es[i], &b = es[j];
            if (!a.alive || !b.alive || a.def->isBoss() || b.def->isBoss()) continue;
            const double dx = b.x - a.x, dy = b.y - a.y, d = hypot2(dx, dy), min = a.r + b.r - 2;
            if (d > 0 && d < min) {
                const double push = (min - d) / 2 / d;
                m.moveBox(a.x, a.y, -dx * push, -dy * push, a.hw, a.hh, a.def->fly);
                m.moveBox(b.x, b.y, dx * push, dy * push, b.hw, b.hh, b.def->fly);
            }
        }
    std::erase_if(m.enemies, [](const Enemy& e) { return !e.alive; });
}

void Sim::updateBoss(Enemy& e, Map& m, double dx, double dy, double d) {
    using S = Enemy::State;
    const auto& B = e.def->boss;
    if (!e.active) {
        if (m.bossRoom.contains(P.x, P.y)) {
            e.active = true;
            e.state = S::Roar;
            e.st = 70;
            shake = 12;
            play("roar");
            toast(e.def->name + " rises!", Color(255, 154, 122));
            if (!B.roomTitle.empty()) area = AreaTitle{B.roomTitle, e.def->name, 200};
        }
        return;
    }
    e.aggroT = 0;
    const bool phase2 = e.hp <= e.maxHp / 2;
    e.st--;
    auto next = [&](S s, int t) {
        e.state = s;
        e.st = t;
    };
    switch (e.state) {
        case S::Roar:
            if (e.st <= 0) next(S::Chase, 120);
            break;
        case S::Chase: {
            const double sp = B.speed * (phase2 ? 1.3 : 1.0);
            m.moveBox(e.x, e.y, dx / d * sp, dy / d * sp, e.hw, e.hh, false);
            if (e.st <= 0) {
                if (phase2 && !B.summon.empty() && e.summons < 2 && e.hp <= e.maxHp * (e.summons ? 0.25 : 0.5)) next(S::Summon, 40);
                else if (B.volley > 0 && rng.chance(0.5)) next(S::VolleyWind, 40);
                else {
                    next(S::SlamWind, 52);
                    e.slamX = e.x;
                    e.slamY = e.y - 6;
                }
            }
            break;
        }
        case S::SlamWind:
            if (e.st <= 0) {
                if (hypot2(P.x - e.slamX, P.y - e.slamY) < B.slamRadius) hurtPlayer(B.slam, e.slamX, e.slamY);
                shake = 14;
                play("boom");
                for (int i = 0; i < 36; i++) {
                    const double a = i / 36.0 * PI * 2;
                    particles.push_back({e.slamX + std::cos(a) * (B.slamRadius - 6), e.slamY + std::sin(a) * (B.slamRadius - 6), std::cos(a) * 0.6, std::sin(a) * 0.6, 0, 24, 2, Color(184, 168, 216)});
                }
                next(S::Recover, phase2 ? 40 : 55);
            }
            break;
        case S::VolleyWind:
            if (e.st <= 0) {
                fireRing(m, e, B.volley, 0);
                e.wave = phase2 ? 18 : 0;
                next(S::Recover, phase2 ? 50 : 45);
            }
            break;
        case S::Summon: break;   // resolved by updateEnemies when the timer runs out
        case S::Recover:
            if (e.st <= 0) next(S::Chase, phase2 ? 90 : 130);
            break;
        default: next(S::Chase, 60); break;
    }
    if (e.wave > 0 && --e.wave == 0) fireRing(m, e, B.volley, PI / B.volley);
}

void Sim::fireRing(Map& m, const Enemy& e, int n, double off) {
    play("shoot");
    for (int i = 0; i < n; i++) {
        const double a = off + static_cast<double>(i) / n * PI * 2;
        Projectile p;
        p.x = e.x;
        p.y = e.y - e.cy;
        p.vx = std::cos(a) * 1.35;
        p.vy = std::sin(a) * 1.35;
        p.dmg = std::max(1, e.dmg - 1);
        p.life = 260;
        p.kind = Projectile::Kind::Knife;
        m.proj.push_back(p);
    }
}

void Sim::updateProjectiles(Map& m) {
    for (auto& p : m.proj) {
        if (p.life <= 0) continue;
        p.x += p.vx;
        p.y += p.vy;
        p.life--;
        const int c = ifloor(p.x / TILE), r = ifloor((p.y + 4) / TILE);
        if (blocksFlight(m.get(c, r)) || (m.inside(c, r) && m.block[m.idx(c, r)])) {
            p.life = 0;
            p.hitSomething = true;
            burst(p.x, p.y, p.kind == Projectile::Kind::Arrow ? Color(200, 173, 120) : Color(236, 230, 216), 5, 0.8);
            if (p.fromPlayer)
                if (Object* o = m.objectAt(c, r); o && o->kind == ObjKind::Pot) breakPot(m, *o);
            continue;
        }
        if (p.fromPlayer) {
            for (auto& e : m.enemies)
                if (e.alive && hypot2(p.x - e.x, p.y - (e.y - e.cy)) < e.r + 2) {
                    damageEnemy(e, p.dmg, p.vx * 0.8, p.vy * 0.8);
                    p.life = 0;
                    p.hitSomething = true;
                    break;
                }
        } else if (!invulnerable() && hypot2(p.x - P.x, p.y - (P.y - 6)) < 6) {
            hurtPlayer(p.dmg, p.x - p.vx * 4, p.y - p.vy * 4);
            p.life = 0;
            p.hitSomething = true;
        }
    }
    if (m.outdoor)
        for (const auto& p : m.proj)
            if (p.life <= 0 && !p.hitSomething && m.get(ifloor(p.x / TILE), ifloor((p.y + 4) / TILE)) == WATER) waterFx(m, p.x, p.y + 4, true);
    std::erase_if(m.proj, [](const Projectile& p) { return p.life <= 0; });
}

void Sim::updateDrops(Map& m) {
    for (auto& d : m.drops) {
        d.t++;
        d.vz -= 0.22;
        d.z += d.vz;
        if (d.z <= 0) {
            d.z = 0;
            d.vz = std::abs(d.vz) > 0.8 ? -d.vz * 0.45 : 0;
        }
        d.x += d.vx;
        d.y += d.vy;
        d.vx *= 0.9;
        d.vy *= 0.9;
        const double dx = P.x - d.x, dy = (P.y - 4) - d.y, dist = std::max(1e-6, hypot2(dx, dy));
        if (d.tossed) {   // thrown away: stays put until the hero has stepped off, then is ordinary loot again
            if (dist > 30) {
                d.tossed = false;
                d.t = 0;
            }
            continue;
        }
        if (flying()) continue;
        const bool coin = d.item.id == "gold" || d.globe;
        if (!d.globe) {   // a full bag leaves loot on the ground instead of nagging every frame
            const ItemDef& idef = db.item(d.item.id);
            const bool fits = !bagFull() || idef.type == ItemType::Special || (idef.stacks() && hasItem(d.item.id));
            if (!fits) {
                if (d.t > 24 && dist < 10 && d.t % 150 == 25) toast("Your bag is full. Open it and throw something away.");
                continue;
            }
        }
        if (d.globe && P.hp >= maxHp()) continue;   // globes wait until you need them
        if (d.t > 24 && dist < (coin ? 60 : 34)) {
            d.x += dx / dist * (coin ? 2.6 : 2);
            d.y += dy / dist * (coin ? 2.6 : 2);
        }
        if (d.t > 24 && dist < 8) {
            if (d.globe) {
                const int h = std::min(4, maxHp() - P.hp);
                P.hp += h;
                floater(P.x, P.y - 20, "+" + std::to_string(h / 2) + (h % 2 ? ".5" : ""), Color(123, 224, 138));
                play("heal");
                d.gone = true;
                continue;
            }
            const ItemDef& def = db.item(d.item.id);
            const std::string name = itemName(d.item);
            const int rarity = d.item.rarity, qty = d.item.qty;
            if (addItem(d.item)) {
                d.gone = true;
                play(d.item.id == "gold" ? "coin" : rarity >= 3 ? "legendary" : rarity >= 1 ? "gem" : "pick");
                if (d.item.id != "heart" && d.item.id != "heart_shard") toast((def.isGear() ? std::string("Found: ") : "+" + std::to_string(qty) + " ") + name, def.isGear() && rarity ? db.rarities[rarity].glow : Color(236, 230, 216));
            }
        }
    }
    std::erase_if(m.drops, [](const Drop& d) { return d.gone; });
}

void Sim::updateTraps(Map& m) {
    for (auto& o : m.objects) {
        if (o.kind != ObjKind::Trap) continue;
        const double cx = o.c * TILE + 8, top = (o.r + 1) * TILE;
        if (o.charge > 0) {
            if (--o.charge == 0) {
                Projectile p;
                p.x = cx;
                p.y = top + 2;
                p.vy = 2.4;
                p.dmg = m.dungeon ? std::max(1, m.dungeon->level / 4) : 1;
                p.life = 90;
                p.kind = Projectile::Kind::Bolt;
                m.proj.push_back(p);
                play("trap");
                o.t = 90;
            }
        } else if (o.t > 0) o.t--;
        else if (std::abs(P.x - cx) < 40 && P.y > top && P.y - top < 110) o.charge = 26;   // you are in its lane: it clicks and glows
    }
}

}   // namespace hc

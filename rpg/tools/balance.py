"""Rebalances monster health and experience, and weapon damage, from their level (WoW / Diablo style).

  python tools/balance.py

The hero's expected damage per blow at level L, with gear of that level and a fair share of skill points:
    P(L) = 2 + 0.29 * L
Monsters take a set number of such blows (by how they fight) and give 45 + 5 * level experience (WoW's
same-level kill), scaled for harmless animals, fliers, mimics and bosses. They hit for 1 + 0.085 * level half
hearts (chargers and bosses harder, archers and fliers softer). Weapons deal 2 + 0.12 * level.
"""
import json
import os
import re

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'config')

def strip(text):
    return re.sub(r'^\s*//.*$', '', text, flags=re.M)

def P(level):
    return 2 + 0.29 * level

HITS = {'chaser': 3.5, 'guard': 3.8, 'charger': 4.0, 'archer': 2.8, 'flier': 2.2, 'flee': 1.5, 'boss': 40}

def main():
    path = os.path.join(ROOT, 'enemies.json')
    text = open(path, encoding='utf-8').read()
    data = json.loads(strip(text))
    by_id = {e['id']: e for e in data['enemies']}
    def get(e, k, default=None):
        while e is not None:
            if k in e:
                return e[k]
            e = by_id.get(e.get('base')) if e.get('base') else None
        return default
    for e in data['enemies']:
        level, ai = get(e, 'level', 1), get(e, 'ai', 'chaser')
        passive, boss = get(e, 'passive', False), ai == 'boss'
        final = boss and get(e, 'boss', {}).get('final', False)
        hits = 60 if final else HITS.get(ai, 3.5)
        if passive: hits = 1.5
        if get(e, 'look') == 'mimic': hits = 6
        hp = max(2, round(hits * P(level)))
        # damage grows with level too: half a heart at the start, four and a half hearts at the top
        hit = 1 + 0.085 * level
        hit *= 0 if passive else 1.3 if ai == 'charger' else 0.8 if ai in ('archer', 'flier') else 1.25 if boss else 1
        dmg = 0 if passive else max(1, round(hit))
        xp = (5 * level + 45) * (0.3 if passive else 30 if final else 15 if boss else 2 if get(e, 'look') == 'mimic' else 0.8 if ai == 'flier' else 1)
        block = re.search(r'\{ "id": "%s",.*?(?=\n    \{ "id":|\n  \])' % re.escape(e['id']), text, flags=re.S)
        b = block.group(0)
        nb = re.sub(r'"hp": \d+', '"hp": %d' % hp, b, count=1) if '"hp"' in b else b.replace('"level":', '"hp": %d, "level":' % hp, 1)
        if not passive:
            nb = re.sub(r'"dmg": \d+', '"dmg": %d' % dmg, nb, count=1) if '"dmg"' in nb else nb.replace('"level":', '"dmg": %d, "level":' % dmg, 1)
        if boss:   # the slam lands harder than a blow
            nb = re.sub(r'"slam": \d+', '"slam": %d' % round(dmg * 1.6), nb, count=1)
        nb = re.sub(r'"xp": \d+', '"xp": %d' % round(xp), nb, count=1) if '"xp"' in nb else nb.replace('"level":', '"xp": %d, "level":' % round(xp), 1)
        text = text[:block.start()] + nb + text[block.end():]
    open(path, 'w', encoding='utf-8', newline='').write(text)

    path = os.path.join(ROOT, 'items.json')
    text = open(path, encoding='utf-8').read()
    def weapon(m):
        line = m.group(0)
        level = int(re.search(r'"level": (\d+)', line).group(1))
        return re.sub(r'"dmg": \d+', '"dmg": %d' % round(2 + 0.12 * level), line, count=1)
    text = re.sub(r'\{ "id": "\w+",[^\n]*"type": "weapon"[^\n]*', weapon, text)
    open(path, 'w', encoding='utf-8', newline='').write(text)

if __name__ == '__main__':
    main()

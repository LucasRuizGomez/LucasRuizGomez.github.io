"""Adds the wider range of gear to config/items.json and packs their Dungeon Crawl icons.

  python tools/add_gear.py <dcss_dir>      (the unpacked "Dungeon Crawl Stone Soup Full", which holds item/)

Writes assets/generated/item_icons.png (32x32 each) and the matching "icon.<id>" entries in config/sprites.json.
Weapon icons lean like the Dungeon Crawl art (blade to the upper right); "diag": true tells the game to stand them up.
Run tools/balance.py afterwards to set weapon damage from level and speed.
"""
import json
import os
import re
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
CONFIG = os.path.join(HERE, '..', 'config')

# id, name, type, level, icon under item/, extra fields, realm (or ''), desc
GEAR = [
    ('rusty_dagger', 'Rusted Dagger', 'weapon', 3, 'weapon/dagger_old', {'reach': 11, 'cd': 14}, '', 'Quick, short and pitted with rust.'),
    ('hand_axe', 'Woodsman\'s Axe', 'weapon', 12, 'weapon/hand_axe_1_new', {'reach': 14, 'cd': 24}, '', 'Made for timber. Works on other things.'),
    ('falchion', 'Falchion', 'weapon', 20, 'weapon/falchion_2_new', {'reach': 15, 'cd': 20}, '', 'A heavy single edge, good for hacking.'),
    ('war_mace', 'Flanged Mace', 'weapon', 28, 'weapon/mace_2_new', {'reach': 14, 'cd': 26}, '', 'Slow, and it does not care about armour.'),
    ('bearded_axe', 'Bearded Axe', 'weapon', 36, 'weapon/broad_axe_2', {'reach': 15, 'cd': 24}, '', 'The hook of its beard pulls shields aside.'),
    ('morning_star', 'Morning Star', 'weapon', 44, 'weapon/eveningstar_2_new', {'reach': 16, 'cd': 24}, '', 'A ball of spikes on a chain.'),
    ('bastard_sword', 'Bastard Sword', 'weapon', 52, 'weapon/long_sword_3', {'reach': 16, 'cd': 20}, '', 'A hand and a half of good steel.'),
    ('halberd', 'Halberd', 'weapon', 60, 'weapon/halberd_1', {'reach': 21, 'cd': 26}, '', 'Axe, spike and hook on a long haft.'),
    ('headsman_axe', 'Headsman\'s Axe', 'weapon', 68, 'weapon/executioner_axe_2_new', {'reach': 17, 'cd': 30}, '', 'Made for one blow. It rarely needs two.'),
    ('greatsword', 'Greatsword', 'weapon', 76, 'weapon/greatsword_2', {'reach': 19, 'cd': 28}, '', 'Taller than a boy and twice as heavy.'),
    ('plague_cleaver', 'Plague Cleaver', 'weapon', 78, 'weapon/executioner_axe_5', {'reach': 17, 'cd': 26}, 'blight', 'Still wet with something green. Found only in the Blighted Fields.'),
    ('bone_reaver', 'Bone Reaver', 'weapon', 84, 'weapon/bardiche_4', {'reach': 21, 'cd': 26}, 'bonewaste', 'A blade of fused bone on a spine. Found only in the Bone Wastes.'),
    ('tidecaller', 'Tidecaller', 'weapon', 88, 'weapon/demon_trident', {'reach': 21, 'cd': 24}, 'sunken', 'It hums when the sea is near. Found only on the Sunken Isle.'),
    ('briarblade', 'Briarblade', 'weapon', 92, 'weapon/katana_2', {'reach': 16, 'cd': 18}, 'blackmire', 'Thorns grow along its spine. Found only in Blackmire Weald.'),
    ('ashbringer', 'Ashbringer', 'weapon', 97, 'weapon/claymore_blessed', {'reach': 19, 'cd': 24}, 'ashen', 'Warm to the touch, always. Found only in the Ashen Reach.'),
    ('gambeson', 'Padded Gambeson', 'armor', 6, 'armor/torso/animal_skin_1_new', {'hp': 1}, '', 'Quilted cloth. Better than a shirt.'),
    ('ring_mail', 'Ring Mail', 'armor', 22, 'armor/torso/ring_mail_1_new', {'hp': 2}, '', 'Iron rings sewn on leather.'),
    ('scale_mail', 'Scale Mail', 'armor', 38, 'armor/torso/elven_scalemail', {'hp': 2, 'def': 1}, '', 'Overlapping scales of steel.'),
    ('banded_mail', 'Banded Mail', 'armor', 54, 'armor/torso/banded_mail_1', {'hp': 3}, '', 'Bands of steel over mail.'),
    ('black_plate', 'Blackened Plate', 'armor', 70, 'armor/torso/orcish_platemail', {'hp': 4}, '', 'Plate burnt black in some old fire.'),
    ('ossuary_plate', 'Ossuary Plate', 'armor', 85, 'armor/torso/plate_mail_2', {'hp': 5}, 'bonewaste', 'Bone and iron fused together. Found only in the Bone Wastes.'),
    ('drowned_mail', 'Drowned Mail', 'armor', 89, 'armor/torso/quicksilver_dragon_scale_mail', {'hp': 5, 'def': 1}, 'sunken', 'Scales that never dry. Found only on the Sunken Isle.'),
    ('kettle_hat', 'Kettle Hat', 'helm', 14, 'armor/headgear/helmet_1', {'hp': 1}, '', 'A brimmed iron pot.'),
    ('sallet', 'Sallet', 'helm', 34, 'armor/headgear/helmet_2_etched', {'hp': 1, 'def': 1}, '', 'A sweeping helm with an etched brow.'),
    ('visored_helm', 'Visored Helm', 'helm', 58, 'armor/headgear/helmet_1_visored', {'hp': 2}, '', 'A slit to see through, and nothing more.'),
    ('horned_helm', 'Horned Helm', 'helm', 72, 'armor/headgear/helmet_4_visor', {'hp': 2, 'def': 1}, '', 'Horns for show, steel for keeping.'),
    ('plague_mask', 'Plague Mask', 'helm', 79, 'armor/headgear/green_mask', {'hp': 2, 'def': 1}, 'blight', 'The beak is stuffed with herbs. Found only in the Blighted Fields.'),
    ('ashen_crown', 'Ashen Crown', 'helm', 96, 'armor/headgear/helmet_art_2', {'hp': 3, 'def': 1}, 'ashen', 'A crown of grey iron. Found only in the Ashen Reach.'),
    ('riding_boots', 'Riding Boots', 'boots', 18, 'armor/feet/boots_2_jackboots', {'speed': 6}, '', 'Tall boots, good for long roads.'),
    ('sabatons', 'Sabatons', 'boots', 46, 'armor/feet/boots_iron_2', {'speed': 8}, '', 'Plated boots that ring on stone.'),
    ('witchbark_boots', 'Witchbark Boots', 'boots', 91, 'armor/feet/boots_4_green', {'speed': 14}, 'blackmire', 'They never sink in the mire. Found only in Blackmire Weald.'),
    ('round_shield', 'Round Shield', 'shield', 16, 'armor/shields/shield_3_round', {'def': 1}, '', 'Boards and a boss of iron.'),
    ('kite_shield', 'Kite Shield', 'shield', 40, 'armor/shields/shield_2_kite', {'def': 2}, '', 'Long enough to cover a knee.'),
    ('tower_shield', 'Tower Shield', 'shield', 64, 'armor/shields/large_shield_2_new', {'def': 3}, '', 'A wall you carry.'),
    ('coral_buckler', 'Coral Buckler', 'shield', 87, 'armor/shields/elven_buckler_2', {'def': 4}, 'sunken', 'Grown, not forged. Found only on the Sunken Isle.'),
]
DOLL = {'armor': lambda l: 'torso_leather' if l < 20 else 'torso_chain' if l < 50 else 'torso_plate+arms_plate+pauldrons+legs_plate',
        'helm': lambda l: 'barbuta' if l < 50 else 'greathelm', 'boots': lambda l: 'boots' if l < 40 else 'boots_plate', 'shield': lambda l: 'shield_crusader'}


def main(dcss):
    path = os.path.join(CONFIG, 'items.json')
    text = open(path, encoding='utf-8').read()
    lines = []
    for gid, name, typ, level, icon, extra, realm, desc in GEAR:
        if '"id": "%s"' % gid in text:
            continue
        fields = {'id': gid, 'name': name, 'type': typ}
        if typ == 'weapon':
            fields['dmg'] = 1
        fields.update(extra)
        if typ in DOLL:
            fields['doll'] = DOLL[typ](level)
        fields.update({'tier': round(1 + level / 20, 1), 'level': level, 'value': 4 + level * 2})
        if realm:
            fields['realm'] = realm
        fields['desc'] = desc
        lines.append('    ' + json.dumps(fields, ensure_ascii=False))
    if lines:
        anchor = text.index('\n  ],', text.index('"items": ['))
        text = text[:anchor] + ',\n    // the wider range of gear (tools/add_gear.py); "realm": found only on that map\n' + ',\n'.join(lines) + text[anchor:]
        open(path, 'w', encoding='utf-8', newline='').write(text)

    cols = 8
    sheet = Image.new('RGBA', (cols * 32, ((len(GEAR) + cols - 1) // cols) * 32))
    entries = []
    for i, g in enumerate(GEAR):
        im = Image.open(os.path.join(dcss, 'item', g[4] + '.png')).convert('RGBA')
        x, y = (i % cols) * 32, (i // cols) * 32
        sheet.paste(im, (x, y))
        diag = ', "diag": true' if g[2] == 'weapon' else ''
        entries.append('    "icon.%s": { "at": ["itemicons", %d, %d, 32, 32]%s },' % (g[0], x, y, diag))
    sheet.save(os.path.join(HERE, '..', 'assets', 'generated', 'item_icons.png'))
    sp = os.path.join(CONFIG, 'sprites.json')
    s = open(sp, encoding='utf-8').read()
    if '"itemicons"' not in s:
        s = s.replace('"skillicons": "assets/generated/skill_icons.png"', '"skillicons": "assets/generated/skill_icons.png",\n    "itemicons": "assets/generated/item_icons.png"', 1)
    s = re.sub(r'\n    // item icons from Dungeon Crawl \(tools/add_gear.py\)\n(    "icon\.[^\n]*\n)*', '\n', s)
    i = s.index('"sprites": {') + len('"sprites": {')
    s = s[:i] + '\n    // item icons from Dungeon Crawl (tools/add_gear.py)\n' + '\n'.join(entries) + s[i:]
    open(sp, 'w', encoding='utf-8', newline='').write(s)


if __name__ == '__main__':
    main(sys.argv[1])

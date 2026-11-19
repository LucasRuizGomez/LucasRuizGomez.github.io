"""Packs the Dungeon Crawl icons used by the skill tree and the action bar into assets/generated/skill_icons.png
(32x32 each, 8 to a row) and prints the matching sprites.json entries.

  python tools/pack_skill_icons.py <dcss_dir>     (the unpacked "Dungeon Crawl Stone Soup Full", which holds gui/)
"""
import json
import os
import sys

from PIL import Image

ICONS = {  # skill id: icon under gui/
    'strength': 'skills/fighting', 'whirlwind': 'spells/air/tornado', 'warcry': 'spells/enchantment/cause_fear_new',
    'precision': 'spells/enchantment/sure_blade_new', 'cyclone': 'spells/air/airstrike_new', 'leap': 'abilities/jump',
    'bloodlust': 'spells/enchantment/berserker_rage_new', 'warlord': 'skills/long_blades', 'butcher': 'skills/axes',
    'berserker': 'spells/enchantment/berserker_rage_old', 'avatar': 'spells/enchantment/song_of_slaying',
    'vitality': 'spells/necromancy/borgnjors_revivification_new', 'bulwark': 'skills/shields', 'stoneskin': 'spells/earth/stoneskin_new',
    'herbalism': 'spells/necromancy/fulsome_distillation', 'regen': 'spells/enchantment/infusion', 'bastion': 'spells/ice/ozocubus_armor_new',
    'ironhide': 'skills/armor', 'colossus': 'spells/earth/statue_form_new', 'thickskin': 'spells/earth/petrify',
    'undying': 'spells/necromancy/deaths_door_new', 'immortal': 'spells/necromancy/borgnjors_revivification_old',
    'fleetfoot': 'spells/air/swiftness_new', 'marksman': 'skills/crossbows', 'volley': 'spells/earth/iron_shot_new',
    'tumbler': 'skills/dodging', 'sharpshooter': 'skills/bows', 'quickstep': 'spells/enchantment/haste_new',
    'scavenger': 'spells/divination/detect_items', 'windrunner': 'spells/air/flight', 'deadeye': 'spells/conjuration/searing_ray',
    'treasure': 'spells/divination/magic_mapping', 'shadow': 'spells/enchantment/invisibility_new',
}

def main(dcss):
    names = list(ICONS)
    cols = 8
    sheet = Image.new('RGBA', (cols * 32, ((len(names) + cols - 1) // cols) * 32))
    entries = {}
    for i, n in enumerate(names):
        im = Image.open(os.path.join(dcss, 'gui', ICONS[n] + '.png')).convert('RGBA').resize((32, 32), Image.NEAREST)
        x, y = (i % cols) * 32, (i // cols) * 32
        sheet.paste(im, (x, y))
        entries['ic_' + n] = {'at': ['skillicons', x, y, 32, 32]}
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'assets', 'generated', 'skill_icons.png')
    sheet.save(out)
    print(json.dumps(entries))

if __name__ == '__main__':
    main(sys.argv[1])

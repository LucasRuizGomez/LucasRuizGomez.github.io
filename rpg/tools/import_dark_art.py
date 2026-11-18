"""Copies the art the game uses out of the downloaded packs into assets/, ready for the build.

  python tools/import_dark_art.py <undead_dir> <vampire_dir> <dcss_dir>

undead_dir:  the unpacked "Free Undead Tileset Top Down" (CraftPix), the folder that holds PNG/
vampire_dir: the unpacked "Free Vampire 4-Direction Pixel Character Sprite Pack" (CraftPix), the folder that holds PNG/
dcss_dir:    the unpacked "Dungeon Crawl Stone Soup Full" (CC0), the folder that holds monster/

Vampire sheets come as 64x64 frames (rows: down, up, left, right); they are cut down to 32x32 frames around
the figure so they load like the hunt-pack animals.
"""
import os
import shutil
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.join(HERE, '..', 'assets')

UNDEAD = [  # shadow-1 variants only
    'Tree_shadow1_1', 'Tree_shadow1_2', 'Tree_shadow1_3', 'Dead_tree_shadow1_1', 'Dead_tree_shadow1_2', 'Dead_tree_shadow1_3',
    'Broken_tree_shadow1_1', 'Broken_tree_shadow1_4', 'Broken_tree_shadow1_5', 'Broken_tree_shadow1_2', 'Broken_tree_shadow1_3',
    'Rock_shadow1_1', 'Rock_shadow1_2', 'Rock_shadow1_3', 'Rock_shadow1_4', 'Rock_shadow1_5',
    'Ruin_shadow1_1', 'Ruin_shadow1_2', 'Ruin_shadow1_3', 'Ruin_shadow1_4', 'Ruin_shadow1_5',
    'Grave_shadow1_1', 'Grave_shadow1_2', 'Grave_shadow1_3', 'Grave_shadow1_4', 'Grave_shadow1_10', 'Grave_shadow1_11', 'Grave_shadow1_12',
    'Bones_shadow1_1', 'Bones_shadow1_3', 'Bones_shadow1_4', 'Bones_shadow1_5', 'Bones_shadow1_6', 'Bones_shadow1_7', 'Bones_shadow1_15', 'Bones_shadow1_16',
    'Thorn_plant_shadow1_3', 'Thorn_plant_shadow1_4', 'Thorn_plant_shadow1_5', 'Thorn_plant_shadow1_6',
    'Plant_shadow1_2', 'Plant_shadow1_3', 'Plant_shadow1_4', 'Plant_shadow1_5',
    'Dead_arm_shadow1_3', 'Dead_arm_shadow1_4', 'Pile_sculls_shadow1', 'Scull_door_shadow1', 'Crystal_shadow1_3',
]

DCSS = {  # game name: path under monster/
    'rat': 'animals/grey_rat', 'jackal': 'animals/jackal_new', 'raven': 'raven', 'cultist': 'necromancer_new',
    'graverobber': 'human_new', 'corpse': 'undead/zombies/zombie_small', 'spider': 'animals/spider', 'bat': 'animals/giant_bat',
    'wight': 'undead/wight_new', 'drowned_soul': 'undead/drowned_soul', 'ghoul': 'undead/ghoul', 'bog_body': 'undead/bog_body',
    'leech': 'animals/giant_leech_new', 'bloodfly': 'animals/giant_mosquito', 'frost_wraith': 'undead/freezing_wraith',
    'barrow_knight': 'undead/skeletal_warrior_new', 'ice_beast': 'animals/ice_beast', 'shadow_wraith': 'undead/shadow_wraith',
    'mummy': 'undead/mummy', 'skull': 'undead/flying_skull', 'phantom': 'undead/phantasmal_warrior', 'revenant': 'undead/revenant',
    'hound': 'animals/hound', 'skeleton': 'undead/skeletons/skeleton_humanoid_small_new', 'zombie_hound': 'undead/zombies/zombie_hound',
}

VAMP_ANIMS = {'Idle': 'idle', 'Walk': 'walk', 'Run': 'run', 'Attack': 'attack', 'Death': 'dead', 'Hurt': 'hurt'}


def main(undead, vamp, dcss):
    out = os.path.join(ASSETS, 'undead')
    os.makedirs(out, exist_ok=True)
    for name in UNDEAD:
        src = os.path.join(undead, 'PNG', 'Objects_separately', name + '.png')
        shutil.copy(src, os.path.join(out, name.replace('_shadow1', '').lower() + '.png'))
    out = os.path.join(ASSETS, 'dcss')
    os.makedirs(out, exist_ok=True)
    for name, path in DCSS.items():
        shutil.copy(os.path.join(dcss, 'monster', path + '.png'), os.path.join(out, name + '.png'))
    out = os.path.join(ASSETS, 'generated', 'vampires')
    os.makedirs(out, exist_ok=True)
    for v in (1, 2, 3):
        for src_name, key in VAMP_ANIMS.items():
            path = os.path.join(vamp, 'PNG', f'Vampires{v}', src_name, f'Vampires{v}_{src_name}_full.png')
            if not os.path.exists(path):
                continue
            sheet = Image.open(path).convert('RGBA')
            cols = sheet.width // 64
            cut = Image.new('RGBA', (cols * 32, 4 * 32))
            for r in range(4):
                for c in range(cols):
                    # the figure stands in the middle of its 64x64 frame, feet near y 44
                    cut.paste(sheet.crop((c * 64 + 16, r * 64 + 16, c * 64 + 48, r * 64 + 48)), (c * 32, r * 32))
            cut.save(os.path.join(out, f'vampire{v}.{key}.png'))


if __name__ == '__main__':
    main(*sys.argv[1:4])

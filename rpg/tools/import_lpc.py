"""Builds the hero's paper-doll layers from the Universal LPC Spritesheet Character Generator (Liberated Pixel Cup).

  python tools/import_lpc.py

Downloads each layer the game dresses the hero in, from the generator's GitHub repository, and writes
assets/lpc/<layer>.<anim>.png: rows down, up, left, right (LPC stores up, left, down, right), frames cut from
64x64 to 40x40 so the knight stands under two tiles tall. Greyscale layers (cape, cloth) are dyed here.
The authors and licences of every file used are written to assets/lpc/CREDITS.csv (from the generator's CREDITS.csv).
"""
import csv
import io
import os
import sys
import urllib.request

from PIL import Image

REPO = 'https://raw.githubusercontent.com/LiberatedPixelCup/Universal-LPC-Spritesheet-Character-Generator/master/'
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'assets', 'lpc')
ANIMS = ['walk', 'slash', 'hurt', 'idle', 'shoot']
FRAME, SIZE = 64, 40

LAYERS = {  # game layer: (path under spritesheets/, dye or None)
    'body': ('body/bodies/male', None),
    'head': ('head/heads/human/male', None),
    'pants': ('legs/pants/male', '#4a3c32'),
    'shirt': ('torso/clothes/longsleeve/longsleeve/male', '#3a3840'),
    'boots': ('feet/boots/basic/male', '#4a382a'),
    'boots_plate': ('feet/armour/plate/male', None),
    'legs_plate': ('legs/armour/plate/male', None),
    'torso_leather': ('torso/armour/leather/male', None),
    'torso_chain': ('torso/chainmail/male', None),
    'torso_plate': ('torso/armour/plate/male', None),
    'arms_plate': ('arms/armour/plate/male', None),
    'pauldrons': ('shoulders/pauldrons/male', None),
    'hood': ('hat/cloth/hood/adult', '#3c3a3e'),
    'leather_cap': ('hat/cloth/leather_cap/adult', None),
    'barbuta': ('hat/helmet/barbuta/male', None),
    'greathelm': ('hat/helmet/greathelm/male', None),
    'cape_bg': ('cape/solid/bg', '#2a2426'),
    'cape_fg': ('cape/solid/fg', '#2a2426'),
    'shield_kite': ('shield/kite/male', None),
    'shield_crusader': ('shield/crusader/fg', None),
}


def fetch(url):
    try:
        with urllib.request.urlopen(url, timeout=30) as r:
            return r.read()
    except Exception:
        return None


def dye(im, hexcol):
    r0, g0, b0 = int(hexcol[1:3], 16), int(hexcol[3:5], 16), int(hexcol[5:7], 16)
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            if a:
                lum = (0.3 * r + 0.59 * g + 0.11 * b) / 255
                k = 0.45 + 1.1 * lum   # keep the shading
                px[x, y] = (min(255, int(r0 * k)), min(255, int(g0 * k)), min(255, int(b0 * k)), a)
    return im


def convert(sheet):
    cols = sheet.width // FRAME
    rows = sheet.height // FRAME
    order = [2, 0, 1, 3] if rows >= 4 else [0, 0, 0, 0]   # down, up, left, right
    out = Image.new('RGBA', (cols * SIZE, 4 * SIZE))
    for i, src in enumerate(order):
        for c in range(cols):
            f = sheet.crop((c * FRAME, src * FRAME, (c + 1) * FRAME, (src + 1) * FRAME)).resize((SIZE, SIZE), Image.LANCZOS)
            a = f.getchannel('A').point(lambda v: 255 if v > 110 else 0)
            f.putalpha(a)
            out.paste(f, (c * SIZE, i * SIZE))
    return out


def main():
    os.makedirs(OUT, exist_ok=True)
    used = []
    for name, (path, color) in LAYERS.items():
        for anim in ANIMS:
            data = fetch(REPO + 'spritesheets/' + path + '/' + anim + '.png')
            if not data:
                print('missing', path, anim)
                continue
            sheet = Image.open(io.BytesIO(data)).convert('RGBA')
            if color:
                sheet = dye(sheet, color)
            convert(sheet).save(os.path.join(OUT, f'{name}.{anim}.png'))
            used.append('spritesheets/' + path + '/' + anim + '.png')
    credits = fetch(REPO + 'CREDITS.csv')
    if credits:
        rows = list(csv.reader(io.StringIO(credits.decode('utf-8', 'replace'))))
        keep = [rows[0]] + [r for r in rows[1:] if r and any(r[0] in u or u.startswith(r[0].rstrip('/')) for u in used)]
        with open(os.path.join(OUT, 'CREDITS.csv'), 'w', newline='', encoding='utf-8') as f:
            csv.writer(f).writerows(keep)
        print(len(keep) - 1, 'credit lines')


if __name__ == '__main__':
    main()

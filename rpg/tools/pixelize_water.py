"""Turn the high-resolution water effects from the craftpix water pack into small pixel-art strips.

The pack (craftpix-net-523429-free-water-effects-sprite-pack) ships smooth, vector-style animations at up to
1800x1200 per frame. The game is 16 px pixel art, so each effect is cropped to the area it uses across all its
frames, scaled down to a few tiles, its alpha snapped to on/off and its colours mapped onto the game's water
palette. Output: one horizontal strip per effect in rpg/assets/generated/ (frames left to right).

Run from the repository root:  python rpg/tools/pixelize_water.py
"""
import glob
import os
from PIL import Image

ROOT = os.path.join(os.path.dirname(__file__), '..', 'assets')
SRC = os.path.join(ROOT, 'craftpix-net-523429-free-water-effects-sprite-pack')
OUT = os.path.join(ROOT, 'generated')

# name: (frame glob, target width in game pixels, frame step)
EFFECTS = {
    'water_ripple':    ('Water3/Png/*.png', 30, 1),   # a drop falls in and rings spread out
    'water_splash':    ('Water4/Png/*.png', 26, 1),   # crown of water thrown up when something falls in
    'water_whirlpool': ('Water1/Png/*.png', 40, 3),   # slow swirl, used once in the big lake
}
# Shades of the game's water (drawTileTo, T.WATER is #3c7fa6). The effect never uses that exact blue,
# so every pixel of it stands out: darker troughs, lighter foam, white highlights.
PALETTE = [(42, 88, 120), (143, 208, 224), (232, 248, 255)]


def nearest(rgb):
    lum = 0.3 * rgb[0] + 0.59 * rgb[1] + 0.11 * rgb[2]
    return PALETTE[0 if lum < 120 else 1 if lum < 200 else 2]


def build(name, pattern, width, step):
    files = sorted(glob.glob(os.path.join(SRC, pattern)))[::step]
    frames = [Image.open(f).convert('RGBA') for f in files]
    box = None
    for im in frames:
        b = im.getchannel('A').point(lambda a: 255 if a > 40 else 0).getbbox()   # ignore near-invisible haze
        if b:
            box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    scale = width / (box[2] - box[0])
    w, h = width, max(1, round((box[3] - box[1]) * scale))
    strip = Image.new('RGBA', (w * len(frames), h), (0, 0, 0, 0))
    for i, im in enumerate(frames):
        small = im.crop(box).resize((w, h), Image.LANCZOS)
        px = small.load()
        for y in range(h):
            for x in range(w):
                r, g, b, a = px[x, y]
                px[x, y] = (*nearest((r, g, b)), 255) if a >= 110 else (0, 0, 0, 0)
        strip.paste(small, (i * w, 0))
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + '.png')
    strip.save(path)
    print(f'{name}: {len(frames)} frames of {w}x{h} -> {os.path.relpath(path)}')


if __name__ == '__main__':
    for name, (pattern, width, step) in EFFECTS.items():
        build(name, pattern, width, step)

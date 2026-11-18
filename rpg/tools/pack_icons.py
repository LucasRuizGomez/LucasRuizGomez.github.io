"""Packs the OwlishMedia RPG icons (32 x 32 PNGs, one per file) into one sheet for the game.

    python tools/pack_icons.py

Writes assets/generated/owl_icons.png (12 icons to a row, in file-name order) and prints the
[x, y] of each icon, which is what config/sprites.json uses.
"""
import os
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "assets", "owlishmedia-rpg-icons")
OUT = os.path.join(HERE, "..", "assets", "generated", "owl_icons.png")
COLS, CELL = 12, 32

names = sorted(f for f in os.listdir(SRC) if f.lower().endswith(".png"))
rows = (len(names) + COLS - 1) // COLS
sheet = Image.new("RGBA", (COLS * CELL, rows * CELL), (0, 0, 0, 0))
for i, name in enumerate(names):
    im = Image.open(os.path.join(SRC, name)).convert("RGBA")
    x, y = (i % COLS) * CELL, (i // COLS) * CELL
    sheet.alpha_composite(im, (x + (CELL - im.width) // 2, y + (CELL - im.height) // 2))
    print(f"{os.path.splitext(name)[0]:>12}: [{x}, {y}]")
sheet.save(OUT)

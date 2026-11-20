"""Draws the Wortschatz icons (two stacked flashcards on blue). Run: python make_icons.py"""
from pathlib import Path
from PIL import Image, ImageDraw

HERE = Path(__file__).parent
BLUE, BACK, FRONT, LINE = (47, 111, 222), (214, 226, 248), (255, 255, 255), (47, 111, 222)


def draw(size: int, maskable: bool = False, rounded: bool = True) -> Image.Image:
    s = 4  # supersample
    W = size * s
    img = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    if rounded and not maskable:
        d.rounded_rectangle([0, 0, W - 1, W - 1], radius=int(W * 0.22), fill=BLUE)
    else:
        d.rectangle([0, 0, W, W], fill=BLUE)
    k = 0.78 if maskable else 1.0  # keep content in the maskable safe zone
    c = W / 2

    def box(x0, y0, x1, y1):
        return [c + (x0 - 0.5) * W * k, c + (y0 - 0.5) * W * k, c + (x1 - 0.5) * W * k, c + (y1 - 0.5) * W * k]

    r = int(W * 0.06 * k)
    d.rounded_rectangle(box(0.30, 0.20, 0.82, 0.62), radius=r, fill=BACK)
    d.rounded_rectangle(box(0.18, 0.34, 0.70, 0.80), radius=r, fill=FRONT)
    lw = max(2, int(W * 0.035 * k))
    for y, x1 in ((0.50, 0.58), (0.60, 0.50), (0.68, 0.54)):
        x_a, y_a, x_b, y_b = box(0.28, y, x1, y)
        d.line([(x_a, y_a), (x_b, y_b)], fill=LINE, width=lw)
    return img.resize((size, size), Image.LANCZOS)


draw(192).save(HERE / "icon-192.png")
draw(512).save(HERE / "icon-512.png")
draw(512, maskable=True).save(HERE / "icon-maskable-512.png")
draw(180, rounded=False).convert("RGB").save(HERE / "apple-touch-icon.png")
print("ok")

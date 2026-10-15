#!/usr/bin/env python3 -I
"""Render the Nyquist launcher-icon design at arbitrary sizes.

Draws a white clock disc with 12 faint tick marks and two thick black hands
showing 10:10, supersampled and downscaled with LANCZOS. All geometry is in
units of the icon edge length, measured from the original 25x25 launcher icon
of the Nyquist watchface (truhanen/pebble-nyquist-watchface).

Usage: python3 -I tools/make_store_icons.py [--bg R,G,B | --transparent] OUT_DIR
Writes icon-144.png and icon-80.png (opaque square by default) to OUT_DIR.
"""
import math
import sys
from PIL import Image, ImageDraw

SS = 16  # supersampling factor
# Geometry as fractions of the icon edge (hub is the disc centre unless offset).
PARAMS = dict(
    disc_r=0.5,          # disc radius
    tick_r0=0.356, tick_r1=0.44, tick_w=0.024, tick_grey=198,
    hub=(0.496, 0.503),
    hour_len=0.21, hour_w=0.139, hour_deg=307.0,
    min_len=0.40, min_w=0.099, min_deg=60.0,
    tail=0.017,
)


def render_disc(size, p=PARAMS, bg=None):
    """Return an RGBA image: disc on `bg` (RGB tuple) or transparent if None."""
    n = size * SS
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = n / 2
    r = p["disc_r"] * n
    d.ellipse([c - r, c - r, c + r - 1, c + r - 1], fill=(255, 255, 255, 255))
    g = int(round(p["tick_grey"]))
    for k in range(12):
        a = math.radians(k * 30)
        ux, uy = math.sin(a), -math.cos(a)
        r0, r1 = p["tick_r0"] * n, p["tick_r1"] * n
        d.line([c + ux * r0, c + uy * r0, c + ux * r1, c + uy * r1],
               fill=(g, g, g, 255), width=max(1, round(p["tick_w"] * n)))
    hx, hy = p["hub"][0] * n, p["hub"][1] * n

    def hand(length, width, deg):
        a = math.radians(deg)
        ux, uy = math.sin(a), -math.cos(a)
        w = width * n
        t = p["tail"] * n
        x0, y0 = hx - ux * t, hy - uy * t
        x1, y1 = hx + ux * length * n, hy + uy * length * n
        d.line([x0, y0, x1, y1], fill=(0, 0, 0, 255), width=round(w))
        for (x, y) in ((x0, y0), (x1, y1)):
            d.ellipse([x - w / 2, y - w / 2, x + w / 2, y + w / 2], fill=(0, 0, 0, 255))

    hand(p["hour_len"], p["hour_w"], p["hour_deg"])
    hand(p["min_len"], p["min_w"], p["min_deg"])
    out = img.resize((size, size), Image.LANCZOS)
    if bg is not None:
        base = Image.new("RGBA", (size, size), bg + (255,))
        base.alpha_composite(out)
        return base.convert("RGB")
    return out


def main(argv):
    bg = (32, 32, 32)
    args = [a for a in argv if not a.startswith("--")]
    for a in argv:
        if a == "--transparent":
            bg = None
        elif a.startswith("--bg="):
            bg = tuple(int(v) for v in a[5:].split(","))
    if len(args) != 1:
        sys.exit(__doc__)
    for s in (144, 80):
        render_disc(s, bg=bg).save(f"{args[0]}/icon-{s}.png")


if __name__ == "__main__":
    main(sys.argv[1:])

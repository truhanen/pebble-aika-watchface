#!/usr/bin/env python3
"""Check the 60 edge ticks in a 200x228 screenshot.

Tick b (b = 0..59) sits at the point where the ray from the screen centre at
b*6 degrees (clockwise from 12 o'clock) meets the screen rectangle. It is 6 px
long along the edge and 4 px deep (24 pixels); see cf_tick_pixels in
src/c/clock_face_calc.h for the exact cell/corner convention, ported here.
Every 5th tick is black; the others are black where Bayer4x4[y%4][x%4] < 3
(3/16 = 18.75%) and white elsewhere. Pixels outside every tick show the face
and are ignored.

Prints the match/mismatch counts and every mismatching pixel (up to 20); exits 1
on a mismatch. With --out FILE, writes a 4x zoomed map: grey = match, red =
mismatch, yellow = neither black nor white, the face (non-tick) pixels as
shot. Usage: check_strip.py shot.png [--out map.png]
"""
import math, sys
from PIL import Image

W, H, S, LEN = 200, 228, 4, 6
CX, CY = W / 2, H / 2
BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def hit(b):
    """(edge, t): edge 0..3 = top/right/bottom/left, t clockwise along it."""
    a = math.radians(b * 6)
    dx, dy = math.sin(a), -math.cos(a)
    tx = ty = 1e30
    if dx > 1e-6: tx = (W - CX) / dx
    elif dx < -1e-6: tx = CX / -dx
    if dy > 1e-6: ty = (H - CY) / dy
    elif dy < -1e-6: ty = CY / -dy
    if ty <= tx:
        x = CX + dx * ty
        return (0, x) if dy < 0 else (2, W - x)
    y = CY + dy * tx
    return (1, y) if dx > 0 else (3, H - y)


def ring_pixel(d, i):
    a, b = W - 1 - 2 * d, H - 1 - 2 * d
    i %= 2 * a + 2 * b
    if i < a: return (d + i, d)
    i -= a
    if i < b: return (W - 1 - d, d + i)
    i -= b
    if i < a: return (W - 1 - d - i, H - 1 - d)
    i -= a
    return (d, H - 1 - d - i)


def tick_pixels(b):
    edge, t = hit(b)
    base = [0, W - 1, W + H - 2, 2 * W + H - 3][edge]
    o = base + math.floor(t + 0.5)
    return [ring_pixel(d, o - (2 * edge + 1) * d - LEN // 2 + j)
            for d in range(S) for j in range(LEN)]


def is_black(b, x, y):
    return b % 5 == 0 or BAYER[y % 4][x % 4] < 3


def main():
    args = sys.argv[1:]
    out_path = args[args.index('--out') + 1] if '--out' in args else None
    im = Image.open(args[0]).convert('RGB')
    out = im.copy()
    ok = bad = 0
    wrong = []
    for b in range(60):
        for x, y in tick_pixels(b):
            px = im.getpixel((x, y))
            expect = is_black(b, x, y)
            if px not in ((0, 0, 0), (255, 255, 255)):
                bad += 1; wrong.append((x, y, b, px)); out.putpixel((x, y), (255, 200, 0))
            elif (px == (0, 0, 0)) == expect:
                ok += 1; out.putpixel((x, y), (150, 150, 150) if expect else (230, 230, 230))
            else:
                bad += 1; wrong.append((x, y, b, px)); out.putpixel((x, y), (220, 0, 0))
    print(f"ticks=60 ok={ok} mismatched={bad}")
    for x, y, b, px in wrong[:20]:
        print(f"  mismatch at ({x},{y}) tick {b}: got {px}")
    if out_path:
        out.resize((W * 4, H * 4), Image.NEAREST).save(out_path)
    sys.exit(1 if bad else 0)


main()

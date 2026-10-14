#!/usr/bin/env python3
"""Check the 60 edge ticks in a 200x228 screenshot.

Tick b (b = 0..59) sits at the point where the ray from the screen centre at
b*6 degrees (clockwise from 12 o'clock) meets the screen rectangle. It is 5 px
long on the outermost row, 4 px deep for 5-minute ticks (b%5==0) and 1 px deep
for 1-minute ticks, its sides following rays from the centre; see cf_tick_pixels in
src/c/clock_face_calc.h for the exact rule, ported here in exact integer arithmetic.
All ticks use the foreground colour (--fg R,G,B, default black 0,0,0).
Every tick pixel's 4-neighbour that is not itself a tick pixel (on screen) is its
1 px shadow and must show the background colour (--bg R,G,B, default white
255,255,255). All other pixels show the face and are ignored.

Prints the match/mismatch counts and every mismatching pixel (up to 20); exits 1
on a mismatch. With --out FILE, writes a 4x zoomed map: grey = match, red =
mismatch, the face (non-tick) pixels as shot.
Usage: check_strip.py shot.png [--out map.png] [--bg R,G,B] [--fg R,G,B]
"""
import math, sys
from PIL import Image

W, H, S, LEN = 200, 228, 4, 5
MINOR_TICK_DEPTH = 1
CX, CY = W / 2, H / 2


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


def perimeter_point(P):
    """Doubled perimeter position -> doubled point on the outer-ring pixel-centre rectangle."""
    A, B = 2 * (W - 1), 2 * (H - 1)
    P %= 2 * A + 2 * B
    if P < A: return (1 + P, 1)
    P -= A
    if P < B: return (2 * W - 1, 1 + P)
    P -= B
    if P < A: return (2 * W - 1 - P, 2 * H - 1)
    P -= A
    return (1, 2 * H - 1 - P)


def tick_pixels(b):
    edge, t = hit(b)
    base = [0, W - 1, W + H - 2, 2 * W + H - 3][edge]
    o = base + math.floor(t + 0.001)
    x1, y1 = perimeter_point(2 * (o - 2) - 1)
    x2, y2 = perimeter_point(2 * (o + 2) + 1)
    x1 -= W; y1 -= H; x2 -= W; y2 -= H
    # Major (5-minute) ticks (b%5==0) are S (4) px deep; minor ticks are MINOR_TICK_DEPTH (1) px deep.
    depth = S if is_major(b) else MINOR_TICK_DEPTH
    out = []
    for d in range(depth):
        start = o - (2 * edge + 1) * d - 12
        for j in range(25):
            x, y = ring_pixel(d, start + j)
            vx, vy = 2 * x + 1 - W, 2 * y + 1 - H
            if x1 * vy - y1 * vx >= 0 and vx * y2 - vy * x2 >= 0:
                out.append((x, y))
    return out


def is_major(b):
    return b % 5 == 0


def main():
    args = sys.argv[1:]
    out_path = args[args.index('--out') + 1] if '--out' in args else None
    bg = tuple(int(v) for v in args[args.index('--bg') + 1].split(',')) if '--bg' in args else (255, 255, 255)
    fg = tuple(int(v) for v in args[args.index('--fg') + 1].split(',')) if '--fg' in args else (0, 0, 0)
    im = Image.open(args[0]).convert('RGB')
    out = im.copy()
    ok = bad = 0
    wrong = []
    owner = {}
    for b in range(60):
        for x, y in tick_pixels(b):
            owner[(x, y)] = b
    shadow = set()
    for (x, y) in owner:
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (x + dx, y + dy)
            if 0 <= n[0] < W and 0 <= n[1] < H and n not in owner:
                shadow.add(n)
    for (x, y), b in owner.items():
        px = im.getpixel((x, y))
        # All ticks use foreground colour (--fg, default black)
        expected_color = fg
        if px == expected_color:
            ok += 1; out.putpixel((x, y), (150, 150, 150))
        else:
            bad += 1; wrong.append((x, y, b, px))
            out.putpixel((x, y), (220, 0, 0))
    for (x, y) in sorted(shadow):
        px = im.getpixel((x, y))
        if px == bg:
            ok += 1; out.putpixel((x, y), (120, 200, 120))
        else:
            bad += 1; wrong.append((x, y, 'shadow', px)); out.putpixel((x, y), (220, 0, 220))
    print(f"ticks=60 shadow={len(shadow)} ok={ok} mismatched={bad}")
    for x, y, b, px in wrong[:20]:
        print(f"  mismatch at ({x},{y}) tick {b}: got {px}")
    if out_path:
        out.resize((W * 4, H * 4), Image.NEAREST).save(out_path)
    sys.exit(1 if bad else 0)


main()

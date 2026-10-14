#!/usr/bin/env python3
"""Check the edge tick strip in a 200x228 screenshot.

Tick b (b = 0..59) is centred on the minute boundary at b*6 degrees: it covers
clockwise angles [b*6 - 3, b*6 + 3) from 12 o'clock, measured from the screen
centre to the pixel centre. Its shade is (b - 1) mod 5: 0 white, 1 ~10% dither,
2 ~20%, 3 ~30%, 4 black. The strip is drawn over everything (the minute
hand included), so every strip pixel must equal its plain expected shade.

Prints the match/mismatch counts and every mismatching pixel (up to --limit 20).
With --out FILE, writes a 4x zoomed map: grey = match, red = mismatch,
yellow = neither black nor white. Usage: check_strip.py shot.png [--out map.png]
Pixels within 0.01 degrees of a block edge are skipped (ambiguous).
"""
import math, sys
from PIL import Image

W, H, S = 200, 228, 4
CX, CY = W // 2, H // 2

def dither10(x, y):
    return (x % 8 == 0 and y % 4 == 0) or ((x + 4) % 8 == 0 and (y + 2) % 4 == 0)

def dither20(x, y):
    return (x % 4 + y % 4 == 0) or ((x + 2) % 4 + (y + 2) % 4 == 0)

def dither30(x, y):
    return (x + y) % 2 == 0 and ((x + y) % 4 != 0 or ((x + y) % 4 == 0 and x % 2 != 0 and y % 2 != 0))

def black(shade, x, y):
    return [False, dither10(x, y), dither20(x, y), dither30(x, y), True][shade]

def strip(x, y):
    return x < S or x >= W - S or y < S or y >= H - S

def tick_of(x, y):
    """Tick index, or None within 0.01 degrees of a block edge."""
    deg = math.degrees(math.atan2(x + 0.5 - CX, -(y + 0.5 - CY))) % 360
    shifted = (deg + 3) % 360
    frac = shifted % 6
    if frac < 0.01 or frac > 5.99:
        return None
    return int(shifted // 6)

def main():
    args = sys.argv[1:]
    out_path = args[args.index('--out') + 1] if '--out' in args else None
    im = Image.open(args[0]).convert('RGB')
    out = Image.new('RGB', (W, H), (255, 255, 255))
    ok = bad = skipped = 0
    wrong = []
    for y in range(H):
        for x in range(W):
            if not strip(x, y):
                out.putpixel((x, y), im.getpixel((x, y)))
                continue
            b = tick_of(x, y)
            if b is None:
                skipped += 1
                out.putpixel((x, y), (0, 0, 255))
                continue
            expect = black((b + 4) % 5, x, y)
            px = im.getpixel((x, y))
            if px not in ((0, 0, 0), (255, 255, 255)):
                bad += 1; wrong.append((x, y, b, px)); out.putpixel((x, y), (255, 200, 0))
            elif (px == (0, 0, 0)) == expect:
                ok += 1; out.putpixel((x, y), (150, 150, 150) if expect else (230, 230, 230))
            else:
                bad += 1; wrong.append((x, y, b, px)); out.putpixel((x, y), (220, 0, 0))
    print(f"ok={ok} mismatched={bad} skipped-edge={skipped}")
    for x, y, b, px in wrong[:20]:
        print(f"  mismatch at ({x},{y}) tick {b}: got {px}")
    if out_path:
        out.resize((W * 4, H * 4), Image.NEAREST).save(out_path)
    sys.exit(1 if bad else 0)

main()

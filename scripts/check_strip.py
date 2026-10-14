#!/usr/bin/env python3
"""Check the edge minute strip in a 200x228 screenshot.

Every strip pixel must equal its expected dither shade, except where the minute
hand covers it, where it must be exactly inverted. Prints mismatch statistics and,
with --out, writes a 4x zoomed map: grey = matches the plain pattern, red =
inverted (hand), yellow = neither black nor white (error).
"""
import math, sys
from PIL import Image

W, H, S = 200, 228, 4
CX, CY = W // 2, H // 2

def dither25(x, y):
    return (x % 4 == 0 and y % 2 == 0) or ((x + 2) % 4 == 0 and (y + 1) % 2 == 0)

def black(shade, x, y):
    return [False, dither25(x, y), (x + y) % 2 == 0, not dither25(x, y), True][shade]

def strip(x, y):
    return x < S or x >= W - S or y < S or y >= H - S

def main():
    im = Image.open(sys.argv[1]).convert('RGB')
    out = Image.new('RGB', (W, H), (255, 255, 255))
    plain = inverted = bad = 0
    for y in range(H):
        for x in range(W):
            if not strip(x, y):
                out.putpixel((x, y), im.getpixel((x, y)))
                continue
            deg = math.degrees(math.atan2(x + 0.5 - CX, -(y + 0.5 - CY))) % 360
            expect = black(int(deg // 6) % 5, x, y)
            px = im.getpixel((x, y))
            if px not in ((0, 0, 0), (255, 255, 255)):
                bad += 1; out.putpixel((x, y), (255, 200, 0)); continue
            if (px == (0, 0, 0)) == expect:
                plain += 1; out.putpixel((x, y), (150, 150, 150) if expect else (230, 230, 230))
            else:
                inverted += 1; out.putpixel((x, y), (220, 0, 0))
    print(f"plain={plain} inverted={inverted} non-bw={bad}")
    if len(sys.argv) > 3 and sys.argv[2] == '--out':
        out.resize((W * 4, H * 4), Image.NEAREST).save(sys.argv[3])

main()

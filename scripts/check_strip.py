#!/usr/bin/env python3
"""Check the 60 edge ticks in a 200x228 screenshot.

Tick b (b = 0..59) sits at the point where the ray from the screen centre at
b*6 degrees (clockwise from 12 o'clock) meets the visible area's boundary: the screen
rectangle with corners rounded to radius --radius (default CF_CORNER_R from
src/c/clock_face_calc.h; 0 = plain rectangle; the physical frame hides the corner
pixels). It is 5 px long on the outermost row (measured along the rounded boundary), 4 px deep for 5-minute ticks (b%5==0) and 1 px deep
for 1-minute ticks, its sides following rays from the centre; see cf_tick_pixels in
src/c/clock_face_calc.h for the exact rule, ported here in exact integer arithmetic.
All ticks use the foreground colour (--fg R,G,B, default black 0,0,0).
Every on-screen pixel within Manhattan distance 2 of a tick pixel (the 12-offset
diamond, CF_SHADOW_R) that is not itself a tick pixel is its 2 px shadow and must show the background colour (--bg R,G,B, default white
255,255,255). All other pixels show the face and are ignored.

Prints the match/mismatch counts and every mismatching pixel (up to 20); exits 1
on a mismatch. With --out FILE, writes a 4x zoomed map: grey = match, red =
mismatch, the face (non-tick) pixels as shot.
Usage: check_strip.py shot.png [--out map.png] [--bg R,G,B] [--fg R,G,B] [--radius N]
"""
import math, sys
from PIL import Image

W, H, S, LEN = 200, 228, 4, 5
MINOR_TICK_DEPTH = 1
SHADOW_R = 2   # CF_SHADOW_R: Manhattan reach of the shadow
CX, CY = W / 2, H / 2
RADIUS = 10   # CF_CORNER_R in src/c/clock_face_calc.h; override with --radius


def read_c_radius():
    """Default radius from the C header, so the two never drift apart."""
    import os, re
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'c', 'clock_face_calc.h')
    try:
        return int(re.search(r'#define\s+CF_CORNER_R\s+(\d+)', open(path).read()).group(1))
    except Exception:
        return RADIUS


def hit(b, r):
    """Point where the b*6 degree ray meets the outer boundary (rounded rectangle, radius r)."""
    a = math.radians(b * 6)
    dx, dy = math.sin(a), -math.cos(a)
    tx = ty = 1e30
    if dx > 1e-6: tx = (W - CX) / dx
    elif dx < -1e-6: tx = CX / -dx
    if dy > 1e-6: ty = (H - CY) / dy
    elif dy < -1e-6: ty = CY / -dy
    t = min(tx, ty)
    x, y = CX + dx * t, CY + dy * t
    if r > 0 and (x < r or x > W - r) and (y < r or y > H - r):
        ax, ay = (r if x < r else W - r), (r if y < r else H - r)
        px, py = CX - ax, CY - ay
        bb = dx * px + dy * py
        t = -bb + math.sqrt(bb * bb - (px * px + py * py - r * r))
        x, y = CX + dx * t, CY + dy * t
    return x, y


def in_band(r, x, y, depth):
    """Pixel centre inside the rounded rect with inset distance in [0, depth) (exact integers)."""
    if not (0 <= x < W and 0 <= y < H): return False
    dx2 = dy2 = 0
    if x < r: dx2 = 2 * x + 1 - 2 * r
    elif x >= W - r: dx2 = 2 * x + 1 - 2 * (W - r)
    if y < r: dy2 = 2 * y + 1 - 2 * r
    elif y >= H - r: dy2 = 2 * y + 1 - 2 * (H - r)
    if dx2 and dy2:
        d, lo = dx2 * dx2 + dy2 * dy2, 2 * r - 2 * depth
        return d <= 4 * r * r and not (lo > 0 and d <= lo * lo)
    return min(x, W - 1 - x, y, H - 1 - y) < depth


class Curve:
    """C: the curve of outer-row pixel centres (rounded rect inset 0.5), walked clockwise
    from the start of the top straight by arc length s."""
    def __init__(self, r):
        self.r = r
        self.a = r if r > 0 else 0.5
        self.rc = self.a - 0.5
        self.lt, self.lr = W - 2 * self.a, H - 2 * self.a
        self.q = math.pi / 2 * self.rc
        self.len = 2 * self.lt + 2 * self.lr + 4 * self.q
        self.seg = [self.lt, self.q, self.lr, self.q, self.lt, self.q, self.lr, self.q]

    def corner(self, k):
        ax = W - self.a if k in (0, 1) else self.a
        ay = H - self.a if k in (1, 2) else self.a
        sx, sy = ((0, -1), (1, 0), (0, 1), (-1, 0))[k]
        return ax, ay, sx, sy

    def point(self, s):
        s %= self.len
        i = 0
        while i < 7 and s >= self.seg[i]:
            s -= self.seg[i]; i += 1
        a = self.a
        if i == 0: return a + s, 0.5
        if i == 2: return W - 0.5, a + s
        if i == 4: return W - a - s, H - 0.5
        if i == 6: return 0.5, H - a - s
        ax, ay, sx, sy = self.corner(i // 2)
        phi = s / self.rc if self.rc > 0 else 0
        co, si = math.cos(phi), math.sin(phi)
        return ax + self.rc * (co * sx - si * sy), ay + self.rc * (co * sy + si * sx)

    def centre_s(self, hx, hy):
        r = self.r
        left, right, top, bot = hx < r, hx > W - r, hy < r, hy > H - r
        if r > 0 and (left or right) and (top or bot):
            k = (0 if top else 1) if right else (2 if bot else 3)
            ax, ay, sx, sy = self.corner(k)
            ux, uy = (hx - ax) / r, (hy - ay) / r
            sinp = min(1.0, max(0.0, ux * -sy + uy * sx))
            start = (self.lt, self.lt + self.q + self.lr, 2 * self.lt + 2 * self.q + self.lr,
                     2 * self.lt + 3 * self.q + 2 * self.lr)[k]
            return start + math.asin(sinp) * self.rc
        d = [hy, W - hx, H - hy, hx]
        edge = d.index(min(d))
        t = (hx, hy, W - hx, H - hy)[edge]
        k = math.floor(t + 0.001)
        start = (0, self.lt + self.q, self.lt + self.q + self.lr + self.q,
                 2 * self.lt + 2 * self.q + self.lr + self.q)[edge]
        return start + k + 0.5 - self.a


def tick_pixels(b, r):
    c = Curve(r)
    hx, hy = hit(b, r)
    sc = c.centre_s(hx, hy)
    fx, fy = c.point(sc)
    def dir16(p):
        return (int(math.floor((p[0] - W / 2) * 16 + 0.5)) if p[0] >= W / 2 else -int(math.floor((W / 2 - p[0]) * 16 + 0.5)),
                int(math.floor((p[1] - H / 2) * 16 + 0.5)) if p[1] >= H / 2 else -int(math.floor((H / 2 - p[1]) * 16 + 0.5)))
    x1, y1 = dir16(c.point(sc - LEN / 2))
    x2, y2 = dir16(c.point(sc + LEN / 2))
    # Major (5-minute) ticks (b%5==0) are S (4) px deep; minor ticks are MINOR_TICK_DEPTH (1) px deep.
    depth = S if is_major(b) else MINOR_TICK_DEPTH
    out = []
    for y in range(int(fy) - 14, int(fy) + 15):
        for x in range(int(fx) - 14, int(fx) + 15):
            if not in_band(r, x, y, depth): continue
            vx, vy = (2 * x + 1 - W) * 8, (2 * y + 1 - H) * 8
            if x1 * vy - y1 * vx >= 0 and vx * y2 - vy * x2 >= 0:
                out.append((x, y))
    return out


def is_major(b):
    return b % 5 == 0


def main():
    args = sys.argv[1:]
    radius = int(args[args.index('--radius') + 1]) if '--radius' in args else read_c_radius()
    out_path = args[args.index('--out') + 1] if '--out' in args else None
    bg = tuple(int(v) for v in args[args.index('--bg') + 1].split(',')) if '--bg' in args else (255, 255, 255)
    fg = tuple(int(v) for v in args[args.index('--fg') + 1].split(',')) if '--fg' in args else (0, 0, 0)
    im = Image.open(args[0]).convert('RGB')
    out = im.copy()
    ok = bad = 0
    wrong = []
    owner = {}
    for b in range(60):
        for x, y in tick_pixels(b, radius):
            owner[(x, y)] = b
    shadow = set()
    for (x, y) in owner:
        for dy in range(-SHADOW_R, SHADOW_R + 1):
            for dx in range(-SHADOW_R, SHADOW_R + 1):
                if not 1 <= abs(dx) + abs(dy) <= SHADOW_R: continue
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

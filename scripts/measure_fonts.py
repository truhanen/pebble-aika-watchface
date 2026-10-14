#!/usr/bin/env python3
"""Measure the real ink extents and glyph coverage of the 17 widget fonts.

Drives the emulator (TEST_HOOKS build, see scripts/emu.sh) through the font
specimen scene (src/c/font_specimen.c), scans the screenshots and GENERATES
  src/c/widget_font_metrics.h     ink metrics + supported characters per font
  docs/widget_font_coverage.md    glyph coverage report

All metrics are in pixels relative to the TOP-LEFT of the text box passed to
graphics_draw_text (graphics_text_layout_get_content_size includes invisible
headroom, which is why they are measured instead).

  scripts/measure_fonts.py            drive the emulator, then generate
  scripts/measure_fonts.py --reuse    re-scan screenshots/test/fonts/*.png only
"""
import os, re, subprocess, sys, time
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS = os.path.join(ROOT, 'screenshots/test/fonts')
EMU = os.path.join(ROOT, 'scripts/emu.sh')

FONTS = [
    ('Gothic 3x-small', 'GOTHIC_14'), ('Gothic 2x-small', 'GOTHIC_14_BOLD'),
    ('Gothic x-small', 'GOTHIC_18_BOLD'), ('Gothic small', 'GOTHIC_24_BOLD'),
    ('Roboto small', 'ROBOTO_CONDENSED_21'), ('Gothic medium', 'GOTHIC_28_BOLD'),
    ('Leco medium', 'LECO_20_BOLD_NUMBERS'), ('Bitham large', 'BITHAM_30_BLACK'),
    ('Droid serif large', 'DROID_SERIF_28_BOLD'), ('Leco large', 'LECO_26_BOLD_NUMBERS_AM_PM'),
    ('Leco x-large', 'LECO_32_BOLD_NUMBERS'), ('Bitham 2x-large', 'BITHAM_42_BOLD'),
    ('Leco 2x-large 36', 'LECO_36_BOLD_NUMBERS'), ('Leco 2x-large 38', 'LECO_38_BOLD_NUMBERS'),
    ('Leco 3x-large', 'LECO_42_NUMBERS'), ('Leco 4x-large', 'LECO_60_NUMBERS_AM_PM'),
    ('Leco 5x-large', 'LECO_60_BOLD_NUMBERS_AM_PM'),
]
NF = len(FONTS)

spec = open(os.path.join(ROOT, 'src/c/font_specimen.h'), encoding='utf-8').read()
CHARS = re.search(r'#define SPECIMEN_CHARS "(.*)"', spec).group(1)
CELL_W = int(re.search(r'SPECIMEN_CELL_W (\d+)', spec).group(1))
CELL_H = int(re.search(r'SPECIMEN_CELL_H (\d+)', spec).group(1))
COLS = int(re.search(r'SPECIMEN_COLS (\d+)', spec).group(1))
PER_PAGE = int(re.search(r'SPECIMEN_PER_PAGE (\d+)', spec).group(1))
PAGES = (len(CHARS) + PER_PAGE - 1) // PER_PAGE
assert len(CHARS) == PAGES * PER_PAGE, (len(CHARS), PER_PAGE)


def drive():
    os.makedirs(SHOTS, exist_ok=True)
    for page in range(PAGES):
        for k in range(NF):
            scene = 1 + k + NF * page
            subprocess.run([EMU, 'int', f'TestScene={scene}'], check=True)
            time.sleep(0.5)
            out = os.path.join(SHOTS, f'{k:02d}_{page}.png')
            for attempt in range(3):
                r = subprocess.run(['pebble', 'screenshot', '--vnc', '--no-open', out],
                                   capture_output=True)
                if r.returncode == 0 and os.path.exists(out):
                    break
            print(f'font {k} page {page}', flush=True)
    subprocess.run([EMU, 'int', 'TestScene=0'], check=True)


def scan_cell(im, i):
    """(x0, y0, x1, y1, bitmap-bytes) of the non-white ink in cell i, or None."""
    cx, cy = (i % COLS) * CELL_W, (i // COLS) * CELL_H
    cell = im.crop((cx, cy, cx + CELL_W, cy + CELL_H)).convert('L')
    bw = cell.point(lambda v: 0 if v >= 128 else 255)
    box = bw.getbbox()
    if box is None:
        return None
    return box + (bw.crop(box).tobytes(),)


def measure():
    cells = {}   # (font, char) -> cell scan
    sentinel = {}
    for k in range(NF):
        for page in range(PAGES):
            path = os.path.join(SHOTS, f'{k:02d}_{page}.png')
            im = Image.open(path).convert('RGB')
            sentinel[k, page] = scan_cell(im, PER_PAGE)
            for i, ch in enumerate(CHARS[page * PER_PAGE:(page + 1) * PER_PAGE]):
                cells[k, ch] = scan_cell(im, i)
    res = []
    for k in range(NF):
        sent = sentinel[k, 0]
        supported = []
        for ch in CHARS:
            c = cells[k, ch]
            if c is None:
                continue
            if sent is not None and c[4] == sent[4] and c[:4] == sent[:4]:
                continue
            supported.append(ch)
        def ext(chars):
            have = [cells[k, ch] for ch in chars if ch in supported]
            if not have:
                return 0, 0
            top = min(c[1] for c in have)
            bot = max(c[3] for c in have)
            return top, bot - top
        m = {'chars': ''.join(supported), 'sentinel': sent}
        m['digit_top'], m['digit_h'] = ext('0123456789')
        m['cap_top'], m['cap_h'] = ext('EFHIKLTXYZ')   # flat/straight capitals only (no Q/J tails, no round overshoot)
        flat = cells[k, '1'] if '1' in supported else cells[k, 'H']   # '1' has a flat foot in every font
        m['base'] = flat[3]
        m['x_top'] = cells[k, 'x'][1] if 'x' in supported else 0
        m['desc_bottom'] = cells[k, 'g'][3] if 'g' in supported else 0
        clipped = [ch for ch in supported if cells[k, ch][2] >= CELL_W]
        m['clipped'] = ''.join(clipped)
        res.append(m)
    # Fallback font for characters a font lacks: among the NON-Leco fonts (Leco has the
    # gaps) that have every character this font lacks and a real cap height, the one
    # whose cap height is closest to this font's digit height.
    leco = {6, 9, 10, 12, 13, 14, 15, 16}
    for k in range(NF):
        lacking = set(CHARS) - set(res[k]['chars'])
        cands = [j for j in range(NF) if j not in leco and j != k and res[j]['cap_h'] > 0
                 and lacking <= set(res[j]['chars'])]
        target = res[k]['digit_h']
        res[k]['fallback'] = min(cands, key=lambda j: (abs(res[j]['cap_h'] - target), j))
    return res


def c_string(s):
    out = ''
    for b in s.encode('utf-8'):
        if b in (0x22, 0x5c):
            out += '\\' + chr(b)
        elif 0x20 <= b < 0x7f:
            out += chr(b)
        else:
            out += '\\%03o' % b
    return '"' + out + '"'


def write_header(res):
    lines = [
        '// GENERATED by scripts/measure_fonts.py -- do not edit.',
        '//',
        '// Ink metrics of the widget fonts (index = widget_fonts.h), in px relative to the',
        '// top-left of the text box given to graphics_draw_text:',
        '//   digit_top/digit_h  ink rows of the digits 0-9 (union)',
        '//   cap_top/cap_h      ink rows of the capitals (0 = font has no capitals)',
        '//   base               first row below the flat glyph bottoms (the baseline)',
        '//   x_top              top ink row of "x"; desc_bottom: one past the bottom of "g"',
        '//   fallback           font index to draw characters missing from this font',
        '//   chars              the characters the font really has (UTF-8, octal escapes)',
        '#pragma once',
        '#include <stdint.h>',
        '',
        'typedef struct {',
        '  int8_t digit_top, digit_h, cap_top, cap_h, base, x_top, desc_bottom;',
        '  uint8_t fallback;',
        '  const char *chars;',
        '} WidgetFontMetrics;',
        '',
        'static const WidgetFontMetrics WIDGET_FONT_METRICS[%d] = {' % NF,
    ]
    for k, m in enumerate(res):
        lines.append('  /* %2d %s (%s) */ { %d, %d, %d, %d, %d, %d, %d, %d,\n      %s },' % (
            k, FONTS[k][0], FONTS[k][1], m['digit_top'], m['digit_h'], m['cap_top'], m['cap_h'],
            m['base'], m['x_top'], m['desc_bottom'], m['fallback'], c_string(m['chars'])))
    lines.append('};')
    open(os.path.join(ROOT, 'src/c/widget_font_metrics.h'), 'w').write('\n'.join(lines) + '\n')


def write_report(res):
    out = ['# Widget font ink metrics and glyph coverage', '',
           'Generated by `scripts/measure_fonts.py` (emery). Offsets are relative to the top',
           'of the text box; the missing-glyph rendering is the specimen sentinel.', '',
           '| # | Font | digit top/h | cap top/h | base | fallback | missing from A-Za-z0-9 and `' +
           ''.join(c for c in CHARS if not c.isalnum()) + '` |',
           '|---|---|---|---|---|---|---|']
    for k, m in enumerate(res):
        missing = ''.join(c for c in CHARS if c not in m['chars'])
        out.append('| %d | %s (%s) | %d/%d | %d/%d | %d | %d | %s |' % (
            k, FONTS[k][0], FONTS[k][1], m['digit_top'], m['digit_h'], m['cap_top'], m['cap_h'],
            m['base'], m['fallback'], ('`%s`' % missing) if missing else 'none'))
        if m['clipped']:
            out.append('|  | WARNING: glyphs touching the cell edge: `%s` | | | | | |' % m['clipped'])
    os.makedirs(os.path.join(ROOT, 'docs'), exist_ok=True)
    open(os.path.join(ROOT, 'docs/widget_font_coverage.md'), 'w').write('\n'.join(out) + '\n')
    print('\n'.join(out))


if __name__ == '__main__':
    if '--reuse' not in sys.argv:
        drive()
    res = measure()
    write_header(res)
    write_report(res)

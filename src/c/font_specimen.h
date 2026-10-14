#pragma once
#include <pebble.h>

// TEST_HOOKS only: TestScene = 1 + font + WIDGET_FONT_COUNT * page draws one page of
// the font specimen that scripts/measure_fonts.py scans. A page is a 5 x 3 grid of
// 40 x 76 px cells: 14 characters of SPECIMEN_CHARS (one per cell, drawn with the
// cell's top-left as the text box origin) plus a sentinel glyph in the last cell,
// which no font has, to recognise missing-glyph rendering.
#define SPECIMEN_CELL_W 40
#define SPECIMEN_CELL_H 76
#define SPECIMEN_COLS 5
#define SPECIMEN_PER_PAGE 14
// 84 characters = 6 pages. Parsed by scripts/measure_fonts.py: keep it one literal.
#define SPECIMEN_CHARS "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz°%.,:-/+@()'*#!?=&;_<>"
#define SPECIMEN_SENTINEL "\xcd\xb8"

void FontSpecimen_draw(GContext *ctx, GRect bounds, int scene);

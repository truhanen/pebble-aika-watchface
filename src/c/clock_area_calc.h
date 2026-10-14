// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#pragma once

// Pure clock-font sizing (no Pebble/FCTX API) so it is host-gcc unit-testable.
// The FCTX glue (glyph measurement + drawing) lives in clock_area.c.
//
// The stacked digital clock derives its em-height from the clock area HEIGHT
// alone (4h/7). When both sidebar columns are active the centre is narrow and a
// height-derived font overflows horizontally onto the columns. This caps the
// em so the two stacked digit lines (hours over minutes) fit within avail_px.
//
// Pass each font's WIDEST digit advance (font units) and units_per_em (same
// fixed-point units — the ratio is scale-free). Using the widest digit (not the
// current time's digits) keeps the size stable across the minute (no resize when
// e.g. "11" gives way to "20"). A line is at most CLOCK_DIGITS_PER_LINE glyphs.
//
// Returns min(height_em, per-line width caps). A non-positive avail_px, advance,
// or upem drops that particular constraint (so a fully degenerate call returns
// height_em unchanged — the pre-existing height-only behaviour).
int ClockArea_fitFontSize(int height_em, int avail_px,
                          int hours_digit_adv, int hours_upem,
                          int minutes_digit_adv, int minutes_upem);

// System-Bitham (GFont) size ladder for the FONT_SETTING_BITHAM clock. Bitham
// exists only at fixed sizes and cannot scale like the vector fonts, so pick the
// largest heavy cut that fits the available per-line height: 42px (BITHAM_42_BOLD,
// the heaviest 42 cut) when it fits, else 30px (BITHAM_30_BLACK, the date-header
// size). Pure logic here; clock_area.c maps the result to the FONT_KEY_* string.
typedef enum { CLOCK_BITHAM_30 = 0, CLOCK_BITHAM_42 = 1 } ClockBithamSize;
ClockBithamSize ClockArea_bithamSize(int avail_px);

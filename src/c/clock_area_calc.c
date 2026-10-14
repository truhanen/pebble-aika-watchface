// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#include "clock_area_calc.h"
#include <limits.h>

// A stacked line (HH / MM) is at most 2 glyphs. Minutes are always 2 digits;
// hours are at most 2. Sizing to 2 (rather than the current time's digit count)
// keeps the font stable minute to minute.
#define CLOCK_DIGITS_PER_LINE 2

// Largest em-height (px) at which CLOCK_DIGITS_PER_LINE glyphs of horizontal
// advance `digit_adv` (font units) fit within `avail_px`:
//   width_px = n * digit_adv * em / upem  <=  avail_px
//   =>  em <= avail_px * upem / (n * digit_adv)
// A non-positive input drops the constraint (INT_MAX = "no limit"). The product
// avail_px*upem stays well within int32 for real screens/fonts (~200 * ~2048).
static int fit_em_for_line(int avail_px, int digit_adv, int upem) {
  if (avail_px <= 0 || digit_adv <= 0 || upem <= 0) { return INT_MAX; }
  return avail_px * upem / (CLOCK_DIGITS_PER_LINE * digit_adv);
}

int ClockArea_fitFontSize(int height_em, int avail_px,
                          int hours_digit_adv, int hours_upem,
                          int minutes_digit_adv, int minutes_upem) {
  int em = height_em;
  int eh = fit_em_for_line(avail_px, hours_digit_adv, hours_upem);
  int em_m = fit_em_for_line(avail_px, minutes_digit_adv, minutes_upem);
  if (eh < em)   { em = eh; }
  if (em_m < em) { em = em_m; }
  return em;
}

ClockBithamSize ClockArea_bithamSize(int avail_px) {
  return avail_px >= 42 ? CLOCK_BITHAM_42 : CLOCK_BITHAM_30;
}

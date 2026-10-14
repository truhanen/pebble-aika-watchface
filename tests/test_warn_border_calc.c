// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
//
// gcc -std=c11 -Wall -I src/c -o /tmp/t tests/test_warn_border_calc.c src/c/warn_border_calc.c
#include "warn_border_calc.h"
#include "battery_days_calc.h"
#include <assert.h>
#include <stdio.h>

// Raw GColor8 argb bytes (a<<6 | r<<4 | g<<2 | b), so this test needs no SDK header.
#define BLACK  0xC0
#define WHITE  0xFF
#define RED    0xF0
#define YELLOW 0xFC
#define DKGRAY 0xD5  /* a3 r1 g1 b1 -- dark, so the guard must pick white */
#define LTGRAY 0xEA  /* a3 r2 g2 b2 -- light, so the guard must pick black */
#define GREEN  0xCC  /* a3 r0 g3 b0 -- saturated green: unweighted sum (3) reads dark and
                         picks white (~1.4:1 contrast); weighted luminance (18) reads light
                         and correctly picks black (~5.6:1) */

int main(void) {
  const int DAYS_5 = 50;      // 5.0 days, in tenths
  const int NONE = BATTERY_DAYS_NONE;

  // ---- both thresholds off: never a battery warning, whatever the battery says
  assert(warn_border_kind(1, false, 1, 0, 0, true, false) == WARN_BORDER_NONE);
  assert(warn_border_kind(1, false, 1, 0, 0, true, true) == WARN_BORDER_NONE);

  // ---- percent threshold only
  assert(warn_border_kind(21, false, DAYS_5, 20, 0, true, false) == WARN_BORDER_NONE);
  assert(warn_border_kind(20, false, DAYS_5, 20, 0, true, false) == WARN_BORDER_BATTERY); // inclusive
  assert(warn_border_kind(5, false, DAYS_5, 20, 0, true, false) == WARN_BORDER_BATTERY);

  // ---- days threshold only (tenths of a day)
  assert(warn_border_kind(90, false, 11, 0, 10, true, false) == WARN_BORDER_NONE);
  assert(warn_border_kind(90, false, 10, 0, 10, true, false) == WARN_BORDER_BATTERY); // inclusive
  assert(warn_border_kind(90, false, 3, 0, 5, true, false) == WARN_BORDER_BATTERY);

  // ---- OR-combined: either threshold alone is enough
  assert(warn_border_kind(15, false, DAYS_5, 20, 5, true, false) == WARN_BORDER_BATTERY);
  assert(warn_border_kind(90, false, 4, 20, 5, true, false) == WARN_BORDER_BATTERY);
  assert(warn_border_kind(90, false, DAYS_5, 20, 5, true, false) == WARN_BORDER_NONE);

  // ---- no estimate yet must NEVER fire the days trigger: BATTERY_DAYS_NONE is -1, and a
  // naive "<= threshold" would read the sentinel as zero days left.
  assert(warn_border_kind(90, false, NONE, 0, 10, true, false) == WARN_BORDER_NONE);
  assert(warn_border_kind(90, false, NONE, 0, 999, true, false) == WARN_BORDER_NONE);
  // ...but the percent trigger still works while the estimate is missing
  assert(warn_border_kind(5, false, NONE, 20, 10, true, false) == WARN_BORDER_BATTERY);

  // ---- charging suppresses the battery warning entirely (both triggers)
  assert(warn_border_kind(3, true, 1, 20, 10, true, false) == WARN_BORDER_NONE);
  // ...but not the BT warning
  assert(warn_border_kind(3, true, 1, 20, 10, false, true) == WARN_BORDER_BT);

  // ---- BT warning
  assert(warn_border_kind(90, false, DAYS_5, 20, 10, false, true) == WARN_BORDER_BT);
  assert(warn_border_kind(90, false, DAYS_5, 20, 10, true, true) == WARN_BORDER_NONE);  // connected
  assert(warn_border_kind(90, false, DAYS_5, 20, 10, false, false) == WARN_BORDER_NONE); // disabled

  // ---- battery WINS when both fire
  assert(warn_border_kind(5, false, 1, 20, 10, false, true) == WARN_BORDER_BATTERY);

  // ---- colour selection
  assert(warn_border_color(WARN_BORDER_BATTERY, RED, YELLOW, BLACK) == RED);
  assert(warn_border_color(WARN_BORDER_BT, RED, YELLOW, BLACK) == YELLOW);
  assert(warn_border_color(WARN_BORDER_NONE, RED, YELLOW, BLACK) == 0);

  // ---- invisibility guard: colour equal to the background is replaced by a contrasting
  // black/white. Without this the frame vanishes on the 1-bit boards (every colour maps
  // to black or white) and whenever the night background matches the warning colour.
  assert(warn_border_color(WARN_BORDER_BATTERY, RED, YELLOW, RED) == WHITE);     // dark red bg -> white
  assert(warn_border_color(WARN_BORDER_BATTERY, WHITE, YELLOW, WHITE) == BLACK); // white bg -> black
  assert(warn_border_color(WARN_BORDER_BATTERY, BLACK, YELLOW, BLACK) == WHITE); // black bg -> white
  assert(warn_border_color(WARN_BORDER_BT, RED, YELLOW, YELLOW) == BLACK);       // light yellow bg -> black
  assert(warn_border_color(WARN_BORDER_BATTERY, DKGRAY, YELLOW, DKGRAY) == WHITE);
  assert(warn_border_color(WARN_BORDER_BATTERY, LTGRAY, YELLOW, LTGRAY) == BLACK);

  // Alpha bits must not defeat the comparison: the same RGB with a different alpha is
  // still the same visible colour, so the guard must still fire.
  assert(warn_border_color(WARN_BORDER_BATTERY, RED, YELLOW, (uint8_t)(RED & 0x3F)) == WHITE);

  // Weighted luminance: a saturated green background with a green-configured warning
  // colour must resolve to BLACK (green reads as light to the eye; an unweighted
  // channel sum wrongly reads it as dark and would pick white -- ~1.4:1 contrast).
  assert(warn_border_color(WARN_BORDER_BATTERY, GREEN, YELLOW, GREEN) == BLACK);

  printf("PASS\n");
  return 0;
}

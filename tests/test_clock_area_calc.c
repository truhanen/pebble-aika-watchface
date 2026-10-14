// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
//
// Host-gcc unit tests for the stacked digital clock font sizing.
//   gcc -I src/c tests/test_clock_area_calc.c src/c/clock_area_calc.c -o t && ./t
//
// Numbers below come from the real font (Avenir): units_per_em=1152, widest
// digit advance=645, measured on the emulator. font_size = 4*h/7.

#include "clock_area_calc.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  const int UPEM = 1152;   // Avenir units_per_em (regular & bold share it here)
  const int ADV  = 645;    // widest digit ('0') horizontal advance, font units

  // --- Two-column overflow (diorite 144px): centre w=84, height font=96. The
  //     widest pair "00" is 107px at em 96 -> must shrink so 2*ADV*em/UPEM<=84.
  //     84*1152/(2*645) = 96768/1290 = 75. ---
  assert(ClockArea_fitFontSize(96, 84, ADV, UPEM, ADV, UPEM) == 75);

  // --- Two-column overflow (emery 200px): centre w=132, height font=130.
  //     132*1152/1290 = 117. ---
  assert(ClockArea_fitFontSize(130, 132, ADV, UPEM, ADV, UPEM) == 117);

  // --- Single column (no cap): centre w=114 on diorite; the pair fits at 96
  //     (107<=114) so the width cap (101) does not bind -> height unchanged. ---
  assert(ClockArea_fitFontSize(96, 114, ADV, UPEM, ADV, UPEM) == 96);

  // --- No sidebars: full width 144, plenty of room -> unchanged. ---
  assert(ClockArea_fitFontSize(96, 144, ADV, UPEM, ADV, UPEM) == 96);

  // --- Different fonts per line (bold hours wider than regular minutes): the
  //     WIDER line governs. hours adv=700 -> 84*1152/(2*700)=69; minutes
  //     adv=500 -> 96 (>=96). min(96,69,96)=69. ---
  assert(ClockArea_fitFontSize(96, 84, 700, UPEM, 500, UPEM) == 69);

  // --- Degenerate guards: each drops only its own constraint. ---
  assert(ClockArea_fitFontSize(96, 0,   ADV, UPEM, ADV, UPEM) == 96);   // avail<=0 -> no cap
  assert(ClockArea_fitFontSize(96, 84,  0,   UPEM, ADV, UPEM) == 75);   // zero hours adv -> minutes govern
  assert(ClockArea_fitFontSize(96, 84,  ADV, 0,    ADV, UPEM) == 75);   // zero hours upem -> minutes govern
  assert(ClockArea_fitFontSize(96, -5,  ADV, UPEM, ADV, UPEM) == 96);   // negative avail -> no cap

  // --- Bitham system-font size ladder (largest heavy cut that fits avail_px).
  //     >=42 -> 42_BOLD; below -> 30_BLACK (the date-header size). ---
  assert(ClockArea_bithamSize(100) == CLOCK_BITHAM_42);  // large clock half on emery
  assert(ClockArea_bithamSize(60)  == CLOCK_BITHAM_42);  // large clock half on diorite
  assert(ClockArea_bithamSize(42)  == CLOCK_BITHAM_42);  // exact boundary fits
  assert(ClockArea_bithamSize(41)  == CLOCK_BITHAM_30);  // just below -> 30
  assert(ClockArea_bithamSize(28)  == CLOCK_BITHAM_30);  // small below-analog band
  assert(ClockArea_bithamSize(0)   == CLOCK_BITHAM_30);  // degenerate -> smallest

  printf("All clock_area_calc tests passed\n");
  return 0;
}

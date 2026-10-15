// SPDX-License-Identifier: GPL-3.0-only
// Compile and run: gcc -std=c11 -Wall -I src/c -o t_rain_calc tests/test_rain_calc.c src/c/rain_calc.c && ./t_rain_calc
#include "rain_calc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void val(uint16_t h, int unit, char sep, const char *want) {
  char b[16];
  rain_format_value(h, unit, sep, b, sizeof b);
  if (strcmp(b, want) != 0) { printf("FAIL value %u unit %d: got '%s' want '%s'\n", h, unit, b, want); }
  assert(strcmp(b, want) == 0);
}
static void unit(int i, int u, const char *want) {
  char b[16];
  rain_format_unit(i, u, b, sizeof b);
  if (strcmp(b, want) != 0) { printf("FAIL unit %d/%d: got '%s' want '%s'\n", i, u, b, want); }
  assert(strcmp(b, want) == 0);
}

int main(void) {
  val(0, 0, '.', "0.0");
  val(140, 0, '.', "1.4");
  val(80, 0, '.', "0.8");
  val(4, 0, '.', "0.0");
  val(5, 0, '.', "0.1");
  val(1234, 0, ',', "12,3");
  val(0xFFFE, 0, '.', "655.3");
  val(0, 1, '.', "0.00");
  val(140, 1, '.', "0.06");     // 1.40 mm = 0.055 in
  val(254, 1, '.', "0.10");     // 2.54 mm = 0.1 in
  val(2540, 1, ',', "1,00");
  val(230, 1, '.', "0.09");
  val(0xFFFF, 0, '.', "--");
  val(0xFFFF, 1, ',', "--");
  unit(0, 0, "mm/h");
  unit(0, 1, "in/h");
  unit(1, 0, "hi mm/3 h");
  unit(2, 1, "hi in/6 h");
  unit(3, 0, "hi mm/9 h");
  unit(4, 0, "hi mm/12 h");
  assert(rain_window_hours(4) == 12 && rain_window_hours(5) == 0 && rain_window_hours(-1) == 0);

  uint8_t d[10] = { 0, 0, 80, 0, 0x8C, 0, 0xE6, 0, 0xFF, 0xFF };
  uint16_t o[5] = { 7, 7, 7, 7, 7 };
  assert(rain_parse(d, 10, o));
  assert(o[0] == 0 && o[1] == 80 && o[2] == 140 && o[3] == 230 && o[4] == RAIN_UNKNOWN);
  uint16_t k[5] = { 7, 7, 7, 7, 7 };
  assert(!rain_parse(d, 9, k) && k[0] == 7);
  assert(!rain_parse(d, 11, k) && !rain_parse(NULL, 10, k));
  printf("--- rain_calc tests passed ---\n");
  return 0;
}

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#include "sleep_calc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  char b[12];

  // --- sleep_format_decimal: hours + truncated tenths, configurable separator ---
  sleep_format_decimal(26580, '.', b, sizeof b); assert(strcmp(b, "7.3") == 0);   // 7h23m
  sleep_format_decimal(23400, ',', b, sizeof b); assert(strcmp(b, "6,5") == 0);   // 6h30m
  sleep_format_decimal(0, ',', b, sizeof b);     assert(strcmp(b, "0,0") == 0);
  sleep_format_decimal(-5, ',', b, sizeof b);    assert(strcmp(b, "0,0") == 0);   // negative clamps
  sleep_format_decimal(3599, ',', b, sizeof b);  assert(strcmp(b, "0,9") == 0);   // truncates, never rounds up
  sleep_format_decimal(36000, ',', b, sizeof b); assert(strcmp(b, "10,0") == 0);  // two-digit hours

  // --- sleep_bar_fill_px: deep share of total, rounded to whole pixels ---
  assert(sleep_bar_fill_px(0, 0, 24) == 0);          // no data
  assert(sleep_bar_fill_px(23400, 0, 24) == 0);      // slept, no deep sleep
  assert(sleep_bar_fill_px(-1, 100, 24) == 0);       // nonsense total
  assert(sleep_bar_fill_px(23400, 6300, 24) == 6);   // 26.9 % of 24 px
  assert(sleep_bar_fill_px(23400, 23400, 24) == 24); // all deep -> full
  assert(sleep_bar_fill_px(23400, 30000, 24) == 24); // deep > total -> clamped, not overflowing
  assert(sleep_bar_fill_px(28800, 60, 24) == 1);     // rounds to 0 but nonzero deep -> 1 px
  assert(sleep_bar_fill_px(23400, 6300, 0) == 0);    // no width -> nothing

  printf("test_sleep_calc: OK\n");
  return 0;
}

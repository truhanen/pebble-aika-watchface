// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
//
// gcc -std=c11 -Wall -I src/c -o /tmp/t tests/test_night_theme_calc.c src/c/night_theme_calc.c
#include "night_theme_calc.h"
#include <assert.h>
#include <stdio.h>

// Raw GColor8 argb bytes, so this test needs no SDK header.
#define BLACK 0xC0
#define WHITE 0xFF
#define MINT  0xD7  /* a stand-in for a configured light panel colour */
#define RED   0xF0
#define CLEAR NIGHT_THEME_ARGB_CLEAR

int main(void) {
  // ---- night INACTIVE: identity, whatever the night palette is
  assert(night_theme_color(MINT, true, false, BLACK, WHITE) == MINT);
  assert(night_theme_color(RED, false, false, BLACK, WHITE) == RED);
  assert(night_theme_color(CLEAR, true, false, BLACK, WHITE) == CLEAR);

  // ---- night ACTIVE: backgrounds take the night background, foregrounds the night
  // foreground, regardless of what was configured
  assert(night_theme_color(MINT, true, true, BLACK, WHITE) == BLACK);
  assert(night_theme_color(WHITE, true, true, BLACK, WHITE) == BLACK);
  assert(night_theme_color(RED, false, true, BLACK, WHITE) == WHITE);
  assert(night_theme_color(BLACK, false, true, BLACK, WHITE) == WHITE);

  // ---- a non-default night palette is honoured (the user picks both colours)
  assert(night_theme_color(MINT, true, true, RED, MINT) == RED);
  assert(night_theme_color(WHITE, false, true, RED, MINT) == MINT);

  // ---- GColorClear survives untouched, night or not. Clear means "inherit" for the
  // status/date/sidebar backgrounds; rewriting it to an opaque night background would
  // break that inheritance, and it does not need rewriting because the colour it
  // inherits FROM is itself night-mapped.
  assert(night_theme_color(CLEAR, true, true, BLACK, WHITE) == CLEAR);
  assert(night_theme_color(CLEAR, false, true, BLACK, WHITE) == CLEAR);

  printf("PASS\n");
  return 0;
}

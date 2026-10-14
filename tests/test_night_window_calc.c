// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#include "night_window_calc.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  // ---- mode OFF: never night, whatever the clock or Quiet Time say
  for (int h = 0; h < 24; h++) {
    assert(night_window_active(NIGHT_WINDOW_OFF, h, 23, 7, false) == false);
    assert(night_window_active(NIGHT_WINDOW_OFF, h, 23, 7, true) == false);
  }

  // ---- mode QUIET_TIME: mirrors the watch's Quiet Time state, ignores the hour fields
  assert(night_window_active(NIGHT_WINDOW_QUIET_TIME, 12, 23, 7, true) == true);
  assert(night_window_active(NIGHT_WINDOW_QUIET_TIME, 2, 23, 7, false) == false);

  // ---- mode CUSTOM: half-open [start, end), wrapping past midnight, ignores Quiet Time
  // 23:00-07:00 -> night at 23, 0, 6; day at 7, 12, 22
  assert(night_window_active(NIGHT_WINDOW_CUSTOM, 23, 23, 7, false) == true);
  assert(night_window_active(NIGHT_WINDOW_CUSTOM, 0, 23, 7, false) == true);
  assert(night_window_active(NIGHT_WINDOW_CUSTOM, 6, 23, 7, false) == true);
  assert(night_window_active(NIGHT_WINDOW_CUSTOM, 7, 23, 7, false) == false);
  assert(night_window_active(NIGHT_WINDOW_CUSTOM, 12, 23, 7, false) == false);
  assert(night_window_active(NIGHT_WINDOW_CUSTOM, 22, 23, 7, false) == false);
  // Quiet Time must not leak into CUSTOM
  assert(night_window_active(NIGHT_WINDOW_CUSTOM, 12, 23, 7, true) == false);

  // non-wrapping window (a daytime "quiet" window is legal, e.g. 09-17)
  assert(night_window_active(NIGHT_WINDOW_CUSTOM, 10, 9, 17, false) == true);
  assert(night_window_active(NIGHT_WINDOW_CUSTOM, 17, 9, 17, false) == false);
  assert(night_window_active(NIGHT_WINDOW_CUSTOM, 8, 9, 17, false) == false);

  // start == end is an EMPTY window, never night -- so a user who sets both the same
  // does not accidentally freeze rotation around the clock
  for (int h = 0; h < 24; h++) {
    assert(night_window_active(NIGHT_WINDOW_CUSTOM, h, 0, 0, false) == false);
    assert(night_window_active(NIGHT_WINDOW_CUSTOM, h, 13, 13, false) == false);
  }

  // an unknown/garbled mode must fail SAFE (rotate normally), never freeze the sidebar
  assert(night_window_active(99, 2, 23, 7, true) == false);
  assert(night_window_active(-1, 2, 23, 7, true) == false);

  // ---- effective interval
  // Night: 0 means "register no timer at all" -- the minute tick still repaints, so
  // rotation continues at 1/min instead of stopping.
  assert(night_window_rotation_interval(5, true) == 0);
  assert(night_window_rotation_interval(30, true) == 0);
  // Day: the configured interval is passed through untouched.
  assert(night_window_rotation_interval(5, false) == 5);
  assert(night_window_rotation_interval(30, false) == 30);
  // "no sub-minute group configured" stays 0 in both cases.
  assert(night_window_rotation_interval(0, false) == 0);
  assert(night_window_rotation_interval(0, true) == 0);

  printf("PASS\n");
  return 0;
}

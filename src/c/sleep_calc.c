// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#include "sleep_calc.h"
#include <stdio.h>

void sleep_format_decimal(int seconds, char sep, char *buf, size_t n) {
  if (seconds < 0) { seconds = 0; }
  int total_tenths = seconds * 10 / 3600;   // truncated tenths of an hour
  snprintf(buf, n, "%d%c%d", total_tenths / 10, sep, total_tenths % 10);
}

int sleep_bar_fill_px(int total_s, int deep_s, int width_px) {
  if (width_px <= 0 || total_s <= 0 || deep_s <= 0) { return 0; }
  if (deep_s >= total_s) { return width_px; }
  int px = (deep_s * width_px + total_s / 2) / total_s;   // rounded to nearest pixel
  if (px < 1) { px = 1; }                                 // nonzero deep is never invisible
  return px;
}

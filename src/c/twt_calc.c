// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include "twt_calc.h"
#include <stdio.h>

int twt_percent(int32_t value, int32_t base) {
  if (base <= 0) return -1;
  if (value < 0) value = 0;
  // rounded: (100*value + base/2) / base
  return (int)(((int64_t)value * 100 + base / 2) / base);
}

int twt_bar_fill_px(int32_t value, int32_t base, int width_px) {
  if (base <= 0 || value <= 0 || width_px <= 0) return 0;
  if (value >= base) return width_px;
  return (int)(((int64_t)value * width_px) / base);
}

void twt_fmt_hhmm_signed(char* buf, size_t bufsize, int32_t minutes) {
  int neg = minutes < 0;
  int32_t mag = neg ? -minutes : minutes;
  snprintf(buf, bufsize, "%s%d:%02d", neg ? "-" : "", (int)(mag / 60), (int)(mag % 60));
}

void twt_fmt_hhmm_remaining(char* buf, size_t bufsize, int32_t remaining) {
  // overtime (remaining < 0) reads as "+h:mm" worked over target; otherwise plain
  int over = remaining < 0;
  int32_t mag = over ? -remaining : remaining;
  snprintf(buf, bufsize, "%s%d:%02d", over ? "+" : "", (int)(mag / 60), (int)(mag % 60));
}

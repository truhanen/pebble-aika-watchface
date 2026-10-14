// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include "warn_border_calc.h"
#include "battery_days_calc.h"

int warn_border_kind(int batteryPct, bool isCharging, int batteryDaysTenths,
                     int warnPct, int warnDaysTenths,
                     bool btConnected, bool btWarnEnabled) {
  bool pctLow = (warnPct > 0) && (batteryPct <= warnPct);
  // BATTERY_DAYS_NONE (-1) means "no learned rate yet", NOT "zero days left".
  bool daysLow = (warnDaysTenths > 0)
      && (batteryDaysTenths != BATTERY_DAYS_NONE)
      && (batteryDaysTenths <= warnDaysTenths);
  if (!isCharging && (pctLow || daysLow)) { return WARN_BORDER_BATTERY; }
  if (btWarnEnabled && !btConnected) { return WARN_BORDER_BT; }
  return WARN_BORDER_NONE;
}

// Weighted luminance over the three 2-bit channels (0..27), mirroring the rule
// twt_status.c's status_update_proc already uses for the same light/dark decision
// (r*3 + g*6 + b, >= 15 = light) -- one rule for this project, not two. An unweighted
// sum picks white on a saturated green (r=0,g=3,b=0: sum 3, "dark") even though green
// reads as light to the eye; weighting green heaviest fixes that.
static bool argb_is_light(uint8_t argb) {
  int r = (argb >> 4) & 0x3;
  int g = (argb >> 2) & 0x3;
  int b = argb & 0x3;
  return (r * 3 + g * 6 + b) >= 15;
}

uint8_t warn_border_color(int kind, uint8_t batteryArgb, uint8_t btArgb,
                          uint8_t effectiveBgArgb) {
  uint8_t c;
  if (kind == WARN_BORDER_BATTERY) { c = batteryArgb; }
  else if (kind == WARN_BORDER_BT) { c = btArgb; }
  else { return 0; }
  // Compare the visible RGB only: the same colour with a different alpha is still the
  // same colour on screen, and the frame would still be invisible.
  if ((c & 0x3F) == (effectiveBgArgb & 0x3F)) {
    return argb_is_light(effectiveBgArgb) ? WARN_BORDER_ARGB_BLACK : WARN_BORDER_ARGB_WHITE;
  }
  return c;
}

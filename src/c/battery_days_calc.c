// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#include "battery_days_calc.h"

void BatteryDays_reset(BatteryDaysState *s) {
  s->learned = 0;
  s->last_t = 0;
  s->last_pct = 0;
  s->have_last = false;
  s->after_charge = false;
}

void BatteryDays_record(BatteryDaysState *s, uint32_t now, uint8_t pct, bool is_charging) {
  if (is_charging) {            // charging: keep learned, re-anchor, arm settling discard
    s->after_charge = true;
    s->have_last = true;
    s->last_t = now;
    s->last_pct = pct;
    return;
  }
  if (!s->have_last) {          // first ever reading: just anchor
    s->have_last = true;
    s->last_t = now;
    s->last_pct = pct;
    return;
  }
  if (pct > s->last_pct) {      // rose while not flagged charging (missed event) -> treat as charge
    s->after_charge = true;
    s->last_t = now;
    s->last_pct = pct;
    return;
  }
  if (pct == s->last_pct) {     // unchanged: keep the anchor so elapsed time accumulates
    return;
  }
  // a drop: pct < last_pct
  uint32_t dt = now - s->last_t;
  uint8_t  dp = (uint8_t)(s->last_pct - pct);
  uint32_t seg_rate = dt / dp;  // seconds per 1% over this segment

  bool fold = true;
  if (s->after_charge) {        // discard the post-charge gauge-settling segment
    fold = false;
    s->after_charge = false;
  }
  if (seg_rate < BATTERY_DAYS_MIN_PLAUSIBLE_SEC_PER_PCT) {  // glitch / residual settling
    fold = false;
  }
  if (fold) {
    if (s->learned == 0) {
      s->learned = seg_rate;    // first real datum replaces the cold-start default
    } else {
      // percent-weighted EWMA step: each percent dropped counts equally, so the
      // average is total_seconds/total_percent (the correct projection rate) over
      // the last ~HORIZON percent. int64 keeps the product exact.
      uint32_t w = (dp < BATTERY_DAYS_EWMA_PCT_HORIZON) ? dp : BATTERY_DAYS_EWMA_PCT_HORIZON;
      int64_t delta = (int64_t)seg_rate - (int64_t)s->learned;
      s->learned = (uint32_t)((int64_t)s->learned
                              + delta * (int64_t)w / (int64_t)BATTERY_DAYS_EWMA_PCT_HORIZON);
    }
    s->learned = BatteryDays_capRate(s->learned);
  }
  s->last_t = now;
  s->last_pct = pct;
}

uint32_t BatteryDays_rate(const BatteryDaysState *s) {
  return s->learned;
}

int BatteryDays_tenthsFromRate(uint8_t pct, uint32_t sec_per_pct) {
  if (sec_per_pct == 0) {
    return BATTERY_DAYS_NONE;
  }
  // days_left = pct * sec_per_pct / 86400 ; tenths = *10, round to nearest.
  // 64-bit: pct*sec_per_pct*10 overflows 32-bit for slow discharge.
  int64_t tenths = ((int64_t)pct * (int64_t)sec_per_pct * 10 + 86400 / 2) / 86400;
  if (tenths < 0) { tenths = 0; }
  if (tenths > BATTERY_DAYS_MAX_TENTHS) { tenths = BATTERY_DAYS_MAX_TENTHS; }
  return (int)tenths;
}

uint32_t BatteryDays_capRate(uint32_t sec_per_pct) {
  // Bigger sec/% = slower discharge = longer life. The default is the ceiling.
  return (sec_per_pct > BATTERY_DAYS_DEFAULT_SEC_PER_PCT)
             ? BATTERY_DAYS_DEFAULT_SEC_PER_PCT
             : sec_per_pct;
}

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include "electricity_calc.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void) {
  // --- elec_format_price (value in 0.01 snt -> "x.x" with one decimal) ---
  char buf[12];
  elec_format_price(523, '.', buf, sizeof(buf));   assert(strcmp(buf, "5.2") == 0);
  elec_format_price(525, '.', buf, sizeof(buf));   assert(strcmp(buf, "5.3") == 0);
  elec_format_price(-30, '.', buf, sizeof(buf));   assert(strcmp(buf, "-0.3") == 0);
  elec_format_price(8000, '.', buf, sizeof(buf));  assert(strcmp(buf, "80.0") == 0);
  elec_format_price(0, ',', buf, sizeof(buf));     assert(strcmp(buf, "0,0") == 0);

  // --- elec_current_index (quarters at startEpoch + i*900) ---
  int idx;
  assert(elec_current_index(1000, 4, 1000, &idx) && idx == 0);
  assert(elec_current_index(1000, 4, 2850, &idx) && idx == 2);
  assert(elec_current_index(1000, 4, 4599, &idx) && idx == 3);
  assert(!elec_current_index(1000, 4, 999, &idx));   // before table
  assert(!elec_current_index(1000, 4, 4600, &idx));  // past table
  assert(!elec_current_index(1000, 0, 1000, &idx));  // empty table

  // --- elec_today_average (quarters at startEpoch + i*900) ---
  int16_t prices[4] = {100, 200, 300, 400};
  int16_t avg;
  assert(elec_today_average(prices, 4, 0, 0, 1800, &avg) && avg == 150);
  assert(elec_today_average(prices, 4, 0, 1800, 100000, &avg) && avg == 350);
  assert(!elec_today_average(prices, 4, 0, 100000, 200000, &avg)); // no entries in window
  int16_t neg[2] = {-100, -300};
  assert(elec_today_average(neg, 2, 0, 0, 1800, &avg) && avg == -200);

  // --- elec_hour_in_quiet (night window [start,end), wraps midnight) ---
  assert(elec_hour_in_quiet(23, 23, 7));   // start hour is quiet
  assert(elec_hour_in_quiet(2, 23, 7));    // after midnight, before end
  assert(!elec_hour_in_quiet(7, 23, 7));   // end hour is NOT quiet (half-open)
  assert(!elec_hour_in_quiet(12, 23, 7));  // midday awake
  assert(elec_hour_in_quiet(13, 12, 18));  // non-wrapping window
  assert(!elec_hour_in_quiet(18, 12, 18)); // non-wrapping end exclusive
  assert(!elec_hour_in_quiet(3, 8, 8));    // start==end => no quiet hours

  // --- elec_cheap_bar = clamp(mean*pct/100, floor, ceiling) ---
  assert(elec_cheap_bar(1000, 70, 200, 800) == 700);  // 10.0 snt mean -> 7.0
  assert(elec_cheap_bar(200, 70, 200, 800) == 200);   // relative 1.4 -> clamped up to floor
  assert(elec_cheap_bar(3000, 70, 200, 800) == 800);  // relative 21.0 -> clamped to ceiling
  assert(elec_cheap_bar(0, 70, 200, 800) == 200);     // zero mean -> floor

  // --- elec_eligible_mean (mean of eligible[] quarters from fromIdx) ---
  int16_t em[6] = {100, 200, 300, 400, 500, 600};
  bool elig[6] = {true, true, true, true, true, true};
  int16_t mean;
  assert(elec_eligible_mean(em, elig, 6, 0, &mean) && mean == 350);
  assert(elec_eligible_mean(em, elig, 6, 4, &mean) && mean == 550); // fromIdx skips earlier
  bool elig2[6] = {false, false, true, true, false, false};
  assert(elec_eligible_mean(em, elig2, 6, 0, &mean) && mean == 350); // only idx 2,3
  bool none[6] = {false, false, false, false, false, false};
  assert(!elec_eligible_mean(em, none, 6, 0, &mean));               // no eligible

  // --- elec_find_next_cheap (earliest run of >=minQuarters eligible quarters
  //     all <= cheapBar; returns the FULL maximal run + its average) ---
  bool e8[8] = {true, true, true, true, true, true, true, true};
  // prices: a 3-long cheap run (too short), then a 4-long cheap run
  int16_t p1[8] = {100, 100, 100, 900, 200, 200, 200, 200};
  ElecWindow w = elec_find_next_cheap(p1, e8, 8, 0, 500, 4);
  assert(w.found && w.startIdx == 4 && w.len == 4 && w.avgCenti == 200);
  // earliest qualifying run is returned even if a later run is cheaper
  // (a non-cheap quarter at idx 4 separates the two runs)
  bool e9[9] = {true, true, true, true, true, true, true, true, true};
  int16_t p2[9] = {300, 300, 300, 300, 900, 100, 100, 100, 100};
  w = elec_find_next_cheap(p2, e9, 9, 0, 500, 4);
  assert(w.found && w.startIdx == 0 && w.len == 4 && w.avgCenti == 300);
  // ineligible quarter breaks the run
  bool e2[8] = {true, true, false, true, true, true, true, true};
  int16_t p3[8] = {100, 100, 100, 100, 100, 100, 100, 100};
  w = elec_find_next_cheap(p3, e2, 8, 0, 500, 4);
  assert(w.found && w.startIdx == 3 && w.len == 5);
  // nothing below the bar -> not found
  int16_t p4[8] = {900, 900, 900, 900, 900, 900, 900, 900};
  w = elec_find_next_cheap(p4, e8, 8, 0, 500, 4);
  assert(!w.found);
  // fromIdx skips an early cheap run
  w = elec_find_next_cheap(p3, e8, 8, 5, 500, 4);
  assert(!w.found);  // only 3 quarters (5,6,7) remain, < minQuarters

  // --- elec_find_cheapest (lowest-average run of exactly winQuarters eligible
  //     quarters; ties resolve to the earliest) ---
  bool ce[8] = {true, true, true, true, true, true, true, true};
  int16_t cp[8] = {500, 500, 100, 100, 100, 100, 500, 500};
  ElecWindow c = elec_find_cheapest(cp, ce, 8, 0, 4);
  assert(c.found && c.startIdx == 2 && c.len == 4 && c.avgCenti == 100);
  // tie -> earliest window wins
  int16_t cp2[8] = {100, 100, 100, 100, 100, 100, 100, 100};
  c = elec_find_cheapest(cp2, ce, 8, 0, 4);
  assert(c.found && c.startIdx == 0);
  // the cheap quarters (idx 0-1) cannot form a 4-window because idx 2 is
  // ineligible; the earliest fully-eligible window (idx 3-6) wins instead
  bool ce2[8] = {true, true, false, true, true, true, true, true};
  int16_t cp3[8] = {10, 10, 10, 900, 900, 900, 900, 900};
  c = elec_find_cheapest(cp3, ce2, 8, 0, 4);
  assert(c.found && c.startIdx == 3 && c.avgCenti == 900);
  // not enough contiguous eligible quarters -> not found
  bool ce3[3] = {true, true, true};
  int16_t cp4[3] = {10, 10, 10};
  c = elec_find_cheapest(cp4, ce3, 3, 0, 4);
  assert(!c.found);

  // --- elec_find_next_cheap_or_cheapest (threshold result, else fallback to
  //     the globally cheapest window so the widget never shows nothing) ---
  bool fe8[8] = {true, true, true, true, true, true, true, true};
  // A below-bar run EXISTS: return the earliest such run (idx 0), NOT the
  // globally cheapest later window (idx 5-8) -> proves no fallback was used.
  int16_t fp1[9] = {300, 300, 300, 300, 900, 100, 100, 100, 100};
  bool fe9[9] = {true, true, true, true, true, true, true, true, true};
  ElecWindow f = elec_find_next_cheap_or_cheapest(fp1, fe9, 9, 0, 500, 4);
  assert(f.found && f.startIdx == 0 && f.avgCenti == 300);
  // NOTHING below the bar -> fall back to the cheapest eligible window.
  int16_t fp2[8] = {900, 900, 900, 900, 800, 800, 800, 800};
  f = elec_find_next_cheap_or_cheapest(fp2, fe8, 8, 0, 500, 4);
  assert(f.found && f.startIdx == 4 && f.len == 4 && f.avgCenti == 800);
  // No fully-eligible window at all -> still not found (genuinely no data).
  bool fe3[3] = {true, true, true};
  int16_t fp3[3] = {10, 10, 10};
  f = elec_find_next_cheap_or_cheapest(fp3, fe3, 3, 0, 500, 4);
  assert(!f.found);

  printf("All electricity_calc tests passed\n");
  return 0;
}

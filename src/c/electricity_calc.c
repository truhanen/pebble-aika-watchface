// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include "electricity_calc.h"
#include <stdio.h>

bool elec_current_index(uint32_t startEpoch, uint16_t count, int64_t now, int *outIdx) {
  if (count == 0) { return false; }
  if (now < (int64_t)startEpoch) { return false; }
  int64_t idx = (now - (int64_t)startEpoch) / 900;
  if (idx >= (int64_t)count) { return false; }
  *outIdx = (int)idx;
  return true;
}

bool elec_today_average(const int16_t *prices, uint16_t count, uint32_t startEpoch,
                        int64_t dayStart, int64_t dayEnd, int16_t *out) {
  int64_t sum = 0;
  int n = 0;
  for (int i = 0; i < (int)count; i++) {
    int64_t start = (int64_t)startEpoch + (int64_t)i * 900;
    if (start >= dayStart && start < dayEnd) {
      sum += prices[i];
      n++;
    }
  }
  if (n == 0) { return false; }
  int64_t avg = (sum >= 0) ? (sum + n / 2) / n : -(((-sum) + n / 2) / n);
  *out = (int16_t)avg;
  return true;
}

void elec_format_price(int16_t value_centi, char sep, char *buf, size_t buflen) {
  // value_centi is 0.01 snt/kWh; display one decimal of snt (0.1 snt), rounded.
  int deci = (value_centi >= 0) ? (value_centi + 5) / 10
                                : -(((-value_centi) + 5) / 10);
  int neg = (deci < 0);
  int a = neg ? -deci : deci;
  int whole = a / 10;
  int frac = a % 10;
  snprintf(buf, buflen, "%s%d%c%d", neg ? "-" : "", whole, sep, frac);
}

bool elec_hour_in_quiet(int hour, int quietStart, int quietEnd) {
  if (quietStart == quietEnd) { return false; }
  if (quietStart < quietEnd) { return hour >= quietStart && hour < quietEnd; }
  return hour >= quietStart || hour < quietEnd;  // wraps past midnight
}

int16_t elec_cheap_bar(int16_t meanCenti, int factorPct,
                       int16_t floorCenti, int16_t ceilingCenti) {
  int32_t bar = ((int32_t)meanCenti * factorPct) / 100;
  if (bar < floorCenti) { bar = floorCenti; }
  if (bar > ceilingCenti) { bar = ceilingCenti; }
  return (int16_t)bar;
}

bool elec_eligible_mean(const int16_t *prices, const bool *eligible,
                        uint16_t count, int fromIdx, int16_t *out) {
  if (fromIdx < 0) { fromIdx = 0; }
  int64_t sum = 0;
  int n = 0;
  for (int i = fromIdx; i < (int)count; i++) {
    if (eligible[i]) { sum += prices[i]; n++; }
  }
  if (n == 0) { return false; }
  int64_t avg = (sum >= 0) ? (sum + n / 2) / n : -(((-sum) + n / 2) / n);
  *out = (int16_t)avg;
  return true;
}

ElecWindow elec_find_next_cheap(const int16_t *prices, const bool *eligible,
                                uint16_t count, int fromIdx,
                                int16_t cheapBar, int minQuarters) {
  ElecWindow w = { false, 0, 0, 0 };
  if (fromIdx < 0) { fromIdx = 0; }
  int runStart = -1;
  // Iterate one past the end so a run that reaches the table end is finalised.
  for (int i = fromIdx; i <= (int)count; i++) {
    bool cheap = (i < (int)count) && eligible[i] && (prices[i] <= cheapBar);
    if (cheap) {
      if (runStart < 0) { runStart = i; }
    } else if (runStart >= 0) {
      int len = i - runStart;
      if (len >= minQuarters) {
        int64_t sum = 0;
        for (int j = runStart; j < i; j++) { sum += prices[j]; }
        int64_t avg = (sum >= 0) ? (sum + len / 2) / len
                                 : -(((-sum) + len / 2) / len);
        w.found = true;
        w.startIdx = runStart;
        w.len = len;
        w.avgCenti = (int16_t)avg;
        return w;
      }
      runStart = -1;
    }
  }
  return w;
}

ElecWindow elec_find_cheapest(const int16_t *prices, const bool *eligible,
                              uint16_t count, int fromIdx, int winQuarters) {
  ElecWindow w = { false, 0, 0, 0 };
  if (fromIdx < 0) { fromIdx = 0; }
  if (winQuarters <= 0) { return w; }
  int64_t bestSum = 0;
  for (int i = fromIdx; i + winQuarters <= (int)count; i++) {
    bool ok = true;
    int64_t sum = 0;
    for (int j = i; j < i + winQuarters; j++) {
      if (!eligible[j]) { ok = false; break; }
      sum += prices[j];
    }
    if (!ok) { continue; }
    if (!w.found || sum < bestSum) {   // strict < => earliest wins on ties
      w.found = true;
      w.startIdx = i;
      w.len = winQuarters;
      bestSum = sum;
    }
  }
  if (w.found) {
    int64_t avg = (bestSum >= 0) ? (bestSum + winQuarters / 2) / winQuarters
                                 : -(((-bestSum) + winQuarters / 2) / winQuarters);
    w.avgCenti = (int16_t)avg;
  }
  return w;
}

ElecWindow elec_find_next_cheap_or_cheapest(const int16_t *prices, const bool *eligible,
                                            uint16_t count, int fromIdx,
                                            int16_t cheapBar, int winQuarters) {
  ElecWindow w = elec_find_next_cheap(prices, eligible, count, fromIdx,
                                      cheapBar, winQuarters);
  if (w.found) { return w; }
  // No hour below the threshold — fall back to the cheapest eligible window so
  // the widget shows the next-cheapest hour instead of "--".
  return elec_find_cheapest(prices, eligible, count, fromIdx, winQuarters);
}

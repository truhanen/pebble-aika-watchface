// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include "poll_state_calc.h"

int32_t poll_stamp_sanitize(int32_t stored, int32_t now) {
  if (stored <= 0) { return 0; }
  if (stored > now) { return now; }
  return stored;
}

bool poll_cold_allowed(int32_t lastCold, int32_t now, int32_t minIntervalS) {
  if (lastCold <= 0) { return true; }
  if (lastCold > now) { return true; }
  return (now - lastCold) >= minIntervalS;
}

int32_t poll_cold_interval(int level, int32_t baseS, int32_t maxS) {
  if (level < 0) { level = 0; }
  int32_t v = baseS;
  // Step rather than shift so a large level can never overflow into a small (or
  // negative) interval -- the failure mode would be MORE traffic, not less.
  for (int i = 0; i < level; i++) {
    if (v >= maxS) { break; }
    v *= 2;
  }
  return (v > maxS) ? maxS : v;
}

bool poll_due(int32_t lastRequest, int32_t now, int32_t intervalS) {
  if (lastRequest <= 0) { return true; }
  if (lastRequest > now) { return true; }
  return (now - lastRequest) >= intervalS;
}

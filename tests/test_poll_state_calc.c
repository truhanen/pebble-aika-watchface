// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#include "poll_state_calc.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  const int32_t now = 1000000;

  // a sane stored stamp survives untouched
  assert(poll_stamp_sanitize(now - 500, now) == now - 500);

  // never requested -> 0, which makes the tick handler poll at the next minute
  assert(poll_stamp_sanitize(0, now) == 0);
  assert(poll_stamp_sanitize(-5, now) == 0);

  // a future stamp (clock/timezone moved backwards) is clamped to now, so the
  // interval starts over instead of blocking polls for years
  assert(poll_stamp_sanitize(now + 10000, now) == now);

  // cold rate limit: never-cold is allowed, just-cold is not, expired is
  assert(poll_cold_allowed(0, now, 600) == true);
  assert(poll_cold_allowed(now - 599, now, 600) == false);
  assert(poll_cold_allowed(now - 600, now, 600) == true);
  // a future cold stamp must not lock cold requests out forever
  assert(poll_cold_allowed(now + 5000, now, 600) == true);

  // interval gate, shared by the tick poll and the BT-reconnect refresh: never
  // requested polls immediately, inside the interval does not, at/after it does
  assert(poll_due(0, now, 900) == true);
  assert(poll_due(now - 899, now, 900) == false);
  assert(poll_due(now - 900, now, 900) == true);
  assert(poll_due(now - 5000, now, 900) == true);
  // a future stamp must not block polls forever (same clock-jump reason as above)
  assert(poll_due(now + 5000, now, 900) == true);

  // ---- cold-request backoff interval: doubles per level, capped
  // A source that never resolves would otherwise force a full fetch every 10 min
  // forever, bypassing every phone-side throttle.
  assert(poll_cold_interval(0, 600, 21600) == 600);
  assert(poll_cold_interval(1, 600, 21600) == 1200);
  assert(poll_cold_interval(2, 600, 21600) == 2400);
  assert(poll_cold_interval(5, 600, 21600) == 19200);
  // capped, and stays capped for any higher level (no overflow, no wrap)
  assert(poll_cold_interval(6, 600, 21600) == 21600);
  assert(poll_cold_interval(30, 600, 21600) == 21600);
  assert(poll_cold_interval(100, 600, 21600) == 21600);
  // a negative/garbled level behaves as level 0 rather than shifting by a negative
  assert(poll_cold_interval(-1, 600, 21600) == 600);

  printf("PASS\n");
  return 0;
}

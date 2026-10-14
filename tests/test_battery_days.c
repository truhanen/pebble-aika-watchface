// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#include "battery_days_calc.h"
#include <assert.h>
#include <stdio.h>

// Convenience: full-life days*10 from a state's learned rate at a given percent.
static int est_tenths(const BatteryDaysState *s, uint8_t pct) {
  uint32_t rate = BatteryDays_rate(s);
  if (rate == 0) { rate = BATTERY_DAYS_DEFAULT_SEC_PER_PCT; }
  return BatteryDays_tenthsFromRate(pct, BatteryDays_capRate(rate));
}

int main(void) {
  // --- retained pure helpers: tenthsFromRate + capRate ---
  assert(BatteryDays_tenthsFromRate(0,  8640) == 0);                       // empty battery
  assert(BatteryDays_tenthsFromRate(40, 8640) == 40);                      // 40% at 8640 s/% = 4.0 d
  assert(BatteryDays_tenthsFromRate(100, BATTERY_DAYS_DEFAULT_SEC_PER_PCT) == 300); // fresh 100% -> 30.0 d
  assert(BatteryDays_tenthsFromRate(50,  BATTERY_DAYS_DEFAULT_SEC_PER_PCT) == 150); // 50% -> 15.0 d
  assert(BatteryDays_tenthsFromRate(100, 1u << 30) == BATTERY_DAYS_MAX_TENTHS);     // absurdly slow -> clamp
  assert(BatteryDays_tenthsFromRate(90, 0) == BATTERY_DAYS_NONE);          // no rate -> sentinel

  assert(BatteryDays_capRate(0) == 0);
  assert(BatteryDays_capRate(12960) == 12960);                             // 15d, faster than 30d -> kept
  assert(BatteryDays_capRate(BATTERY_DAYS_DEFAULT_SEC_PER_PCT) == BATTERY_DAYS_DEFAULT_SEC_PER_PCT);
  assert(BatteryDays_capRate(172800) == BATTERY_DAYS_DEFAULT_SEC_PER_PCT); // slow outlier -> capped

  // --- reset / cold start: no rate yet ---
  { BatteryDaysState s; BatteryDays_reset(&s);
    assert(BatteryDays_rate(&s) == 0);
    // first reading only anchors, still no rate
    BatteryDays_record(&s, 1000, 90, false);
    assert(BatteryDays_rate(&s) == 0); }

  // --- first real drop replaces the (zero) default with the measured segment rate ---
  { BatteryDaysState s; BatteryDays_reset(&s);
    BatteryDays_record(&s, 0,     90, false);
    BatteryDays_record(&s, 3600,  89, false);   // 1% over 1h -> 3600 s/%
    assert(BatteryDays_rate(&s) == 3600); }

  // --- unchanged percent does NOT advance the anchor: a long idle stretch yields
  //     one big-dt (slow) segment when the drop finally lands ---
  { BatteryDaysState s; BatteryDays_reset(&s);
    BatteryDays_record(&s, 0,     80, false);   // anchor
    BatteryDays_record(&s, 3600,  80, false);   // unchanged
    BatteryDays_record(&s, 7200,  80, false);   // unchanged
    BatteryDays_record(&s, 21600, 79, false);   // drop after 6h idle -> 21600 s/%
    assert(BatteryDays_rate(&s) == 21600); }

  // --- percent-weighted step: a short active segment then a long idle segment; each
  //     1% counts equally (weight = dp / HORIZON), moving learned one HORIZON-step ---
  { BatteryDaysState s; BatteryDays_reset(&s);
    BatteryDays_record(&s, 0,      99, false);
    BatteryDays_record(&s, 3600,   98, false);  // active: 3600 s/% -> learned=3600
    assert(BatteryDays_rate(&s) == 3600);
    BatteryDays_record(&s, 3600 + 36000, 97, false); // idle: 36000 s/% over 10h, dp=1
    // learned += (36000-3600) * 1/20 = 3600 + 1620 = 5220
    assert(BatteryDays_rate(&s) == 5220); }

  // --- charging preserves `learned` and re-anchors; the post-charge settling drop
  //     is DISCARDED (not folded) ---
  { BatteryDaysState s; BatteryDays_reset(&s);
    BatteryDays_record(&s, 0,     90, false);
    BatteryDays_record(&s, 7200,  88, false);   // 2% over 2h -> 3600 s/%
    uint32_t before = BatteryDays_rate(&s);
    assert(before == 3600);
    BatteryDays_record(&s, 8000,  95, true);    // charging: re-anchor, learned kept
    assert(BatteryDays_rate(&s) == before);
    BatteryDays_record(&s, 8100,  94, false);   // unplugged, settling: 100s/1% -> DISCARDED
    assert(BatteryDays_rate(&s) == before);     // unchanged: settling not folded
    // a subsequent genuine segment DOES fold
    BatteryDays_record(&s, 8100 + 7200, 93, false); // 7200 s/% real segment
    assert(BatteryDays_rate(&s) != before); }

  // --- a percent rise (missed charge event) is treated as a charge: re-anchor,
  //     keep learned, discard the following settling segment ---
  { BatteryDaysState s; BatteryDays_reset(&s);
    BatteryDays_record(&s, 0,     50, false);
    BatteryDays_record(&s, 7200,  48, false);   // 3600 s/%
    uint32_t before = BatteryDays_rate(&s);
    BatteryDays_record(&s, 9000,  70, false);   // rose -> treat as charge
    assert(BatteryDays_rate(&s) == before);
    BatteryDays_record(&s, 9060,  69, false);   // 60s settling -> discarded
    assert(BatteryDays_rate(&s) == before); }

  // --- MIN_PLAUSIBLE floor: an implausibly fast segment (not post-charge) is not
  //     folded (gauge glitch) ---
  { BatteryDaysState s; BatteryDays_reset(&s);
    BatteryDays_record(&s, 0,      80, false);
    BatteryDays_record(&s, 7200,   78, false);  // establishes learned=3600
    uint32_t before = BatteryDays_rate(&s);
    BatteryDays_record(&s, 7250,   77, false);  // 50s/1% < 120 floor -> discarded
    assert(BatteryDays_rate(&s) == before); }

  // --- FIELD REPRODUCTION: 12h active (1% / 5400s) + 12h idle (1% / 21600s) per
  //     day. Time-weighted true rate = 86400s / 10% = 8640 s/% => full life 10 d,
  //     7.4 d @ 74%. The OLD active-only view projected ~4.4 d. Include charges to
  //     prove the learned rate survives them. ---
  { BatteryDaysState s; BatteryDays_reset(&s);
    uint32_t t = 0; int pct = 100;
    for (int day = 0; day < 25; day++) {
      // 12h active: 8 drops of 1% every 5400s (=12h, 8%)
      for (int i = 0; i < 8; i++) { t += 5400; pct -= 1; BatteryDays_record(&s, t, (uint8_t)pct, false); }
      // 12h idle: 2 drops of 1% every 21600s (=12h, 2%)
      for (int i = 0; i < 2; i++) { t += 21600; pct -= 1; BatteryDays_record(&s, t, (uint8_t)pct, false); }
      if (pct <= 60) {                    // recharge like the user does (tops up early)
        t += 3600; pct = 95; BatteryDays_record(&s, t, (uint8_t)pct, true);   // charge
        t += 60;              BatteryDays_record(&s, t, (uint8_t)pct, false);  // unplug (settling anchor)
      }
    }
    uint32_t learned = BatteryDays_rate(&s);
    // converged near the time-weighted 8640 s/% (EWMA ripple allowed), NOT the
    // active-only ~5000 s/% that produced the 4.4 d bug.
    printf("field-repro learned=%u  days@74=%d.%d\n",
           learned, est_tenths(&s, 74) / 10, est_tenths(&s, 74) % 10);
    assert(learned >= 7000 && learned <= 9800);
    int tenths74 = est_tenths(&s, 74);
    assert(tenths74 >= 60 && tenths74 <= 90);   // ~6.0-9.0 d, i.e. NOT the buggy 4.4 d
  }

  printf("All battery_days tests passed\n");
  return 0;
}

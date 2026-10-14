// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#pragma once

#include <stdint.h>
#include <stdbool.h>

// Estimate "days of battery left" from charge_percent samples over time.
// Pure logic (no Pebble API) so it is host-gcc unit-testable; the Pebble glue
// (persist + time + battery service) lives in battery_days.c.
//
// Model: an exponential moving average of the discharge rate (seconds per 1%
// drop), updated once per observed discharge segment and weighted by the PERCENT
// dropped (each 1% counts equally). Averaging per-percent gives exactly the
// projection rate total_seconds / total_percent = the average sec/% you will
// experience going forward; a PERCENT horizon spanning ~2 days of drop therefore
// folds in idle nights alongside active days. The average survives charge cycles
// (only the charge LEVEL resets, the learned rate persists). See
// docs/superpowers/specs/2026-07-06-timestyle-battery-estimate-fullcycle-design.md.
//
// NB: weighting by percent (not by wall-clock time) is deliberate. A time-weighted
// average of sec/% over-weights slow idle segments and projects too FEW percent per
// day -> too MANY days. total_time/total_drop (== equal weight per 1%) is the
// correct projection basis.

#define BATTERY_DAYS_MAX_TENTHS   999    // clamp display to 99.9 days
#define BATTERY_DAYS_NONE         (-1)   // sentinel: no rate yet / degenerate

// Default assumed battery life for a fresh install (no learned rate yet):
// 30-day life = 30*86400/100 seconds per 1% drop. Also the CEILING (see capRate):
// the manufacturer's promised max life, so a slower measured rate is implausible.
#define BATTERY_DAYS_DEFAULT_SEC_PER_PCT 25920u

// EWMA horizon in PERCENT dropped. ~20% spans about two days of discharge on a
// ~10-day battery, so a full day/night cycle averages out; the average converges
// over roughly this many percent while staying stable against short usage swings.
#define BATTERY_DAYS_EWMA_PCT_HORIZON    20u

// A discharge segment faster than this (fewer sec per 1%) is physically
// implausible (1% in <120s => >830%/day) -> a gauge glitch or post-charge
// settling; not folded into the learned rate.
#define BATTERY_DAYS_MIN_PLAUSIBLE_SEC_PER_PCT 120u

// Time-weighted discharge-rate estimator state (persisted by the glue).
typedef struct {
  uint32_t learned;      // time-weighted EWMA rate, sec per 1% drop; 0 = none yet
  uint32_t last_t;       // unix timestamp of the last recorded reading
  uint8_t  last_pct;     // charge_percent of the last recorded reading
  bool     have_last;    // false until the first reading is seen
  bool     after_charge; // the next discharge segment is post-charge -> discard it
} BatteryDaysState;

// Reset to the cold-start empty state (no learned rate, no anchor).
void BatteryDays_reset(BatteryDaysState *s);

// Record a battery reading. Charging (or a percent rise) re-anchors and arms the
// post-charge settling discard, leaving `learned` untouched; an unchanged percent
// lets the elapsed time accumulate; a drop folds a time-weighted segment into
// `learned` (unless it is the post-charge settling segment or implausibly fast).
void BatteryDays_record(BatteryDaysState *s, uint32_t now, uint8_t pct, bool is_charging);

// The learned discharge rate in seconds per 1% drop, or 0 if none learned yet.
uint32_t BatteryDays_rate(const BatteryDaysState *s);

// Remaining life in TENTHS of a day from a charge level + rate, clamped to
// [0, BATTERY_DAYS_MAX_TENTHS]. sec_per_pct == 0 -> BATTERY_DAYS_NONE.
int BatteryDays_tenthsFromRate(uint8_t pct, uint32_t sec_per_pct);

// Cap a discharge rate to the 30-day default ceiling: the default is the MAXIMUM
// plausible life, so a slower measured rate (bigger sec/%, implying >30d) is
// implausible and falls back to the default. A faster rate is kept; 0 passes
// through unchanged.
uint32_t BatteryDays_capRate(uint32_t sec_per_pct);

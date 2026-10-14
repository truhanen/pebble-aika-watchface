// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define ELEC_MAX_QUARTERS 192

// A contiguous price window result (used by the cheap-electricity widgets).
typedef struct {
  bool    found;
  int     startIdx;   // index into prices[]
  int     len;        // number of quarters in the window
  int16_t avgCenti;   // average price over the window (0.01 snt/kWh)
} ElecWindow;

// Quarter index for `now`; quarters are at startEpoch + i*900 (seconds, UTC).
// Returns false if count==0 or now is outside [startEpoch, startEpoch+count*900).
bool elec_current_index(uint32_t startEpoch, uint16_t count, int64_t now, int *outIdx);

// Average of all quarters whose start falls in [dayStart, dayEnd) (epoch seconds).
// out is in 0.01 snt (rounded). Returns false if no quarter falls in the window.
bool elec_today_average(const int16_t *prices, uint16_t count, uint32_t startEpoch,
                        int64_t dayStart, int64_t dayEnd, int16_t *out);

// Formats value_centi (0.01 snt/kWh) as "x.x" with one decimal of snt into buf.
// `sep` is the decimal separator char. Handles negatives. buf should be >= 12 bytes.
void elec_format_price(int16_t value_centi, char sep, char *buf, size_t buflen);

// True if `hour` (0-23) falls in the quiet window [quietStart, quietEnd) (hours,
// 0-23). The window wraps past midnight when quietStart > quietEnd. If
// quietStart == quietEnd the window is empty (always returns false).
bool elec_hour_in_quiet(int hour, int quietStart, int quietEnd);

// Cheap-price threshold (0.01 snt): clamp(meanCenti * factorPct / 100,
// floorCenti, ceilingCenti). Used by the "next cheap" widget.
int16_t elec_cheap_bar(int16_t meanCenti, int factorPct,
                       int16_t floorCenti, int16_t ceilingCenti);

// Mean (0.01 snt, rounded) of quarters i in [fromIdx, count) with eligible[i].
// Returns false if no eligible quarter exists in range.
bool elec_eligible_mean(const int16_t *prices, const bool *eligible,
                        uint16_t count, int fromIdx, int16_t *out);

// Earliest maximal run of >= minQuarters consecutive eligible quarters whose
// prices are all <= cheapBar, scanning from fromIdx forward. Returns the full
// run (startIdx, len, average). found=false if none qualifies.
ElecWindow elec_find_next_cheap(const int16_t *prices, const bool *eligible,
                                uint16_t count, int fromIdx,
                                int16_t cheapBar, int minQuarters);

// Lowest-average run of exactly winQuarters consecutive eligible quarters,
// scanning from fromIdx forward. Ties resolve to the earliest window.
// found=false if no fully-eligible window of winQuarters exists.
ElecWindow elec_find_cheapest(const int16_t *prices, const bool *eligible,
                              uint16_t count, int fromIdx, int winQuarters);

// "Next cheap" widget result with a graceful fallback: the earliest run below
// cheapBar (genuinely cheap) if one exists, otherwise the globally cheapest
// winQuarters-eligible window — so the widget shows the next-cheapest hour
// rather than nothing when no hour beats the threshold. found=false only when
// no fully-eligible winQuarters window exists at all (no usable data).
ElecWindow elec_find_next_cheap_or_cheapest(const int16_t *prices, const bool *eligible,
                                            uint16_t count, int fromIdx,
                                            int16_t cheapBar, int winQuarters);

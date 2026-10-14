// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#pragma once
#include <stddef.h>

/* Pure sleep-widget arithmetic: no SDK calls, no time.h, so this compiles for both
   the watch and the host test (tests/test_sleep_calc.c). */

// Format `seconds` as hours with one TRUNCATED tenth (never rounded up), using `sep`
// as the decimal separator: 26580 -> "7.3". Negative input is treated as 0.
void sleep_format_decimal(int seconds, char sep, char *buf, size_t n);

// Pixels of a `width_px`-wide bar to fill, where the fill is the restful share of
// total sleep. Returns 0 when width_px <= 0, total_s <= 0 or deep_s <= 0; at least
// 1 px whenever deep_s > 0 and width_px > 0; never more than width_px.
int sleep_bar_fill_px(int total_s, int deep_s, int width_px);

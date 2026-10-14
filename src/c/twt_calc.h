// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <stdint.h>
#include <stddef.h>

// Rounded percentage 100*value/base. Returns -1 when base <= 0 (caller hides the
// percent). value < 0 is clamped to 0. The result MAY exceed 100 (overtime).
int twt_percent(int32_t value, int32_t base);

// Filled width in pixels for a progress bar of total width `width_px` representing
// value/base, clamped to [0, width_px]. Returns 0 when base <= 0, value <= 0, or
// width_px <= 0.
int twt_bar_fill_px(int32_t value, int32_t base, int width_px);

// Formats minutes as "h:mm". Negative values get a leading '-' on the magnitude
// (e.g. -65 -> "-1:05", 0 -> "0:00", 90 -> "1:30"). Writes into buf (bufsize >= 8).
void twt_fmt_hhmm_signed(char* buf, size_t bufsize, int32_t minutes);

// Formats a "remaining = target - worked" value for display. A non-negative value
// (time still left) is shown plain ("0:45"); a negative value (overtime) is shown
// as the magnitude with a leading '+' ("+0:05" = 5 min worked over target), which
// reads more intuitively than a minus sign. Writes into buf (bufsize >= 8).
void twt_fmt_hhmm_remaining(char* buf, size_t bufsize, int32_t remaining);

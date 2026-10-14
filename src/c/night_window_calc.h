// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <stdbool.h>

// Decides whether it is currently "night" for the watchface's purposes, and (as one
// consumer of that decision) whether sidebar widget rotation should be slowed to save
// battery.
//
// Why the rotation consumer exists: a rotating group with a sub-minute interval (5/10/30 s)
// registers an app_timer, and because layer_mark_dirty re-renders the ENTIRE window (there
// is no partial refresh on Pebble), each of those wakeups repaints the clock and every
// widget. At 5 s that is 720 extra full renders per hour, all night, for a sidebar nobody
// is looking at. Intervals of 60 s or more cost nothing extra -- they ride the minute tick
// the watchface performs anyway -- so "slowing down" simply means suppressing the timer.
//
// Pebble-free so it host-compiles for tests/test_night_window_calc.c.

typedef enum {
  NIGHT_WINDOW_OFF = 0,         // rotate at the configured interval around the clock
  NIGHT_WINDOW_QUIET_TIME = 1,  // follow the watch's own Quiet Time schedule
  NIGHT_WINDOW_CUSTOM = 2,      // follow the configured hour window
} NightWindowMode;

// True when the night window is currently in effect.
//
// `hour` is the local hour (0-23). `startHour`/`endHour` describe a half-open window
// [start, end) that may wrap past midnight; start == end is an EMPTY window (never
// night), matching the electricity quiet-hours convention. `quietTimeActive` is the
// caller's quiet_time_is_active() reading, passed in so this file stays Pebble-free.
//
// An unrecognised mode returns false: failing safe means rotating normally, never
// silently freezing the sidebar.
bool night_window_active(int mode, int hour, int startHour, int endHour,
                          bool quietTimeActive);

// The sub-minute rotation interval to actually arm a timer for: 0 (i.e. no timer) while
// the night window is active, otherwise the configured interval unchanged. 0 in means 0
// out -- there was no sub-minute group to slow down in the first place.
int night_window_rotation_interval(int configuredSec, bool nightActive);

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <stdbool.h>
#include <stdint.h>

// Decides whether the clock area gets a warning frame, and in what colour.
//
// Pebble-free so it host-compiles for tests/test_warn_border_calc.c: colours cross this
// boundary as raw GColor8 argb bytes (a<<6 | r<<4 | g<<2 | b), never as GColor.

typedef enum {
  WARN_BORDER_NONE = 0,
  WARN_BORDER_BATTERY = 1,
  WARN_BORDER_BT = 2,
} WarnBorderKind;

// Opaque black / white, for the invisibility guard below.
#define WARN_BORDER_ARGB_BLACK 0xC0
#define WARN_BORDER_ARGB_WHITE 0xFF

// Which frame to draw right now, or WARN_BORDER_NONE.
//
// batteryDaysTenths is BatteryDays_currentEstimateTenths(): tenths of a day, or
// BATTERY_DAYS_NONE when no estimate exists yet. warnPct and warnDaysTenths are the
// configured thresholds, where 0 means "this trigger is off"; the two are OR-combined
// and both comparisons are inclusive.
//
// Charging suppresses the battery warning: a watch on the charger is not low in any
// sense the user can act on (the battery-days widget hides itself for the same reason).
// The battery warning beats the Bluetooth one, because it is the condition that ends
// with the watch switched off.
int warn_border_kind(int batteryPct, bool isCharging, int batteryDaysTenths,
                     int warnPct, int warnDaysTenths,
                     bool btConnected, bool btWarnEnabled);

// The argb actually stroked for `kind`, or 0 for WARN_BORDER_NONE.
//
// Guards against an invisible frame: when the configured colour has the same visible
// RGB as the background it would be drawn on, returns opaque black or white -- whichever
// contrasts with that background. This matters constantly rather than rarely: the 1-bit
// boards (diorite, flint) map every colour to black or white, and with night colours on
// the background is the night background regardless of what the user configured.
uint8_t warn_border_color(int kind, uint8_t batteryArgb, uint8_t btArgb,
                          uint8_t effectiveBgArgb);

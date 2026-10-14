// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <pebble.h>

// Height in px reserved at the top of the screen for the large date line.
// Tuned to FONT_KEY_BITHAM_30_BLACK.
#define BIG_DATE_HEIGHT 34

// true on platforms where we render the date header (rect, non-aplite),
// matching TwtStatus_isSupported().
bool DateHeader_isSupported(void);

// Create/destroy the date layer as a child of `parent`, occupying the top strip.
// The layer is created HIDDEN; main.c shows it only when settings.showBigDate is on.
void DateHeader_initLayer(Layer* parent, GRect frame);
void DateHeader_deinitLayer(void);

// Reposition the date line (so it can be inset to clear the sidebar).
void DateHeader_setFrame(GRect frame);

// Show/hide the date line.
void DateHeader_setHidden(bool hidden);

// Rebuild the date string from the given time
// (call from update_clock(), like ClockArea_update_time()).
void DateHeader_updateTime(struct tm* timeInfo);

// Refresh color + text and mark the layer dirty (call on tick + after layout).
void DateHeader_redraw(void);

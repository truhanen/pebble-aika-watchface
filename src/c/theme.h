// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <pebble.h>

// The RESOLVED palette: what drawing code should actually use, as opposed to what the
// user configured. Recomputed from `settings` plus the current night state, so night
// colours are applied in ONE place instead of at every draw site.
//
// Drawing code reads `theme`. settings.c, messaging.c and theme.c are the only files
// that may read settings.*Color -- they are the configuration path, not the drawing
// path. See the grep gate in the plan/spec.
typedef struct {
  GColor timeColor;
  GColor timeBgColor;
  GColor sidebarColor;
  GColor sidebarTextColor;
  GColor sidebarBgColorLeft;   // GColorClear = inherit sidebarColor
  GColor sidebarBgColorRight;  // GColorClear = inherit sidebarColor
  GColor dateBgColor;          // GColorClear = inherit timeBgColor
  GColor twtStatusBgColor;     // GColorClear = inherit timeBgColor
  GColor twtFlashColor;        // NOT night-mapped: a one-shot signal, not palette
  GColor iconFillColor;        // derived from the resolved primary sidebar background
  GColor iconStrokeColor;
} Theme;

extern Theme theme;

// Recompute `theme` from `settings`. Call after any settings change and whenever the
// night state flips.
void Theme_update(bool nightActive);

// The night state `theme` was last computed with.
bool Theme_nightActive(void);

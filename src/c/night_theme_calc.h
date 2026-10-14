// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <stdbool.h>
#include <stdint.h>

// Maps one configured colour to what should actually be drawn, given the night state.
//
// Pebble-free so it host-compiles for tests/test_night_theme_calc.c: colours cross this
// boundary as raw GColor8 argb bytes.

// GColor8 for GColorClear (alpha 0). Used as "inherit" by the status, date and sidebar
// background settings.
#define NIGHT_THEME_ARGB_CLEAR 0x00

// Night off -> `configuredArgb` unchanged. Night on -> the night background for a
// background, the night foreground for anything else.
//
// GColorClear is ALWAYS returned unchanged: it means "inherit the colour behind me", and
// the colour it inherits from has itself been through this function, so it is already
// night-mapped. Rewriting Clear to an opaque colour would silently turn every
// inherit-by-default panel into an explicitly painted one.
uint8_t night_theme_color(uint8_t configuredArgb, bool isBackground, bool nightActive,
                          uint8_t nightBgArgb, uint8_t nightFgArgb);

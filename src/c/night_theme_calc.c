// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include "night_theme_calc.h"

uint8_t night_theme_color(uint8_t configuredArgb, bool isBackground, bool nightActive,
                          uint8_t nightBgArgb, uint8_t nightFgArgb) {
  if (!nightActive) { return configuredArgb; }
  if (configuredArgb == NIGHT_THEME_ARGB_CLEAR) { return configuredArgb; }
  return isBackground ? nightBgArgb : nightFgArgb;
}

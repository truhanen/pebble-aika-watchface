// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include "theme.h"
#include "settings.h"
#include "night_theme_calc.h"

Theme theme;
static bool s_night_active = false;

static GColor mapc(GColor configured, bool isBackground, bool nightActive) {
  GColor out;
  out.argb = night_theme_color(configured.argb, isBackground, nightActive,
                               settings.nightBgColor.argb, settings.nightFgColor.argb);
  return out;
}

void Theme_update(bool nightActive) {
  s_night_active = nightActive;

  theme.timeColor          = mapc(settings.timeColor, false, nightActive);
  theme.timeBgColor        = mapc(settings.timeBgColor, true, nightActive);

  theme.iconFillColor = theme.timeBgColor;
  theme.iconStrokeColor = theme.timeColor;
}

bool Theme_nightActive(void) { return s_night_active; }

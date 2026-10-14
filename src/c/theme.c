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
  theme.sidebarColor       = mapc(settings.sidebarColor, true, nightActive);
  theme.sidebarTextColor   = mapc(settings.sidebarTextColor, false, nightActive);
  theme.sidebarBgColorLeft = mapc(settings.sidebarBgColorLeft, true, nightActive);
  theme.sidebarBgColorRight = mapc(settings.sidebarBgColorRight, true, nightActive);
  theme.dateBgColor        = mapc(settings.dateBgColor, true, nightActive);
  theme.twtStatusBgColor   = mapc(settings.twtStatusBgColor, true, nightActive);
  // Deliberately NOT night-mapped: the flash marks a work-time target being reached and
  // has to stand out from the palette, at night as much as by day.
  theme.twtFlashColor      = settings.twtFlashColor;

  // Sidebar icons follow the RESOLVED sidebar background, not the configured one, so at
  // night they invert with the text around them. Same rule as the version that used to
  // live in Settings_updateDynamicSettings: GColorClear = inherit sidebarColor.
  GColor primaryBg = gcolor_equal(theme.sidebarBgColorLeft, GColorClear)
      ? theme.sidebarColor : theme.sidebarBgColorLeft;
  if (gcolor_equal(primaryBg, GColorBlack)) {
    theme.iconFillColor = GColorBlack;
    theme.iconStrokeColor = theme.sidebarTextColor;
  } else {
    theme.iconFillColor = GColorWhite;
    theme.iconStrokeColor = GColorBlack;
  }
}

bool Theme_nightActive(void) { return s_night_active; }

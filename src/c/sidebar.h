#pragma once
#include <pebble.h>

extern int sidebarWidth;

// Per-column vertical padding (top and bottom). Shared with main.c so it can
// compute the per-column inner height available for widget packing.
#define V_PADDING_DEFAULT 5

// "public" functions
void Sidebar_init(Window* window);
void Sidebar_deinit();
void Sidebar_redraw();
void Sidebar_updateTime(struct tm* timeInfo);

// main.c owns the panel frames and calls these.
void Sidebar_setPrimaryFrame(GRect frame);
void Sidebar_setSecondaryFrame(GRect frame);
void Sidebar_setSecondaryHidden(bool hidden);
void Sidebar_setPrimaryHidden(bool hidden);

// Fills the left column from settings.widgetList and the right column from
// settings.rightWidgetList, each in full (overflow is clipped at draw time, not
// dropped). Writes the per-column visible-candidate counts (number of non-EMPTY
// entries) to the out params.
void Sidebar_distributeWidgets(int *primaryCountOut, int *secondaryCountOut);

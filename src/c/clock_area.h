#pragma once
#include <pebble.h>

extern Layer* clock_area_layer;

// "public" functions
void ClockArea_init(Window* window);
void ClockArea_deinit();
void ClockArea_redraw();
void ClockArea_update_time(struct tm* time_info);

// Tell the clock layer whether a bottom status strip is currently visible, so it
// can swap the analog dial for the big digital clock when statusClockDigital is on.
void ClockArea_setStatusVisible(bool visible);

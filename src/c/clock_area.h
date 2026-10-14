#pragma once
#include <pebble.h>

extern Layer* clock_area_layer;

// "public" functions
void ClockArea_init(Window* window);
void ClockArea_deinit();
void ClockArea_redraw();
void ClockArea_update_time(struct tm* time_info);

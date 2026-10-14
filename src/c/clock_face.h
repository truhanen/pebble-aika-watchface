#pragma once
#include <pebble.h>

// The full-screen clock face layer: hands, pivot and edge minute strip.
extern Layer *clock_face_layer;

void ClockFace_init(Window *window);
void ClockFace_deinit(void);
void ClockFace_redraw(void);
void ClockFace_updateTime(struct tm *time_info);

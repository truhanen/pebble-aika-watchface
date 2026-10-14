#pragma once
#include <pebble.h>

// Draws an analog clock face (hour + minute hands, optional 12 tick marks,
// center pivot) filling `bounds`, in LAYER-LOCAL coordinates. `fg` = hand/tick
// color, `bg` = background/halo color. Hours 0-23, minutes 0-59. The 12 hour
// tick marks are drawn per `tick_style`: 0 = hide, 1 = normal,
// 2 = bold (thicker + slightly longer).
//
// Ported from truhanen/pebble-nyquist-watchface (GPL-3.0).
void ClockAnalog_draw(GContext *ctx, GRect bounds, int hours, int minutes,
                      GColor fg, GColor bg, int tick_style);

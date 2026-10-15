#pragma once
#include <pebble.h>

// The 17 per-widget font choices. The index is the value stored in the widget
// payload (see widget_font_metrics.h for each font's measured ink metrics).
#define WIDGET_FONT_COUNT 17
#define WIDGET_FONT_DEFAULT_SIDE 5         // Gothic medium (GOTHIC_28_BOLD)
#define WIDGET_FONT_DEFAULT_CENTER 7   // Bitham large (BITHAM_30_BLACK)
#define WIDGET_FONT_UNIT 0                 // units are always Gothic 3x-small

GFont WidgetFonts_get(uint8_t index);

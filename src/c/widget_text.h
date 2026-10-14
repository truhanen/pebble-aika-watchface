#pragma once
// Text and unit drawing for widgets: mixed-font runs on a shared baseline (a font's
// missing glyphs come from its measured fallback font, see widget_font_metrics.h),
// the value shadow, and the GOTHIC_14 unit (inline, or stacked as a fraction).
#include <pebble.h>

// Advance width of text in the widget font, summed over its font runs.
int WidgetText_width(const char *text, int font);
// Height of the digits' ink in the widget font.
int WidgetText_inkHeight(int font);

// Draw text in the current text colour with its baseline on row `baseline`. Runs of
// characters the font lacks are drawn in its fallback font on the same baseline.
void WidgetText_drawAtBaseline(GContext *ctx, const char *text, int font, int x, int baseline);
// Draw text so the digits' ink starts at row ink_top.
void WidgetText_draw(GContext *ctx, const char *text, int font, int x, int ink_top);
// A widget value: a 2 px shadow in `bg` (the CF_SHADOW_OFFSETS diamond), then the text in `fg`.
void WidgetText_drawValue(GContext *ctx, const char *text, int font, int x, int ink_top,
                          GColor fg, GColor bg);

// Unit label in GOTHIC_14: "A/B" is a stacked fraction (numerator, a 1 px rule as
// wide as the wider part, denominator), anything else one inline line.
// True if the label is drawn as one inline line (no stacked fraction).
bool WidgetUnit_isInline(const char *label);
void WidgetUnit_measure(const char *label, int *w, int *h);
// Draw the unit in `fg` (no shadow) into box (the size WidgetUnit_measure returned).
void WidgetUnit_draw(GContext *ctx, const char *label, GRect box, GColor fg);

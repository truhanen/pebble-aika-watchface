#include <pebble.h>

#include "clock_area.h"
#include "settings.h"
#include "theme.h"
#include "clock_analog.h"
#include "warn_border_calc.h"
#include "battery_days.h"

Layer* clock_area_layer;

static int s_clock_hours;
static int s_clock_minutes;

static void draw_clock_content(Layer *l, GContext* ctx) {
  GRect bounds = layer_get_unobstructed_bounds(l);
  ClockAnalog_draw(ctx, bounds, s_clock_hours, s_clock_minutes,
                   theme.timeColor, theme.timeBgColor, settings.analogTickStyle);
}

// Frame thickness. The SDK's graphics_context_set_stroke_width only supports ODD
// widths: an even value is stored as-is but the drawing routines round it DOWN to the
// previous odd value at draw time, so 2 would silently render as a 1px hairline -- 3 is
// the smallest width that actually draws thicker than 1px, on every board. Note
// graphics_draw_rect still strokes centred on the path, which is why the inset stays 1:
// a width-3 stroke centred on a path 1px in occupies rows 0,1,2 -- flush to the edge
// without overshooting.
#define WARN_BORDER_WIDTH 3
#define WARN_BORDER_INSET 1

// Warning frame around the clock area (the clock layer's frame). Drawn LAST so a clock
// hand can never occlude it.
static void draw_warn_border(Layer *l, GContext* ctx) {
  // Feature entirely off (both battery triggers AND the BT trigger 0/false) -- the
  // default. Bail out before touching any service, so the default configuration never
  // pays for battery_state_service_peek/BatteryDays_currentEstimateTenths/
  // bluetooth_connection_service_peek on every redraw (up to 60x/min with some
  // configurations). Mirrors night_window_now()'s cheap path in main.c.
  if (settings.batteryWarnPct == 0 && settings.batteryWarnDaysTenths == 0
      && settings.btWarnBorder == 0) { return; }

  BatteryChargeState charge = battery_state_service_peek();
  int kind = warn_border_kind(charge.charge_percent, charge.is_charging,
                              BatteryDays_currentEstimateTenths(),
                              settings.batteryWarnPct, settings.batteryWarnDaysTenths,
                              bluetooth_connection_service_peek(),
                              settings.btWarnBorder != 0);
  if (kind == WARN_BORDER_NONE) { return; }

  uint8_t argb = warn_border_color(kind, settings.batteryWarnColor.argb,
                                   settings.btWarnColor.argb, theme.timeBgColor.argb);
  GColor c; c.argb = argb;

  // Unobstructed bounds, so the frame moves with the timeline-peek banner instead of
  // hiding behind it.
  GRect r = grect_inset(layer_get_unobstructed_bounds(l),
                        GEdgeInsets(WARN_BORDER_INSET));
  graphics_context_set_stroke_color(ctx, c);
  graphics_context_set_stroke_width(ctx, WARN_BORDER_WIDTH);
  graphics_draw_rect(ctx, r);
}

void update_clock_area_layer(Layer *l, GContext* ctx) {
  draw_clock_content(l, ctx);
  draw_warn_border(l, ctx);
}

void ClockArea_init(Window* window) {
  GRect bounds = layer_get_bounds(window_get_root_layer(window));
  clock_area_layer = layer_create(bounds);
  layer_add_child(window_get_root_layer(window), clock_area_layer);
  layer_set_update_proc(clock_area_layer, update_clock_area_layer);
}

void ClockArea_deinit() {
  layer_destroy(clock_area_layer);
}

void ClockArea_redraw() {
  layer_mark_dirty(clock_area_layer);
}

void ClockArea_update_time(struct tm* time_info) {
  s_clock_hours = time_info->tm_hour;
  s_clock_minutes = time_info->tm_min;
  layer_mark_dirty(clock_area_layer);
}

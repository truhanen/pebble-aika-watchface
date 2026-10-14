// Clock face: hour + minute hands, centre pivot, the dithered minute strip along the
// screen edges and the widgets (icons below the hands, values above), all on one
// full-screen layer.
//
// Hand dimensions are those of the Nyquist watchface (truhanen/
// pebble-nyquist-watchface, GPL-3.0); the minute hand is lengthened so it always
// runs off the screen. Each hand is a pentagon whose five corners are computed from
// the ideal geometry and rounded once to the nearest pixel (clock_face_calc.c),
// then filled and outlined with the plain GContext API.
#include <pebble.h>

#include "clock_face.h"
#include "clock_face_calc.h"
#include "settings.h"
#include "theme.h"
#include "warn_border_calc.h"
#include "battery_days.h"
#include "test_hooks.h"
#include "font_specimen.h"
#include "widget_layer.h"

Layer *clock_face_layer;

static int s_hours;
static int s_minutes;

// Minute index (0..59) of every strip pixel, in cf_strip_row_spans order.
static uint8_t *s_strip_minute;

static GPoint to_gpoint(CfPoint p) { return GPoint(p.x, p.y); }

static void hand_pose(GPoint center, int32_t angle, int shoulder, int width,
                      int apex_ext, CfHandPose *out) {
  float sin_a = (float)sin_lookup(angle) / TRIG_MAX_RATIO;
  float cos_a = (float)cos_lookup(angle) / TRIG_MAX_RATIO;
  cf_hand_pose(center.x, center.y, sin_a, cos_a, shoulder, width, apex_ext,
               CF_HAND_TAIL, out);
}

static void draw_hand_border(GContext *ctx, const CfHandPose *p, int stroke_w, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, stroke_w);
  graphics_draw_line(ctx, to_gpoint(p->inner_left),  to_gpoint(p->inner_right));
  graphics_draw_line(ctx, to_gpoint(p->inner_right), to_gpoint(p->outer_right));
  graphics_draw_line(ctx, to_gpoint(p->inner_left),  to_gpoint(p->outer_left));
  graphics_draw_line(ctx, to_gpoint(p->outer_right), to_gpoint(p->apex));
  graphics_draw_line(ctx, to_gpoint(p->outer_left),  to_gpoint(p->apex));
}

static void draw_hand_fill(GContext *ctx, const CfHandPose *p, GColor color) {
  // A file-static path: GPath only copies the points pointer, and the event loop is
  // single-threaded, so rewriting the points before each draw is safe and saves a
  // heap alloc/free per hand per frame.
  static GPoint s_pts[5];
  static GPath s_path = { .num_points = 5, .points = s_pts, .rotation = 0,
                          .offset = { 0, 0 } };
  s_pts[0] = to_gpoint(p->inner_left);
  s_pts[1] = to_gpoint(p->inner_right);
  s_pts[2] = to_gpoint(p->outer_right);
  s_pts[3] = to_gpoint(p->apex);
  s_pts[4] = to_gpoint(p->outer_left);
  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, &s_path);
}

static void build_strip_table(int w, int h) {
  s_strip_minute = malloc(cf_strip_pixel_count(w, h));
  if (!s_strip_minute) { return; }
  int cx = w / 2, cy = h / 2, i = 0;
  for (int y = 0; y < h; y++) {
    int x0[2], x1[2];
    int n = cf_strip_row_spans(y, w, h, x0, x1);
    for (int s = 0; s < n; s++) {
      for (int x = x0[s]; x < x1[s]; x++) {
        s_strip_minute[i++] = (uint8_t)cf_minute_of_direction(2 * x + 1 - 2 * cx,
                                                              2 * y + 1 - 2 * cy);
      }
    }
  }
}

// Writes the strip straight into the framebuffer. A pixel that currently shows the
// hand colour (only the minute hand reaches the strip) gets the inverted shade, so
// the hand's exact outline cuts through the dither pattern.
static void draw_strip(GContext *ctx, GRect bounds, GColor hand) {
  if (!s_strip_minute) { return; }
  bool invert = !gcolor_equal(hand, theme.timeBgColor);
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) { return; }
  const uint8_t black = GColorBlack.argb, white = GColorWhite.argb;
  int w = bounds.size.w, h = bounds.size.h, i = 0;
  for (int y = 0; y < h; y++) {
    int x0[2], x1[2];
    int n = cf_strip_row_spans(y, w, h, x0, x1);
    if (n == 0) { continue; }
    GBitmapDataRowInfo row = gbitmap_get_data_row_info(fb, y);
    for (int s = 0; s < n; s++) {
      for (int x = x0[s]; x < x1[s]; x++) {
        bool is_black = cf_shade_is_black(cf_strip_shade(s_strip_minute[i++]), x, y);
        if (invert && row.data[x] == hand.argb) { is_black = !is_black; }
        row.data[x] = is_black ? black : white;
      }
    }
  }
  graphics_release_frame_buffer(ctx, fb);
}

// Frame thickness. The SDK's graphics_context_set_stroke_width only supports ODD
// widths (even values round down at draw time), and graphics_draw_rect strokes
// centred on the path, so a width-3 stroke on a path inset 1 px occupies 3 rows.
#define WARN_BORDER_WIDTH 3

// Warning frame just inside the edge strip. Drawn last so nothing occludes it.
static void draw_warn_border(GContext *ctx, GRect bounds) {
  // Feature entirely off (the default): skip the service peeks.
  if (settings.batteryWarnPct == 0 && settings.batteryWarnDaysTenths == 0
      && settings.btWarnBorder == 0) { return; }

  BatteryChargeState charge = battery_state_service_peek();
  int kind = warn_border_kind(charge.charge_percent, charge.is_charging,
                              BatteryDays_currentEstimateTenths(),
                              settings.batteryWarnPct, settings.batteryWarnDaysTenths,
                              bluetooth_connection_service_peek(),
                              settings.btWarnBorder != 0);
  if (kind == WARN_BORDER_NONE) { return; }

  GColor c;
  c.argb = warn_border_color(kind, settings.batteryWarnColor.argb,
                             settings.btWarnColor.argb, theme.timeBgColor.argb);
  graphics_context_set_stroke_color(ctx, c);
  graphics_context_set_stroke_width(ctx, WARN_BORDER_WIDTH);
  graphics_draw_rect(ctx, grect_inset(bounds, GEdgeInsets(CF_STRIP_W + 1)));
}

static void face_update_proc(Layer *l, GContext *ctx) {
  GRect bounds = layer_get_bounds(l);
  GPoint center = GPoint(bounds.size.w / 2, bounds.size.h / 2);
  GColor fg = theme.timeColor, bg = theme.timeBgColor;

  int scene = TestHooks_scene();
  if (scene > 0) { FontSpecimen_draw(ctx, bounds, scene); return; }

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Icons and units sit below the hands, the values above them.
  WidgetLayer_prepare(bounds);
  WidgetLayer_drawIcons(ctx);

  int32_t minute_angle = s_minutes * TRIG_MAX_ANGLE / 60;
  int32_t hour_angle = (s_hours % 12) * TRIG_MAX_ANGLE / 12
                       + s_minutes * TRIG_MAX_ANGLE / 720;
  CfHandPose hour, minute;
  hand_pose(center, hour_angle, CF_HOUR_SHOULDER, CF_HOUR_WIDTH, CF_HOUR_APEX_EXT, &hour);
  hand_pose(center, minute_angle, CF_MINUTE_SHOULDER, CF_MINUTE_WIDTH, CF_MINUTE_APEX_EXT, &minute);

  draw_hand_fill(ctx, &hour, fg);
  draw_hand_border(ctx, &hour, CF_HAND_EDGE_W, fg);

  draw_hand_border(ctx, &minute, CF_HAND_EDGE_W + 2 * CF_HAND_HALO_W, bg);
  draw_hand_fill(ctx, &minute, fg);
  draw_hand_border(ctx, &minute, CF_HAND_EDGE_W, fg);

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_circle(ctx, center, CF_PIVOT_R);

  WidgetLayer_drawValues(ctx);

  draw_strip(ctx, bounds, fg);
  draw_warn_border(ctx, bounds);
}

void ClockFace_init(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  build_strip_table(bounds.size.w, bounds.size.h);
  clock_face_layer = layer_create(bounds);
  layer_set_update_proc(clock_face_layer, face_update_proc);
  layer_add_child(root, clock_face_layer);
}

void ClockFace_deinit(void) {
  layer_destroy(clock_face_layer);
  free(s_strip_minute);
  s_strip_minute = NULL;
}

void ClockFace_redraw(void) {
  layer_mark_dirty(clock_face_layer);
}

void ClockFace_updateTime(struct tm *time_info) {
  s_hours = time_info->tm_hour;
  s_minutes = time_info->tm_min;
  layer_mark_dirty(clock_face_layer);
}

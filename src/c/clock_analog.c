// Analog clock face for TimeStyle.
//
// Ported from the watchface truhanen/pebble-nyquist-watchface
// (https://github.com/truhanen/pebble-nyquist-watchface), GPL-3.0. TimeStyle is
// itself GPL-3.0, so this reuse is license-compatible. Only the clock-face
// drawing (hands + ticks + pivot) is reused; nyquist's corner widgets are not.
//
// Drawn with the plain Pebble GContext graphics API (gpath + graphics_draw_line)
// in LAYER-LOCAL coordinates, so — unlike the FCTX digital path in
// clock_area.c — the layer frame origin is honored automatically and no manual
// offset compensation is needed.
//
// Hand corner coordinates track upstream's "more precisely pre-generated hand
// coordinates" (2026-07): the ideal hand geometry rounded to the nearest pixel.
// We compute the same values at runtime (sin_lookup → float, round once per
// corner) instead of baking per-board tables, so the face stays bounds-scalable.
// See docs/superpowers/specs/2026-07-04-timestyle-analog-nearest-pixel-hands-design.md
// (in the superrepo).
#ifndef PBL_PLATFORM_APLITE
#include <pebble.h>
#include "clock_analog.h"

// integer square root
static int32_t isqrt32(int32_t n) {
  if (n <= 0) return 0;
  int32_t x = n, y = (x + 1) / 2;
  while (y < x) { x = y; y = (x + n / x) / 2; }
  return x;
}

// inner tick point inset by tick_len toward center
static GPoint tick_inner(GPoint outer, GPoint center, int tick_len) {
  int32_t dx = center.x - outer.x;
  int32_t dy = center.y - outer.y;
  int32_t dist = isqrt32(dx * dx + dy * dy);
  if (dist == 0) return outer;
  return (GPoint){
    outer.x + (int32_t)((int64_t)dx * tick_len / dist),
    outer.y + (int32_t)((int64_t)dy * tick_len / dist)
  };
}

// draw all 12 tick marks: halo then body, fixed radius
static void draw_all_ticks(GContext *ctx, GPoint center, int tick_r, int tick_len,
                           int tick_halo_width, int tick_width,
                           GColor halo_color, GColor body_color) {
  for (int i = 0; i < 12; i++) {
    int32_t angle = i * TRIG_MAX_ANGLE / 12;
    int32_t sin_a = sin_lookup(angle);
    int32_t cos_a = cos_lookup(angle);

    GPoint outer = {
      center.x + (int32_t)((int64_t)sin_a * tick_r / TRIG_MAX_RATIO),
      center.y - (int32_t)((int64_t)cos_a * tick_r / TRIG_MAX_RATIO),
    };
    GPoint inner = tick_inner(outer, center, tick_len);

    graphics_context_set_stroke_color(ctx, halo_color);
    graphics_context_set_stroke_width(ctx, tick_halo_width);
    graphics_draw_line(ctx, outer, inner);
    graphics_context_set_stroke_color(ctx, body_color);
    graphics_context_set_stroke_width(ctx, tick_width);
    graphics_draw_line(ctx, outer, inner);
  }
}

// Round a float coordinate to the nearest pixel (ties away from zero). Matches
// nyquist's snap_to_nearest_pixel, so the corners computed below equal the
// nearest-pixel hand coordinates upstream now pre-generates.
static int rnd(float v) {
  return (int)(v >= 0 ? v + 0.5f : v - 0.5f);
}

// Pentagon outline of a hand. The five corners are computed from the integer
// sin_lookup/cos_lookup table converted to float, then each rounded ONCE to the
// nearest pixel — reproducing at runtime the nearest-pixel coordinates upstream
// now pre-generates (see the file header). This replaces the old integer
// trunc/compensated rounding + 1px axis-shift heuristic: the outline now hugs the
// ideal geometry (worst corner ~0.6px vs ~2.4px before) while the face stays
// bounds-scalable across boards. Also used for the minute hand's halo.
static void draw_hand_border(GContext *ctx, GPoint center, int32_t angle,
                             int outer_dist, int half_width, int apex_ext,
                             int tail, int stroke_w, GColor color) {
  float sin_a = (float)sin_lookup(angle) / TRIG_MAX_RATIO;
  float cos_a = (float)cos_lookup(angle) / TRIG_MAX_RATIO;

  float inner_x = center.x - tail * sin_a;
  float inner_y = center.y + tail * cos_a;
  float outer_x = center.x + outer_dist * sin_a;
  float outer_y = center.y - outer_dist * cos_a;
  float side_x  = half_width * cos_a;
  float side_y  = half_width * sin_a;

  GPoint inner_left  = { rnd(inner_x - side_x), rnd(inner_y - side_y) };
  GPoint inner_right = { rnd(inner_x + side_x), rnd(inner_y + side_y) };
  GPoint outer_left  = { rnd(outer_x - side_x), rnd(outer_y - side_y) };
  GPoint outer_right = { rnd(outer_x + side_x), rnd(outer_y + side_y) };
  GPoint apex        = { rnd(outer_x + apex_ext * sin_a),
                         rnd(outer_y - apex_ext * cos_a) };

  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, stroke_w);
  graphics_draw_line(ctx, inner_left,  inner_right);
  graphics_draw_line(ctx, inner_right, outer_right);
  graphics_draw_line(ctx, inner_left,  outer_left);
  graphics_draw_line(ctx, outer_right, apex);
  graphics_draw_line(ctx, outer_left,  apex);
}

// filled pentagon body of a hand
static void draw_hand_fill(GContext *ctx, GPoint center, int32_t angle,
                           int length, int width, int tail, GColor color) {
  // A file-static path instead of gpath_create/gpath_destroy per call: this runs twice
  // per frame (hour + minute) and the watchface redraws constantly, so it was two heap
  // alloc/free pairs every frame. GPath is a public 4-field struct and gpath_create only
  // copies the points POINTER (per the SDK docs), so a static instance whose points are
  // rewritten before each draw is equivalent. gpath_rotate_to/gpath_move_to just set
  // fields, and the Pebble event loop is single-threaded, so sharing one is safe.
  static GPoint s_pts[5];
  static GPath s_path = { .num_points = 5, .points = s_pts, .rotation = 0,
                          .offset = { 0, 0 } };
  int hw = width / 2;
  s_pts[0] = (GPoint){ -hw, tail };
  s_pts[1] = (GPoint){ hw, tail };
  s_pts[2] = (GPoint){ hw, -length };
  s_pts[3] = (GPoint){ 0, -(length + hw) };
  s_pts[4] = (GPoint){ -hw, -length };
  gpath_rotate_to(&s_path, angle);
  gpath_move_to(&s_path, center);
  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, &s_path);
}

// Scale an emery-reference dimension (a percent of a 100px half-screen) to this
// face's `half`; floor strokes at 1px when floor1 is set.
static int dim(int half, int pct, int floor1) {
  int v = half * pct / 100;
  if (floor1 && v < 1) v = 1;
  return v;
}

void ClockAnalog_draw(GContext *ctx, GRect bounds, int hours, int minutes,
                      GColor fg, GColor bg, int tick_style) {
  int w = bounds.size.w;
  int h = bounds.size.h;
  GPoint center = GPoint(bounds.origin.x + w / 2, bounds.origin.y + h / 2);
  int half = (w < h ? w : h) / 2;

  // Dimensions derived from `half` using nyquist's emery ratios (emery half~100).
  int tick_r         = dim(half, 85, 0);
  int minute_len     = dim(half, 89, 0);
  int hour_len       = dim(half, 60, 0);
  int hand_edge_w    = dim(half, 12, 1);
  int hand_halo_w    = dim(half, 2,  1);
  int minute_outer_w = dim(half, 24, 0);
  int minute_fill_w  = dim(half, 12, 0);
  int hour_outer_w   = dim(half, 33, 0);
  int hour_fill_w    = dim(half, 22, 0);
  int hand_tail      = dim(half, 18, 0);
  // tick_style: 0=hide, 1=normal, 2=bold (mirrors ANALOG_TICKS_* in settings.h).
  // Bold widens the body + halo and lengthens the tick so it reads clearly.
  bool bold_ticks    = (tick_style == 2);
  int tick_w         = dim(half, bold_ticks ? 4 : 2, 1);
  int tick_halo_w    = dim(half, bold_ticks ? 8 : 6, 1);
  int tick_len       = dim(half, bold_ticks ? 9 : 6, 1);
  int pivot_r        = dim(half, 3,  1);

  int32_t min_angle  = minutes * TRIG_MAX_ANGLE / 60;
  int32_t hour_angle = (hours % 12) * TRIG_MAX_ANGLE / 12
                       + minutes * TRIG_MAX_ANGLE / 720;

  // background (matches the window bg; also needed for the halo/pivot gaps)
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // hour hand
  draw_hand_border(ctx, center, hour_angle,
                   hour_len - hand_edge_w / 2,
                   hour_outer_w / 2 - hand_edge_w / 2,
                   hour_outer_w / 2 - hand_edge_w / 2,
                   hand_tail, hand_edge_w, fg);
  draw_hand_fill(ctx, center, hour_angle,
                 hour_len - hand_edge_w / 2 - 1, hour_fill_w, hand_tail, fg);

  // minute hand: halo border, body border, fill
  draw_hand_border(ctx, center, min_angle,
                   minute_len - hand_edge_w / 2,
                   minute_outer_w / 2 - hand_edge_w / 2,
                   minute_outer_w / 2 - hand_edge_w / 2,
                   hand_tail, hand_edge_w + 2 * hand_halo_w, bg);
  draw_hand_border(ctx, center, min_angle,
                   minute_len - hand_edge_w / 2,
                   minute_outer_w / 2 - hand_edge_w / 2,
                   minute_outer_w / 2 - hand_edge_w / 2,
                   hand_tail, hand_edge_w, fg);
  draw_hand_fill(ctx, center, min_angle,
                 minute_len - hand_edge_w / 2 - 1, minute_fill_w, hand_tail, fg);

  // center pivot
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_circle(ctx, center, pivot_r);

  // ticks last, so they sit on top of the hands (skipped when tick_style == hide)
  if (tick_style != 0) {
    draw_all_ticks(ctx, center, tick_r, tick_len, tick_halo_w, tick_w, bg, fg);
  }
}
#endif  // !PBL_PLATFORM_APLITE

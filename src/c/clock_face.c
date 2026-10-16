// Clock face: hour + minute hands, centre pivot, the 60 minute-boundary ticks along
// the screen edges (separate, 5 px long on the outer edge, following the frame's rounded corners (CF_CORNER_R), with radial sides, 2 px background shadow, all in the
// time text colour: 4 px deep for 5-minute ticks and 1 px deep and 1 px in from the edge for 1-minute ticks) and the widgets (icons below
// the hands, values and icon-only widgets above), all on one full-screen layer.
// The hands' hub (pivot) moves vertically with the minute (cf_pivot_y); the tick table is
// rebuilt from the pivot whenever its integer y changes.
//
// Hand dimensions are those of the Nyquist watchface (truhanen/
// pebble-nyquist-watchface, GPL-3.0); the minute hand is lengthened: by default its tip just
// touches the screen edge it points at; optionally it runs off the screen. Each hand is a pentagon whose five corners are computed from
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
static int s_w, s_h;

// Pivot y: the middle of the screen, or the moving centre (cf_pivot_y) for the current minute.
static int pivot_y(void) {
  return settings.fixedCentre ? s_h / 2 : cf_pivot_y(s_w, s_h, s_minutes);
}

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

// Colour of every tick pixel, and the shadow pixels around the ticks, all computed
// once at init.
typedef struct { uint8_t x, y; } TickPixel;
typedef struct { uint8_t x, y; } ShadowPixel;
static TickPixel s_tick_pixels[CF_TICK_COUNT * CF_TICK_MAX_PIXELS];
static int s_tick_pixel_count;
static ShadowPixel s_shadow_pixels[CF_TICK_COUNT * CF_SHADOW_MAX_PER_TICK];
static int s_shadow_pixel_count;

static int s_tick_cy = -1;   // pivot y the tick table was built for

// Band bitmaps for the shadow computation (is-tick and seen), static: no heap use.
#define SCREEN_W 200
#define SCREEN_H 228
static uint8_t s_band[2 * ((2 * CF_BAND_DEPTH * SCREEN_W + (SCREEN_H - 2 * CF_BAND_DEPTH) * 2 * CF_BAND_DEPTH + 7) / 8)];

// Builds the tick and shadow tables (row-sorted) for rays from (w/2, cy), with no heap.
static void build_tick_table(int w, int h, int cy) {
  int n = 0;
  for (int b = 0; b < CF_TICK_COUNT; b++) {
    int32_t angle = b * TRIG_MAX_ANGLE / CF_TICK_COUNT;
    float sin_a = (float)sin_lookup(angle) / TRIG_MAX_RATIO;
    float cos_a = (float)cos_lookup(angle) / TRIG_MAX_RATIO;
    float hx, hy;
    cf_boundary_hit_from(w, h, CF_CORNER_R, w / 2.0f, (float)cy, sin_a, cos_a, &hx, &hy);
    CfPoint px[CF_TICK_MAX_PIXELS];
    // Major (5-minute) ticks are CF_STRIP_W deep; minor ticks are CF_MINOR_TICK_DEPTH deep.
    bool is_major = cf_tick_is_major(b);
    int depth = is_major ? CF_STRIP_W : CF_MINOR_TICK_DEPTH;
    int inset = is_major ? 0 : CF_MINOR_TICK_INSET;
    int count = cf_tick_pixels_from(w, h, CF_CORNER_R, w / 2.0f, (float)cy, hx, hy, depth, inset, px);
    // All ticks use the foreground colour (resolved at draw time).
    for (int i = 0; i < count; i++) {
      s_tick_pixels[n++] = (TickPixel){ (uint8_t)px[i].x, (uint8_t)px[i].y };
    }
  }
  s_tick_pixel_count = n;
  int misses;
  s_shadow_pixel_count = cf_shadow_band(w, h, (const uint8_t *)s_tick_pixels, n, s_band, sizeof s_band,
                                        (uint8_t *)s_shadow_pixels,
                                        CF_TICK_COUNT * CF_SHADOW_MAX_PER_TICK, &misses);
  // Row-major order: draw_ticks then re-fetches the row info only once per row.
  cf_sort_xy((uint8_t *)s_tick_pixels, s_tick_pixel_count);
  cf_sort_xy((uint8_t *)s_shadow_pixels, s_shadow_pixel_count);
  s_tick_cy = cy;
}

// Writes the shadow pixels (background colour), then the tick pixels, straight into
// the framebuffer over whatever is underneath (including the minute hand). Everything
// else in the strip area is left as drawn. All ticks use the foreground colour.
static void draw_ticks(GContext *ctx, GColor bg, GColor fg) {
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) { return; }
  int last_y = -1;
  GBitmapDataRowInfo row = { 0 };
  for (int i = 0; i < s_shadow_pixel_count; i++) {
    const ShadowPixel *p = &s_shadow_pixels[i];
    if (p->y != last_y) { row = gbitmap_get_data_row_info(fb, p->y); last_y = p->y; }
    row.data[p->x] = bg.argb;
  }
  for (int i = 0; i < s_tick_pixel_count; i++) {
    const TickPixel *p = &s_tick_pixels[i];
    if (p->y != last_y) { row = gbitmap_get_data_row_info(fb, p->y); last_y = p->y; }
    row.data[p->x] = fg.argb;
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
  s_w = bounds.size.w; s_h = bounds.size.h;
  int cy = pivot_y();
  GPoint center = GPoint(s_w / 2, cy);
  // Rebuilt only when the integer pivot y changed (once a minute while it glides, never
  // with the fixed centre).
  if (cy != s_tick_cy) { build_tick_table(s_w, s_h, cy); }
  GColor fg = theme.timeColor, bg = theme.timeBgColor;

  int scene = TestHooks_scene();
  if (scene > 0) { FontSpecimen_draw(ctx, bounds, scene); return; }

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Icons and units of value widgets sit below the hands; the values and the icon-only widgets above them.
  WidgetLayer_prepare(bounds);
  WidgetLayer_drawIcons(ctx);

  int32_t minute_angle = s_minutes * TRIG_MAX_ANGLE / 60;
  int32_t hour_angle = (s_hours % 12) * TRIG_MAX_ANGLE / 12
                       + s_minutes * TRIG_MAX_ANGLE / 720;
  CfHandPose hour, minute;
  hand_pose(center, hour_angle, CF_HOUR_SHOULDER, CF_HOUR_WIDTH, CF_HOUR_APEX_EXT, &hour);
  hand_pose(center, minute_angle, CF_MINUTE_SHOULDER(settings.minuteOffscreen), CF_MINUTE_WIDTH, CF_MINUTE_APEX_EXT, &minute);

  // Both hands get a bg halo so they separate from the widget icons/units below them.
  draw_hand_border(ctx, &hour, CF_HAND_EDGE_W + 2 * CF_HAND_HALO_W, bg);
  draw_hand_fill(ctx, &hour, fg);
  draw_hand_border(ctx, &hour, CF_HAND_EDGE_W, fg);

  draw_hand_border(ctx, &minute, CF_HAND_EDGE_W + 2 * CF_HAND_HALO_W, bg);
  draw_hand_fill(ctx, &minute, fg);
  draw_hand_border(ctx, &minute, CF_HAND_EDGE_W, fg);

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_circle(ctx, center, CF_PIVOT_R);

  WidgetLayer_drawValues(ctx, &hour, &minute, center);

  draw_ticks(ctx, bg, fg);
  draw_warn_border(ctx, bounds);
}

void ClockFace_init(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_w = bounds.size.w; s_h = bounds.size.h;
  build_tick_table(s_w, s_h, pivot_y());
  clock_face_layer = layer_create(bounds);
  layer_set_update_proc(clock_face_layer, face_update_proc);
  layer_add_child(root, clock_face_layer);
}

void ClockFace_deinit(void) {
  layer_destroy(clock_face_layer);
}

void ClockFace_redraw(void) {
  layer_mark_dirty(clock_face_layer);
}

void ClockFace_updateTime(struct tm *time_info) {
  s_hours = time_info->tm_hour;
  s_minutes = time_info->tm_min;
  layer_mark_dirty(clock_face_layer);
}

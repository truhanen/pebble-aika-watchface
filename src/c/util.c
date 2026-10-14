#include <pebble.h>
#include "util.h"
#include "test_hooks.h"
#include "widget_layout_calc.h"


bool recolor_iterator_cb(GDrawCommand *command, uint32_t index, void *context) {
  GColor *colors = (GColor *)context;

  gdraw_command_set_fill_color(command, colors[0]);
  gdraw_command_set_stroke_color(command, colors[1]);

  return true;
}

void gdraw_command_image_recolor(GDrawCommandImage *img, GColor fill_color, GColor stroke_color) {
  GColor colors[2];
  colors[0] = fill_color;
  colors[1] = stroke_color;

  gdraw_command_list_iterate(gdraw_command_image_get_command_list(img),
                             recolor_iterator_cb, &colors);
}

// PDC points are 1/8 px for precise paths (every icon here), whole px otherwise.
typedef struct {
  int min_x, max_x, min_y, max_y, stroke, per_px;
  WlIconScale scale;
  bool any;
} IconFit;

static int icon_per_px(const GDrawCommand *c) {
  return gdraw_command_get_type((GDrawCommand *)c) == GDrawCommandTypePrecisePath ? 8 : 1;
}

static bool icon_extent_cb(GDrawCommand *c, uint32_t index, void *context) {
  IconFit *f = context;
  f->per_px = icon_per_px(c);
  if (gdraw_command_get_stroke_width(c) > f->stroke) { f->stroke = gdraw_command_get_stroke_width(c); }
  for (uint16_t i = 0; i < gdraw_command_get_num_points(c); i++) {
    GPoint p = gdraw_command_get_point(c, i);
    if (!f->any || p.x < f->min_x) { f->min_x = p.x; }
    if (!f->any || p.x > f->max_x) { f->max_x = p.x; }
    if (!f->any || p.y < f->min_y) { f->min_y = p.y; }
    if (!f->any || p.y > f->max_y) { f->max_y = p.y; }
    f->any = true;
  }
  return true;
}

static bool icon_scale_cb(GDrawCommand *c, uint32_t index, void *context) {
  IconFit *f = context;
  int origin = f->stroke * f->per_px / 2;
  for (uint16_t i = 0; i < gdraw_command_get_num_points(c); i++) {
    GPoint p = gdraw_command_get_point(c, i);
    gdraw_command_set_point(c, i, GPoint(wl_icon_coord(p.x, f->min_x, origin, f->scale),
                                         wl_icon_coord(p.y, f->min_y, origin, f->scale)));
  }
  return true;
}

GDrawCommandImage *icon_image_create_with_resource(uint32_t resource_id) {
  GDrawCommandImage *img = gdraw_command_image_create_with_resource(resource_id);
  if (!img) { return NULL; }
  GDrawCommandList *list = gdraw_command_image_get_command_list(img);
  IconFit f = { .per_px = 8 };
  gdraw_command_list_iterate(list, icon_extent_cb, &f);
  if (!f.any || f.max_y <= f.min_y) { return img; }
  f.scale = wl_icon_scale(f.max_y - f.min_y, f.per_px, f.stroke, WL_ICON_H);
  gdraw_command_list_iterate(list, icon_scale_cb, &f);
  gdraw_command_image_set_bounds_size(
      img, GSize(wl_icon_width_px(f.max_x - f.min_x, f.per_px, f.stroke, f.scale), WL_ICON_H));
  return img;
}

int time_get_beats(const struct tm *tm) {
  // get utc time
  time_t utc_time = mktime((struct tm *)tm);

  // convert to "biel mean time" (BMT) by adding an hour
  time_t bmt_time = utc_time + 3600;

  // use gmtime to get the BMT components
  struct tm *bmt_tm = gmtime(&bmt_time);

  // how many seconds have passed since midnight?
  int seconds_since_midnight = bmt_tm->tm_hour * 3600 + bmt_tm->tm_min * 60 + bmt_tm->tm_sec;
  int beats = (seconds_since_midnight * 1000 + 43200) / 86400;

  return beats % 1000;
}

bool is_health_metric_accessible(HealthMetric metric) {
  time_t start = time_start_of_today();
  time_t end = time(NULL);

  // Check step data is available
     HealthServiceAccessibilityMask mask = health_service_metric_accessible(metric, start, end);
  bool result = mask & HealthServiceAccessibilityMaskAvailable;

  return result;
}

bool is_user_sleeping() {
  // currently disabled
  return false;
  //
  //  time_t now = time(NULL);
  //
    //  HealthActivityMask activities = HealthActivitySleep | HealthActivityRestfulSleep;
  //
    //  HealthServiceAccessibilityMask mask = health_service_any_activity_accessible(activities, now, now);
    //  bool sleep_access_available = mask & HealthServiceAccessibilityMaskAvailable;
  //
  //  if(sleep_access_available) {
  //    uint32_t current_activities = health_service_peek_current_activities();
    //    bool sleeping = current_activities & HealthActivitySleep || current_activities & HealthActivityRestfulSleep;
  //
  //    return sleeping;
  //  } else {
  //    return false;
  //  }
}

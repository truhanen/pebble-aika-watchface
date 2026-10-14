// Widgets, on the new measure / draw_icon / draw_value API (see widgets.h).
//
// Every ported widget is a WidgetDesc: how to format its value, and what stands beside
// it (an icon, or a unit). The three Widget functions are generic and look the
// descriptor up by id. Ids without a descriptor draw nothing yet, except EMPTY, which
// reserves the ink height of its font. Value strings keep the data access (health,
// weather, price tables, phone slots) of the sidebar widgets they replace.
#include "widgets.h"
#include "settings.h"
#include "languages.h"
#include "theme.h"
#include "util.h"
#include "weather.h"
#include "electricity.h"
#include "electricity_calc.h"
#include "crypto.h"
#include "currency.h"
#include "tuya.h"
#include "clock_face.h"
#include "widget_text.h"
#include "widget_layout_calc.h"
#include "test_hooks.h"
#include <math.h>
#include <string.h>

typedef enum { ICON_NONE, ICON_BATTERY, ICON_BT, ICON_STEPS, ICON_HEART, ICON_WEATHER, ICON_UNIT } IconKind;

typedef struct {
  bool (*value)(const WidgetCtx *c, char *buf, size_t n);   // false = no value text
  IconKind icon;
  const char *(*unit)(const WidgetCtx *c);                   // ICON_UNIT label
} WidgetDesc;

static GDrawCommandImage *s_bt_image, *s_steps_image, *s_heart_image;

// UTC time of the most recent heart-rate reading; 0 = unknown.
static time_t s_last_hr_update_time = 0;
// BPM of that most recent reading; 0 = unknown. Used as a fallback when the
// live peek has no value yet (e.g. right after a cold start / install).
static int s_last_hr_bpm = 0;

// Seed s_last_hr_update_time/s_last_hr_bpm from the last hour of minute history
// so the fallback BPM is correct immediately at launch, not only after the first
// in-session update.
static void hr_seed_from_history() {
  time_t now = time(NULL);
  time_t start = now - 3600;
  time_t end = now;
  HealthMinuteData minute_data[60];
  uint32_t num = health_service_get_minute_history(minute_data, 60, &start, &end);
  // Records are oldest-first; `start` is updated to the first record's time.
  for (int i = (int)num - 1; i >= 0; i--) {
    if (!minute_data[i].is_invalid && minute_data[i].heart_rate_bpm > 0) {
      s_last_hr_update_time = start + (time_t)i * 60;
      s_last_hr_bpm = minute_data[i].heart_rate_bpm;
      break;
    }
  }
}

static void hr_health_handler(HealthEventType event, void *context) {
  if (event == HealthEventHeartRateUpdate) {
    int bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
    // HealthEventHeartRateUpdate also fires when HealthMetricHeartRateRawBPM changes,
    // a cycle before the filtered HealthMetricHeartRateBPM recomputes; only treat it
    // as a new reading when the displayed filtered value actually changes.
    if (bpm > 0 && bpm != s_last_hr_bpm) {
      s_last_hr_update_time = time(NULL);
      s_last_hr_bpm = bpm;
    }
    ClockFace_redraw();
  }
}

void Widgets_init(void) {
  s_bt_image = gdraw_command_image_create_with_resource(RESOURCE_ID_DISCONNECTED);
  s_steps_image = gdraw_command_image_create_with_resource(RESOURCE_ID_HEALTH_STEPS);
  s_heart_image = gdraw_command_image_create_with_resource(RESOURCE_ID_HEALTH_HEART);
  health_service_events_subscribe(hr_health_handler, NULL);
  hr_seed_from_history();
}

void Widgets_deinit(void) {
  gdraw_command_image_destroy(s_bt_image);
  gdraw_command_image_destroy(s_steps_image);
  gdraw_command_image_destroy(s_heart_image);
  health_service_events_unsubscribe();
}

/********** values **********/

static uint8_t battery_percent(void) {
  BatteryChargeState st = battery_state_service_peek();
  return (st.charge_percent > 0) ? st.charge_percent : 5;
}

static bool value_battery(const WidgetCtx *c, char *buf, size_t n) {
  if (!settings.showBatteryPct) { return false; }
  // the percent sign goes first in Turkish
  snprintf(buf, n, (settings.languageId == LANGUAGE_TR) ? "%%%d" : "%d%%", battery_percent());
  return true;
}

static bool value_steps(const WidgetCtx *c, char *buf, size_t n) {
  int steps = 0;
  if (is_health_metric_accessible(HealthMetricStepCount)) {
    steps = (int)health_service_sum_today(HealthMetricStepCount);
  }
#ifdef SCREENSHOT_FIXTURES
  steps = 1454;   // demo: renders "1.4k"
#endif
  if (steps < 1000) {
    snprintf(buf, n, "%i", steps);
  } else if (steps < 10000) {
    snprintf(buf, n, "%i%c%ik", steps / 1000, settings.decimalSeparator, steps / 100 % 10);
  } else {
    snprintf(buf, n, "%ik", steps / 1000);
  }
  return true;
}

// Distance as a bare number in the display unit's own scale: one decimal below 10, a
// whole number from 10 (0.0 / 0.5 / 5.0 / 9.9 / 12).
static bool value_distance(const WidgetCtx *c, char *buf, size_t n) {
  int distance = 0;
  if (is_health_metric_accessible(HealthMetricWalkedDistanceMeters)) {
    distance = (int)health_service_sum_today(HealthMetricWalkedDistanceMeters);
  }
  bool metric = health_service_get_measurement_system_for_display(
                    HealthMetricWalkedDistanceMeters) == MeasurementSystemMetric;
  int tenths = metric ? distance / 100 : distance * 10 / 1609;   // tenths of a km / mile
  if (tenths < 100) {
    snprintf(buf, n, "%i%c%i", tenths / 10, settings.decimalSeparator, tenths % 10);
  } else {
    snprintf(buf, n, "%i", tenths / 10);
  }
  return true;
}

static bool value_heart(const WidgetCtx *c, char *buf, size_t n) {
  int bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
  // the live peek has no value right after a cold start; use the last reading
  if (bpm <= 0) { bpm = s_last_hr_bpm; }
  snprintf(buf, n, "%i", bpm);
  return true;
}

static bool value_weather(const WidgetCtx *c, char *buf, size_t n) {
  if (Weather_weatherInfo.currentTemp == INT32_MIN) {
    snprintf(buf, n, "--\xc2\xb0");
    return true;
  }
  int temp = Weather_weatherInfo.currentTemp;
  if (!settings.useMetric) { temp = (int)roundf(temp * 1.8f + 32); }
  snprintf(buf, n, "%d\xc2\xb0", temp);
  return true;
}

static bool value_electricity(const WidgetCtx *c, char *buf, size_t n) {
  int16_t v;
  if (Electricity_getCurrentPrice(&v)) {
    elec_format_price(v, settings.decimalSeparator, buf, n);
  } else {
    snprintf(buf, n, "--");
  }
  return true;
}

static const char *unit_electricity(const WidgetCtx *c) { return "c/kWh"; }

static CryptoSlot *data_slot(uint8_t wid) {
  return Crypto_isWid(wid) ? Crypto_find(wid)
       : Currency_isWid(wid) ? Currency_find(wid)
       : Tuya_find(wid);
}

static bool value_slot(const WidgetCtx *c, char *buf, size_t n) {
  CryptoSlot *s = data_slot(c->id);
  snprintf(buf, n, "%s", (s && s->valid) ? s->value : "--");
  return true;
}

static const char *unit_slot(const WidgetCtx *c) {
  CryptoSlot *s = data_slot(c->id);
  return (s && s->label[0]) ? s->label : "--";
}

static const WidgetDesc DESC_EMPTY = { NULL, ICON_NONE, NULL };
static const WidgetDesc DESC_BT = { NULL, ICON_BT, NULL };
static const WidgetDesc DESC_BATTERY = { value_battery, ICON_BATTERY, NULL };
static const WidgetDesc DESC_STEPS = { value_steps, ICON_STEPS, NULL };
static const WidgetDesc DESC_DISTANCE = { value_distance, ICON_STEPS, NULL };
static const WidgetDesc DESC_HEART = { value_heart, ICON_HEART, NULL };
static const WidgetDesc DESC_WEATHER = { value_weather, ICON_WEATHER, NULL };
static const WidgetDesc DESC_ELECTRICITY = { value_electricity, ICON_UNIT, unit_electricity };
static const WidgetDesc DESC_SLOT = { value_slot, ICON_UNIT, unit_slot };

static const WidgetDesc *desc_for(uint8_t id) {
  if (Crypto_isWid(id) || Currency_isWid(id) || Tuya_isWid(id)) { return &DESC_SLOT; }
  switch (id) {
    case EMPTY:                return &DESC_EMPTY;
    case BLUETOOTH_DISCONNECT: return &DESC_BT;
    case BATTERY_METER:        return &DESC_BATTERY;
    case STEP_COUNTER:         return &DESC_STEPS;
    case DISTANCE:             return &DESC_DISTANCE;
    case HEARTRATE:            return &DESC_HEART;
    case WEATHER_CURRENT:      return &DESC_WEATHER;
    case ELECTRICITY:          return &DESC_ELECTRICITY;
    default:                   return NULL;   // not ported yet: draws nothing
  }
}

/********** icons **********/

// The battery is drawn horizontally from primitives: body outline, nub on the right,
// a fill bar by percent and, while charging, a bolt with a bg halo so it reads over
// both the filled and the empty part.
#define BATTERY_W 22
#define BATTERY_H 11
#define BATTERY_INNER_W 16

static GDrawCommandImage *icon_image(IconKind kind) {
  switch (kind) {
    case ICON_BT:      return s_bt_image;
    case ICON_STEPS:   return s_steps_image;
    case ICON_HEART:   return s_heart_image;
    case ICON_WEATHER: return Weather_currentWeatherIcon;
    default:           return NULL;
  }
}

static void draw_battery(GContext *g, GRect box, GColor fg, GColor bg) {
  BatteryChargeState st = battery_state_service_peek();
  int x = box.origin.x, y = box.origin.y;
  graphics_context_set_stroke_color(g, fg);
  graphics_context_set_stroke_width(g, 1);
  graphics_draw_rect(g, GRect(x, y, BATTERY_W - 2, BATTERY_H));
  graphics_context_set_fill_color(g, fg);
  graphics_fill_rect(g, GRect(x + BATTERY_W - 2, y + 3, 2, BATTERY_H - 6), 0, GCornerNone);
  int fill = (BATTERY_INNER_W * battery_percent() + 50) / 100;
  if (fill < 1) { fill = 1; }
  graphics_fill_rect(g, GRect(x + 2, y + 2, fill, BATTERY_H - 4), 0, GCornerNone);
  if (st.is_charging) {
    // a zig-zag bolt with a bg halo, so it reads over the filled and the empty part
    static const int8_t BOLT[4][2] = { { 13, 2 }, { 9, 5 }, { 13, 5 }, { 9, 8 } };
    static const int8_t DIRS[5][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }, { 0, 0 } };
    graphics_context_set_antialiased(g, false);   // keep the 1 px diagonals crisp
    for (int d = 0; d < 5; d++) {
      graphics_context_set_stroke_color(g, d < 4 ? bg : fg);
      for (int i = 0; i < 3; i++) {
        graphics_draw_line(g, GPoint(x + BOLT[i][0] + DIRS[d][0], y + BOLT[i][1] + DIRS[d][1]),
                           GPoint(x + BOLT[i + 1][0] + DIRS[d][0], y + BOLT[i + 1][1] + DIRS[d][1]));
      }
    }
    graphics_context_set_antialiased(g, true);
  }
}

/********** the Widget functions **********/

static void measure(const WidgetCtx *c, WidgetSize *out) {
  memset(out, 0, sizeof(*out));
  const WidgetDesc *d = desc_for(c->id);
  if (!d) { return; }
  out->ink_h = (int16_t)WidgetText_inkHeight(c->font);
  char buf[24];
  if (d->value && d->value(c, buf, sizeof(buf))) {
    out->value_w = (int16_t)WidgetText_width(buf, c->font);
  }
  if (c->hide_icon) { return; }
  if (d->icon == ICON_BATTERY) {
    out->icon_w = BATTERY_W;
    out->icon_h = BATTERY_H;
  } else if (d->icon == ICON_UNIT) {
    int w, h;
    WidgetUnit_measure(d->unit(c), &w, &h);
    out->icon_w = (int16_t)w;
    out->icon_h = (int16_t)h;
  } else if (d->icon != ICON_NONE) {
    GDrawCommandImage *img = icon_image(d->icon);
    if (img) {
      GSize s = gdraw_command_image_get_bounds_size(img);
      out->icon_w = s.w;
      out->icon_h = s.h;
    }
  }
}

static void draw_icon(GContext *g, const WidgetCtx *c, GRect box) {
  const WidgetDesc *d = desc_for(c->id);
  if (!d || c->hide_icon) { return; }
  GColor fg = theme.timeColor, bg = theme.timeBgColor;
  if (d->icon == ICON_BATTERY) {
    draw_battery(g, box, fg, bg);
  } else if (d->icon == ICON_UNIT) {
    WidgetUnit_draw(g, d->unit(c), box, fg);
  } else if (d->icon != ICON_NONE) {
    GDrawCommandImage *img = icon_image(d->icon);
    if (img) {
      gdraw_command_image_recolor(img, theme.iconFillColor, theme.iconStrokeColor);
      gdraw_command_image_draw(g, img, box.origin);
    }
  }
}

static void draw_value(GContext *g, const WidgetCtx *c, GRect box, GTextAlignment align) {
  const WidgetDesc *d = desc_for(c->id);
  char buf[24];
  if (!d || !d->value || !d->value(c, buf, sizeof(buf))) { return; }
  WidgetText_drawValue(g, buf, c->font, box.origin.x, box.origin.y, theme.timeColor, theme.timeBgColor);
}

static const Widget WIDGET = { measure, draw_icon, draw_value };

const Widget *Widgets_get(uint8_t id) {
  (void)id;
  return &WIDGET;
}

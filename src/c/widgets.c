// Widgets, on the measure / draw_icon / draw_value API (see widgets.h).
//
// Every widget is a WidgetDesc: how to format its value, and what stands beside it (an
// icon, or a unit). The three Widget functions are generic and look the descriptor up by
// id. EMPTY has neither and just reserves the ink height of its font; a widget with an
// icon and no value (the icon-only ones) is as high as its icon. Value strings keep the
// data access (health, weather, price tables, phone slots, the face's time) of the
// sidebar widgets they replace.
#include "widgets.h"
#include "settings.h"
#include "languages.h"
#include "theme.h"
#include "util.h"
#include "weather.h"
#include "rain_calc.h"
#include "electricity.h"
#include "electricity_calc.h"
#include "crypto.h"
#include "currency.h"
#include "tuya.h"
#include "tuya_leds.h"
#include "battery_days.h"
#include "sleep_calc.h"
#include "clock_face.h"
#include "widget_text.h"
#include "widget_layout_calc.h"
#include "clock_face_calc.h"
#include "test_hooks.h"
#include "text_case.h"
#include <math.h>
#include <string.h>

typedef enum {
  ICON_NONE, ICON_BATTERY, ICON_BT_OFF, ICON_BT_STATE, ICON_STEPS, ICON_HEART, ICON_WEATHER,
  ICON_FORECAST, ICON_RAIN, ICON_SLEEP, ICON_DEEP_SLEEP, ICON_SLEEP_BAR, ICON_LEDS
} IconKind;

typedef struct {
  bool (*value)(const WidgetCtx *c, char *buf, size_t n);   // false = no value text now;
                                                            // NULL = a widget without a value
  IconKind icon;                                            // ICON_NONE = the widget has no icon
  bool (*unit)(const WidgetCtx *c, char *buf, size_t n);    // unit label; NULL = no unit;
                                                            // false = none to show now
} WidgetDesc;

static GDrawCommandImage *s_steps_image, *s_heart_image, *s_sleep_image, *s_deep_sleep_image, *s_rain_image;

// The time the date / time widgets show: the face's own, test-hook override included.
static struct tm s_tm;

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
  s_steps_image = icon_image_create_with_resource(RESOURCE_ID_HEALTH_STEPS);
  s_heart_image = icon_image_create_with_resource(RESOURCE_ID_HEALTH_HEART);
  s_sleep_image = icon_image_create_with_resource(RESOURCE_ID_HEALTH_SLEEP);
  s_deep_sleep_image = icon_image_create_with_resource(RESOURCE_ID_HEALTH_DEEP_SLEEP);
  s_rain_image = icon_image_create_with_resource(RESOURCE_ID_WEATHER_LIGHT_RAIN);
  health_service_events_subscribe(hr_health_handler, NULL);
  hr_seed_from_history();
}

void Widgets_deinit(void) {
  gdraw_command_image_destroy(s_steps_image);
  gdraw_command_image_destroy(s_heart_image);
  gdraw_command_image_destroy(s_sleep_image);
  gdraw_command_image_destroy(s_deep_sleep_image);
  gdraw_command_image_destroy(s_rain_image);
  health_service_events_unsubscribe();
}

void Widgets_updateTime(const struct tm *t) { s_tm = *t; }

/********** values **********/

static uint8_t battery_percent(void) {
  BatteryChargeState st = battery_state_service_peek();
  return (st.charge_percent > 0) ? st.charge_percent : 5;
}

static const WidgetDesc *desc_for(uint8_t id);

// What stands beside the widget's value right now: the requested identifier mode, or
// the fallback when the widget has not got it (widget_list.h).
static WlIdent ident_of(const WidgetCtx *c) {
  const WidgetDesc *d = desc_for(c->id);
  return d ? WidgetList_resolveIdent((WlIdent)c->ident, d->icon != ICON_NONE, d->unit != NULL)
           : WL_IDENT_NONE;
}

// Widget 2: the number. In UNIT mode the unit is "%".
static bool value_battery(const WidgetCtx *c, char *buf, size_t n) {
  snprintf(buf, n, "%d", battery_percent());
  return true;
}

static bool unit_battery(const WidgetCtx *c, char *buf, size_t n) {
  snprintf(buf, n, "%%");
  return true;
}

// Estimated days left, one decimal. The estimate is meaningless while charging: the
// icon's bolt says it all then.
static bool value_battery_days(const WidgetCtx *c, char *buf, size_t n) {
  if (battery_state_service_peek().is_charging) { return false; }
  int tenths = BatteryDays_currentEstimateTenths();
  if (tenths == BATTERY_DAYS_NONE) {
    snprintf(buf, n, "--");   // warm-up / not enough data
  } else {
    snprintf(buf, n, "%d%c%d", tenths / 10, settings.decimalSeparator, tenths % 10);
  }
  return true;
}

static bool unit_battery_days(const WidgetCtx *c, char *buf, size_t n) {
  if (battery_state_service_peek().is_charging) { return false; }   // no value, so no unit
  snprintf(buf, n, "d");
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

static bool unit_steps(const WidgetCtx *c, char *buf, size_t n) { snprintf(buf, n, "steps"); return true; }

static bool unit_distance(const WidgetCtx *c, char *buf, size_t n) {
  bool metric = health_service_get_measurement_system_for_display(
                    HealthMetricWalkedDistanceMeters) == MeasurementSystemMetric;
  snprintf(buf, n, metric ? "km" : "mi");
  return true;
}

static bool unit_heart(const WidgetCtx *c, char *buf, size_t n) { snprintf(buf, n, "bpm"); return true; }

static bool value_heart(const WidgetCtx *c, char *buf, size_t n) {
  int bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
  // the live peek has no value right after a cold start; use the last reading
  if (bpm <= 0) { bpm = s_last_hr_bpm; }
  snprintf(buf, n, "%i", bpm);
  return true;
}

static int to_display_temp(int celsius) {
  return settings.useMetric ? celsius : (int)roundf(celsius * 1.8f + 32);
}

// The temperature carries its own degree sign only beside its icon; with the unit
// ("°C" / "°F") or with no identifier it is the bare number.
static const char *degree(const WidgetCtx *c) {
  return ident_of(c) != WL_IDENT_ICON ? "" : "\xc2\xb0";
}

static bool value_weather(const WidgetCtx *c, char *buf, size_t n) {
  if (Weather_weatherInfo.currentTemp == INT32_MIN) {
    snprintf(buf, n, "--%s", degree(c));
    return true;
  }
  snprintf(buf, n, "%d%s", to_display_temp(Weather_weatherInfo.currentTemp), degree(c));
  return true;
}

static bool unit_temperature(const WidgetCtx *c, char *buf, size_t n) {
  snprintf(buf, n, settings.useMetric ? "\xc2\xb0""C" : "\xc2\xb0""F");
  return true;
}

// Today's high / low on one line, "18/11"; the icon is the forecast's.
static bool value_forecast(const WidgetCtx *c, char *buf, size_t n) {
  if (Weather_weatherInfo.todaysHighTemp == INT32_MIN) {
    snprintf(buf, n, "--/--");
    return true;
  }
  snprintf(buf, n, "%d/%d", to_display_temp(Weather_weatherInfo.todaysHighTemp),
           to_display_temp(Weather_weatherInfo.todaysLowTemp));
  return true;
}

static bool value_uv(const WidgetCtx *c, char *buf, size_t n) {
  // A real UV index is always >= 0. A negative value (INT32_MIN before any data, or -1
  // sent by the provider when no reading could be obtained, e.g. FMI at night) means
  // "no data": show a placeholder rather than a misleading 0.
  if (Weather_weatherInfo.currentUVIndex >= 0) {
    snprintf(buf, n, "%d", (int)Weather_weatherInfo.currentUVIndex);
  } else {
    snprintf(buf, n, "--");
  }
  return true;
}

// Max rain in the next N hours (ids 40..44 = window 0..4), mm/h or in/h.
static bool value_rain(const WidgetCtx *c, char *buf, size_t n) {
  int i = c->id - RAIN_MAX_1H;
  uint16_t v = (i >= 0 && i < RAIN_WINDOWS) ? Weather_weatherInfo.rainMax[i] : RAIN_UNKNOWN;
  rain_format_value(v, settings.precipUnit, settings.decimalSeparator, buf, n);
  return true;
}

static bool unit_rain(const WidgetCtx *c, char *buf, size_t n) {
  rain_format_unit(c->id - RAIN_MAX_1H, settings.precipUnit, buf, n);
  return true;
}

static bool unit_uv(const WidgetCtx *c, char *buf, size_t n) { snprintf(buf, n, "UVI"); return true; }

/***** time and date *****/

static bool value_digital_time(const WidgetCtx *c, char *buf, size_t n) {
  int h = s_tm.tm_hour;
  if (!clock_is_24h_style()) {
    h %= 12;
    if (h == 0) { h = 12; }
  }
  snprintf(buf, n, (settings.showLeadingZero && h < 10) ? "0%d:%02d" : "%d:%02d", h, s_tm.tm_min);
  return true;
}

// AM / PM beside a 12 h time; a 24 h clock has no unit.
static bool unit_digital_time(const WidgetCtx *c, char *buf, size_t n) {
  if (clock_is_24h_style()) { return false; }
  snprintf(buf, n, s_tm.tm_hour < 12 ? "AM" : "PM");
  return true;
}

static int mod(int a, int b) {
  int r = a % b;
  return r < 0 ? r + b : r;
}

// Local time shifted to the configured alternate zone: "14:05", or "2:05p" on a 12 h
// clock. The configured offset is in whole hours.
static bool value_alt_time(const WidgetCtx *c, char *buf, size_t n) {
  int hour = s_tm.tm_hour - (int)(s_tm.tm_gmtoff / 3600) - s_tm.tm_isdst + settings.altclockOffset;
  char am_pm[2] = { 0, 0 };
  if (clock_is_24h_style()) {
    hour = mod(hour, 24);
  } else {
    am_pm[0] = (mod(hour, 24) < 12) ? 'a' : 'p';
    hour = mod(hour, 12);
    if (hour == 0) { hour = 12; }
  }
  snprintf(buf, n, (settings.showLeadingZero && hour < 10) ? "0%d:%02d%s" : "%d:%02d%s",
           hour, s_tm.tm_min, am_pm);
  return true;
}

static bool unit_alt_time(const WidgetCtx *c, char *buf, size_t n) {
  if (settings.altclockName[0] == '\0') { return false; }
  snprintf(buf, n, "%s", settings.altclockName);
  return true;
}

static bool value_seconds(const WidgetCtx *c, char *buf, size_t n) {
  snprintf(buf, n, ":%02d", s_tm.tm_sec);
  return true;
}

static bool value_week(const WidgetCtx *c, char *buf, size_t n) {
  strftime(buf, n, "%V", &s_tm);
  return true;
}

static bool unit_week(const WidgetCtx *c, char *buf, size_t n) {
  snprintf(buf, n, "%s", wordForWeek[settings.languageId]);
  return true;
}

static bool value_beats(const WidgetCtx *c, char *buf, size_t n) {
  snprintf(buf, n, "@%d", time_get_beats(&s_tm));
  return true;
}

// The date widgets 26..34. Weekday and month come from the language tables.
static bool value_date(const WidgetCtx *c, char *buf, size_t n) {
  int d = s_tm.tm_mday, m = s_tm.tm_mon + 1, y = s_tm.tm_year + 1900;
  const char *wd_upper = dayNames[settings.languageId][(unsigned)s_tm.tm_wday % 7];
  const char *mon_upper = monthNames[settings.languageId][(unsigned)s_tm.tm_mon % 12];

  // Convert to initial-caps: first letter unchanged, rest lowercase
  char wd[8], mon[8];
  text_initial_caps(wd_upper, wd, sizeof(wd));
  text_initial_caps(mon_upper, mon, sizeof(mon));

  switch (c->id) {
    case DATE_D_M_DOT:    snprintf(buf, n, "%d.%d.", d, m); break;
    case DATE_DD_MM_DOT:  snprintf(buf, n, "%02d.%02d.", d, m); break;
    case DATE_DD_MM:      snprintf(buf, n, "%02d.%02d", d, m); break;
    case DATE_D_M_YYYY:   snprintf(buf, n, "%d.%d.%d", d, m, y); break;
    case DATE_WKDAY_DD:   snprintf(buf, n, "%s %02d", wd, d); break;
    case DATE_WKDAY_D_M:  snprintf(buf, n, "%s %d.%d.", wd, d, m); break;
    case DATE_WKDAY:      snprintf(buf, n, "%s", wd); break;
    case DATE_DD_MON:     snprintf(buf, n, "%02d %s", d, mon); break;
    default:              snprintf(buf, n, "%04d-%02d-%02d", y, m, d); break;
  }
  return true;
}

/***** sleep *****/

static int read_sleep_seconds(HealthMetric metric) {
  if (!is_health_metric_accessible(metric)) { return 0; }
  return (int)health_service_sum_today(metric);
}

// Total sleep and the restful part of it (what the sleep widgets show).
static void sleep_seconds(int *total, int *deep) {
  *total = read_sleep_seconds(HealthMetricSleepSeconds);
  *deep = read_sleep_seconds(HealthMetricSleepRestfulSeconds);
#ifdef SCREENSHOT_FIXTURES
  *total = 23400;   // demo: 6.5 h slept, 1.75 h of it restful
  *deep = 6300;
#endif
}

static bool value_sleep(const WidgetCtx *c, char *buf, size_t n) {
  int total, deep;
  sleep_seconds(&total, &deep);
  sleep_format_decimal(c->id == DEEP_SLEEP_TIMER ? deep : total, settings.decimalSeparator, buf, n);
  return true;
}

// Total / restful on one line, "7.3/1.8".
static bool value_sleep_combined(const WidgetCtx *c, char *buf, size_t n) {
  int total, deep;
  sleep_seconds(&total, &deep);
  char a[12], b[12];
  sleep_format_decimal(total, settings.decimalSeparator, a, sizeof(a));
  sleep_format_decimal(deep, settings.decimalSeparator, b, sizeof(b));
  snprintf(buf, n, "%s/%s", a, b);
  return true;
}

static bool unit_hours(const WidgetCtx *c, char *buf, size_t n) { snprintf(buf, n, "h"); return true; }

/***** electricity *****/

static bool value_electricity(const WidgetCtx *c, char *buf, size_t n) {
  int16_t v;
  if (Electricity_getCurrentPrice(&v)) {
    elec_format_price(v, settings.decimalSeparator, buf, n);
  } else {
    snprintf(buf, n, "--");
  }
  return true;
}

static bool unit_electricity(const WidgetCtx *c, char *buf, size_t n) { snprintf(buf, n, "c/kWh"); return true; }

// The window the cheap-electricity widgets show, or false when there is none.
static bool cheap_window(uint8_t id, ElecDisplay *d) {
  return id == NEXT_CHEAP_ELEC
      ? Electricity_getNextCheap(settings.elecQuietStart, settings.elecQuietEnd,
                                 settings.elecCheapFactorPct, settings.elecCheapFloorCenti,
                                 settings.elecCheapCeilingCenti, d)
      : Electricity_getCheapestHour(settings.elecQuietStart, settings.elecQuietEnd, d);
}

// When the window starts: "NOW", "14:00", or "3:15+" when it is on a later day.
static bool value_cheap_time(const WidgetCtx *c, char *buf, size_t n) {
  ElecDisplay d;
  if (!cheap_window(c->id, &d)) {
    snprintf(buf, n, "--");
  } else if (d.now) {
    snprintf(buf, n, "NOW");
  } else {
    snprintf(buf, n, "%d:%02d%s", d.startHour, d.startMin, d.today ? "" : "+");
  }
  return true;
}

// The window's average price as the unit, a stacked "2.1c" over "kWh".
static bool unit_cheap_price(const WidgetCtx *c, char *buf, size_t n) {
  ElecDisplay d;
  if (!cheap_window(c->id, &d)) {
    snprintf(buf, n, "c/kWh");
    return true;
  }
  char price[12];
  elec_format_price(d.avgCenti, settings.decimalSeparator, price, sizeof(price));
  snprintf(buf, n, "%sc/kWh", price);
  return true;
}

/***** phone data slots (crypto, currency, tuya sensors) *****/

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

static bool unit_slot(const WidgetCtx *c, char *buf, size_t n) {
  CryptoSlot *s = data_slot(c->id);
  snprintf(buf, n, "%s", (s && s->label[0]) ? s->label : "--");
  return true;
}

/********** descriptors **********/

static const WidgetDesc DESC_EMPTY = { NULL, ICON_NONE, NULL };
static const WidgetDesc DESC_BT = { NULL, ICON_BT_OFF, NULL };
static const WidgetDesc DESC_BT_ICON = { NULL, ICON_BT_STATE, NULL };
static const WidgetDesc DESC_BATTERY = { value_battery, ICON_BATTERY, unit_battery };
static const WidgetDesc DESC_BATTERY_DAYS = { value_battery_days, ICON_BATTERY, unit_battery_days };
static const WidgetDesc DESC_BATTERY_ICON = { NULL, ICON_BATTERY, NULL };
static const WidgetDesc DESC_STEPS = { value_steps, ICON_STEPS, unit_steps };
static const WidgetDesc DESC_DISTANCE = { value_distance, ICON_STEPS, unit_distance };
static const WidgetDesc DESC_HEART = { value_heart, ICON_HEART, unit_heart };
static const WidgetDesc DESC_WEATHER = { value_weather, ICON_WEATHER, unit_temperature };
static const WidgetDesc DESC_WEATHER_ICON = { NULL, ICON_WEATHER, NULL };
static const WidgetDesc DESC_FORECAST = { value_forecast, ICON_FORECAST, unit_temperature };
static const WidgetDesc DESC_RAIN = { value_rain, ICON_RAIN, unit_rain };
static const WidgetDesc DESC_UV = { value_uv, ICON_NONE, unit_uv };
static const WidgetDesc DESC_ALT_TIME = { value_alt_time, ICON_NONE, unit_alt_time };
static const WidgetDesc DESC_SECONDS = { value_seconds, ICON_NONE, NULL };
static const WidgetDesc DESC_WEEK = { value_week, ICON_NONE, unit_week };
static const WidgetDesc DESC_BEATS = { value_beats, ICON_NONE, NULL };
static const WidgetDesc DESC_DATE = { value_date, ICON_NONE, NULL };
static const WidgetDesc DESC_DIGITAL_TIME = { value_digital_time, ICON_NONE, unit_digital_time };
static const WidgetDesc DESC_SLEEP = { value_sleep, ICON_SLEEP, unit_hours };
static const WidgetDesc DESC_DEEP_SLEEP = { value_sleep, ICON_DEEP_SLEEP, unit_hours };
static const WidgetDesc DESC_SLEEP_COMBINED = { value_sleep_combined, ICON_SLEEP, unit_hours };
static const WidgetDesc DESC_SLEEP_BAR = { value_sleep, ICON_SLEEP_BAR, unit_hours };
static const WidgetDesc DESC_ELECTRICITY = { value_electricity, ICON_NONE, unit_electricity };
static const WidgetDesc DESC_CHEAP = { value_cheap_time, ICON_NONE, unit_cheap_price };
static const WidgetDesc DESC_TUYA_LEDS = { NULL, ICON_LEDS, NULL };
static const WidgetDesc DESC_SLOT = { value_slot, ICON_NONE, unit_slot };

static const WidgetDesc *desc_for(uint8_t id) {
  if (Crypto_isWid(id) || Currency_isWid(id) || Tuya_isWid(id)) { return &DESC_SLOT; }
  if (id >= DATE_D_M_DOT && id <= DATE_ISO) { return &DESC_DATE; }
  switch (id) {
    case EMPTY:                return &DESC_EMPTY;
    case BLUETOOTH_DISCONNECT: return &DESC_BT;
    case BATTERY_METER:        return &DESC_BATTERY;
    case ALT_TIME_ZONE:        return &DESC_ALT_TIME;
    case SECONDS:              return &DESC_SECONDS;
    case WEEK_NUMBER:          return &DESC_WEEK;
    case WEATHER_CURRENT:      return &DESC_WEATHER;
    case WEATHER_FORECAST_TODAY: return &DESC_FORECAST;
    case SLEEP_TIMER:          return &DESC_SLEEP;
    case STEP_COUNTER:         return &DESC_STEPS;
    case BEATS:                return &DESC_BEATS;
    case HEARTRATE:            return &DESC_HEART;
    case WEATHER_UV_INDEX:     return &DESC_UV;
    case ELECTRICITY:          return &DESC_ELECTRICITY;
    case NEXT_CHEAP_ELEC:
    case CHEAPEST_ELEC_HOUR:   return &DESC_CHEAP;
    case DEEP_SLEEP_TIMER:     return &DESC_DEEP_SLEEP;
    case DISTANCE:             return &DESC_DISTANCE;
    case BATTERY_DAYS:         return &DESC_BATTERY_DAYS;
    case TUYA_LEDS:            return &DESC_TUYA_LEDS;
    case SLEEP_COMBINED:       return &DESC_SLEEP_COMBINED;
    case SLEEP_DEEP_BAR:       return &DESC_SLEEP_BAR;
    case DIGITAL_TIME:         return &DESC_DIGITAL_TIME;
    case BATTERY_ICON:         return &DESC_BATTERY_ICON;
    case WEATHER_ICON:         return &DESC_WEATHER_ICON;
    case BLUETOOTH_ICON:       return &DESC_BT_ICON;
    case RAIN_MAX_1H: case RAIN_MAX_3H: case RAIN_MAX_6H: case RAIN_MAX_9H: case RAIN_MAX_12H:
                               return &DESC_RAIN;
    default:                   return NULL;   // unknown id: draws nothing
  }
}

/********** icons **********/

// The battery is the vertical one of the original Nyquist watchface (GPL-3.0, same
// geometry and drawing): a nub on top of a rounded body, the fill growing upwards by
// percent, red at 20 % or less on colour watches. While charging it is filled solid with a
// bg-coloured bolt over it (Nyquist fills solid alone, which looks like 100 %).
#define BATTERY_ICON_WIDTH        14   // outer width of the battery icon
#define BATTERY_ICON_HEIGHT       20   // outer height of the body
#define BATTERY_ICON_NUB_WIDTH     6   // width of the battery nub
#define BATTERY_ICON_NUB_HEIGHT    3   // height of the battery nub
#define BATTERY_ICON_BORDER        2   // border thickness inside the icon
#define BATTERY_ICON_FILL_GAP      2   // inset gap around the fill inside the icon
#define BATTERY_W BATTERY_ICON_WIDTH
#define BATTERY_H (BATTERY_ICON_NUB_HEIGHT + BATTERY_ICON_HEIGHT)   // == WL_ICON_H

// The Bluetooth rune (2 px strokes; icon-only Bluetooth widget and the disconnect fallback),
// same box connected or not.
#define BT_RUNE_W 14
#define BT_RUNE_H WL_ICON_H

// Share bar of the sleep + deep widget: a vertical gauge like the battery body.
#define SLEEP_BAR_W 12
#define SLEEP_BAR_H WL_ICON_H

// The Tuya LED row: LEDs LED_D wide in a row WL_ICON_H high.
#define LED_D 14
#define LED_GAP 2

static GDrawCommandImage *icon_image(IconKind kind) {
  switch (kind) {
    case ICON_STEPS:    return s_steps_image;
    case ICON_HEART:    return s_heart_image;
    case ICON_SLEEP:    return s_sleep_image;
    case ICON_DEEP_SLEEP: return s_deep_sleep_image;
    case ICON_WEATHER:  return Weather_currentWeatherIcon;
    case ICON_FORECAST: return Weather_forecastWeatherIcon;
    case ICON_RAIN:     return s_rain_image;
    default:            return NULL;
  }
}

static uint8_t led_count(void) { return TuyaLeds_count == 0 ? 1 : TuyaLeds_count; }

// Nub and body in one colour (the icon's shape, for its shadow).
static void draw_battery_silhouette(GContext *g, GRect box, GColor color) {
  graphics_context_set_fill_color(g, color);
  graphics_fill_rect(g, GRect(box.origin.x + (BATTERY_ICON_WIDTH - BATTERY_ICON_NUB_WIDTH) / 2, box.origin.y,
                              BATTERY_ICON_NUB_WIDTH, BATTERY_ICON_NUB_HEIGHT), 0, GCornerNone);
  graphics_fill_rect(g, GRect(box.origin.x, box.origin.y + BATTERY_ICON_NUB_HEIGHT,
                              BATTERY_ICON_WIDTH, BATTERY_ICON_HEIGHT), 2, GCornersAll);
}

static void draw_battery(GContext *g, GRect box, GColor fg, GColor bg) {
  BatteryChargeState st = battery_state_service_peek();
  uint8_t pct = battery_percent();
  int x = box.origin.x, y = box.origin.y;
  int body_y = y + BATTERY_ICON_NUB_HEIGHT;
  int inset = BATTERY_ICON_BORDER + BATTERY_ICON_FILL_GAP;
  int fill_x = x + inset, fill_w = BATTERY_ICON_WIDTH - 2 * inset;
  int fill_inner_h = BATTERY_ICON_HEIGHT - 2 * inset;
  int fill_h = fill_inner_h * pct / 100;
  int fill_y = body_y + inset + (fill_inner_h - fill_h);

  graphics_context_set_fill_color(g, fg);
  graphics_fill_rect(g, GRect(x + (BATTERY_ICON_WIDTH - BATTERY_ICON_NUB_WIDTH) / 2, y,
                              BATTERY_ICON_NUB_WIDTH, BATTERY_ICON_NUB_HEIGHT), 0, GCornerNone);
  graphics_fill_rect(g, GRect(x, body_y, BATTERY_ICON_WIDTH, BATTERY_ICON_HEIGHT), 2, GCornersAll);
  graphics_context_set_fill_color(g, bg);
  graphics_fill_rect(g, GRect(x + BATTERY_ICON_BORDER, body_y + BATTERY_ICON_BORDER,
                              BATTERY_ICON_WIDTH - 2 * BATTERY_ICON_BORDER,
                              BATTERY_ICON_HEIGHT - 2 * BATTERY_ICON_BORDER), 1, GCornersAll);
#ifdef PBL_COLOR
  graphics_context_set_fill_color(g, pct <= 20 ? GColorRed : fg);
#else
  graphics_context_set_fill_color(g, fg);
#endif
  if (st.is_charging) {
    graphics_fill_rect(g, GRect(fill_x, body_y + inset, fill_w, fill_inner_h), 0, GCornerNone);
    // the bolt: a bg zig-zag across the solid fill
    graphics_context_set_antialiased(g, false);
    graphics_context_set_stroke_color(g, bg);
    graphics_context_set_stroke_width(g, 1);
    int bx = fill_x, by = body_y + inset;
    graphics_draw_line(g, GPoint(bx + 4, by), GPoint(bx + 1, by + 6));
    graphics_draw_line(g, GPoint(bx + 1, by + 6), GPoint(bx + 4, by + 6));
    graphics_draw_line(g, GPoint(bx + 4, by + 6), GPoint(bx + 1, by + fill_inner_h - 1));
    graphics_context_set_antialiased(g, true);
  } else {
    graphics_fill_rect(g, GRect(fill_x, fill_y, fill_w, fill_h), 0, GCornerNone);
  }
}

// The rune is drawn with 2 px strokes. graphics_context_set_stroke_width only honours odd
// widths, so each stroke is two 1 px lines one pixel apart: in x for a steep segment, in y
// for a shallow one, so the stroke stays 2 px across its direction.
static void draw_stroke2(GContext *g, int x0, int y0, int x1, int y1, GColor color) {
  int ox = 1, oy = 0;
  if (abs(x1 - x0) > abs(y1 - y0)) { ox = 0; oy = 1; }
  graphics_context_set_stroke_color(g, color);
  graphics_draw_line(g, GPoint(x0, y0), GPoint(x1, y1));
  graphics_draw_line(g, GPoint(x0 + ox, y0 + oy), GPoint(x1 + ox, y1 + oy));
}

// The slash across a disconnected rune, from the bottom left to the top right (start
// and end chosen so the halo leaves the fewest 1 px slivers of the rune's strokes).
static void draw_bt_slash(GContext *g, GRect box, GColor color) {
  draw_stroke2(g, box.origin.x + 2, box.origin.y + BT_RUNE_H - 1, box.origin.x + BT_RUNE_W - 2, box.origin.y, color);
}

// The rune's strokes (and, disconnected, the slash) in one colour.
static void draw_bt_strokes(GContext *g, GRect box, bool connected, GColor color) {
  static const int8_t P[6][2] = { { 0, 6 }, { 12, 16 }, { 6, 22 }, { 6, 0 }, { 12, 6 }, { 0, 16 } };
  int x = box.origin.x, y = box.origin.y;
  graphics_context_set_antialiased(g, false);
  graphics_context_set_stroke_width(g, 1);
  for (int i = 0; i < 5; i++) {
    draw_stroke2(g, x + P[i][0], y + P[i][1], x + P[i + 1][0], y + P[i + 1][1], color);
  }
  if (!connected) { draw_bt_slash(g, box, color); }
  graphics_context_set_antialiased(g, true);
}

// Disconnected, the slash carries a 1 px bg gap on each side so it reads over the rune.
static void draw_bt_rune(GContext *g, GRect box, bool connected, GColor fg, GColor bg) {
  draw_bt_strokes(g, box, true, fg);
  if (!connected) {
    graphics_context_set_antialiased(g, false);
    graphics_context_set_stroke_width(g, 1);
    for (int dx = -1; dx <= 1; dx++) {
      GRect b = box;
      b.origin.x += dx;
      draw_bt_slash(g, b, bg);
    }
    draw_bt_slash(g, box, fg);
    graphics_context_set_antialiased(g, true);
  }
}

// Outlined bar filled to the restful share of the night; the outline is drawn even at
// 0 % so a dataless widget reads as an empty bar rather than a missing one.
static void draw_sleep_bar(GContext *g, GRect box, GColor fg) {
  int total, deep;
  sleep_seconds(&total, &deep);
  graphics_context_set_stroke_color(g, fg);
  graphics_context_set_stroke_width(g, 1);
  graphics_draw_rect(g, GRect(box.origin.x, box.origin.y, SLEEP_BAR_W, SLEEP_BAR_H));
  int fill = sleep_bar_fill_px(total, deep, SLEEP_BAR_H - 2);
  if (fill > 0) {
    graphics_context_set_fill_color(g, fg);
    graphics_fill_rect(g, GRect(box.origin.x + 1, box.origin.y + SLEEP_BAR_H - 1 - fill,
                                SLEEP_BAR_W - 2, fill), 0, GCornerNone);
  }
}

static void draw_leds_silhouette(GContext *g, GRect box, GColor color) {
  graphics_context_set_fill_color(g, color);
  for (int i = 0; i < led_count(); i++) {
    graphics_fill_circle(g, GPoint(box.origin.x + i * (LED_D + LED_GAP) + LED_D / 2, box.origin.y + WL_ICON_H / 2),
                         LED_D / 2);
  }
}

// A row of LEDs, green on / red off, an empty ring for an unknown state (and for no
// data at all, so a placed widget is visibly present but declares it knows nothing).
static void draw_leds(GContext *g, GRect box, GColor fg) {
  const int r = LED_D / 2;
  bool no_data = TuyaLeds_count == 0;
  graphics_context_set_stroke_color(g, fg);
  for (int i = 0; i < led_count(); i++) {
    uint8_t state = no_data ? TUYA_LED_UNKNOWN : TuyaLeds_states[i];
    GPoint p = GPoint(box.origin.x + i * (LED_D + LED_GAP) + r, box.origin.y + WL_ICON_H / 2);
    if (state == TUYA_LED_ON || state == TUYA_LED_OFF) {
      graphics_context_set_fill_color(g, (state == TUYA_LED_ON) ? GColorGreen : GColorRed);
      graphics_fill_circle(g, p, r);
    }
    graphics_draw_circle(g, p, r);
  }
}

/********** the Widget functions **********/

static void measure(const WidgetCtx *c, WidgetSize *out) {
  memset(out, 0, sizeof(*out));
  const WidgetDesc *d = desc_for(c->id);
  if (!d) { return; }
  char buf[24];
  if (d->value || (d->icon == ICON_NONE && !d->unit)) {   // a value, or EMPTY: reserves the font's ink height
    out->ink_h = (int16_t)WidgetText_inkHeight(c->font);
  }
  if (d->value && d->value(c, buf, sizeof(buf))) {
    out->value_w = (int16_t)WidgetText_width(buf, c->font);
    out->value_desc = wl_text_has_descender(buf);
  }
  WlIdent mode = ident_of(c);
  if (mode == WL_IDENT_NONE) { return; }
  if (mode == WL_IDENT_UNIT) {
    if (d->unit(c, buf, sizeof(buf))) {
      int w, h;
      WidgetUnit_measure(buf, &w, &h);
      out->icon_w = (int16_t)w;
      out->icon_h = (int16_t)h;
      out->unit_inline = WidgetUnit_isInline(buf);
      out->unit_desc = out->unit_inline && wl_text_has_descender(buf);
    }
    return;
  }
  switch (d->icon) {
    case ICON_NONE:
      break;
    case ICON_BATTERY:
      out->icon_w = BATTERY_W;
      out->icon_h = BATTERY_H;
      break;
    case ICON_BT_STATE:
    case ICON_BT_OFF:
      out->icon_w = BT_RUNE_W;
      out->icon_h = BT_RUNE_H;
      break;
    case ICON_STEPS: case ICON_HEART: case ICON_WEATHER: case ICON_FORECAST:
    case ICON_RAIN: case ICON_SLEEP: case ICON_DEEP_SLEEP: {
      GDrawCommandImage *img = icon_image(d->icon);
      if (img) {
        GSize s = gdraw_command_image_get_bounds_size(img);
        out->icon_w = s.w;
        out->icon_h = s.h;
      }
      break;
    }
    case ICON_SLEEP_BAR:
      out->icon_w = SLEEP_BAR_W;
      out->icon_h = SLEEP_BAR_H;
      break;
    case ICON_LEDS:
      out->icon_w = led_count() * LED_D + (led_count() - 1) * LED_GAP;
      out->icon_h = WL_ICON_H;
      break;
  }
}

// Icon-only widgets are drawn above the hands with a background shadow (the same
// diamond as the value shadow): their silhouette in bg at every shadow offset, then the
// icon itself. A widget with a value or a unit keeps its icon below the hands.
bool Widgets_iconOnTop(uint8_t id) {
  const WidgetDesc *d = desc_for(id);
  return d && !d->value && d->icon != ICON_NONE;
}

// The icon's shape in one colour (no per-frame allocation: the PDC images are recoloured
// in place and restored by the normal draw).
static void draw_icon_silhouette(GContext *g, const WidgetCtx *c, GRect box, GColor color) {
  const WidgetDesc *d = desc_for(c->id);
  switch (d->icon) {
    case ICON_BT_STATE: draw_bt_strokes(g, box, bluetooth_connection_service_peek(), color); break;
    case ICON_BT_OFF:   draw_bt_strokes(g, box, false, color); break;
    case ICON_BATTERY:  draw_battery_silhouette(g, box, color); break;
    case ICON_LEDS:     draw_leds_silhouette(g, box, color); break;
    default: {
      GDrawCommandImage *img = icon_image(d->icon);
      if (img) {
        gdraw_command_image_recolor(img, color, color);
        gdraw_command_image_draw(g, img, box.origin);
      }
    }
  }
}

static void draw_icon_shadow(GContext *g, const WidgetCtx *c, GRect box, GColor bg) {
  for (int i = 0; i < CF_SHADOW_OFFSET_COUNT; i++) {
    GRect b = box;
    b.origin.x += CF_SHADOW_OFFSETS[i][0];
    b.origin.y += CF_SHADOW_OFFSETS[i][1];
    draw_icon_silhouette(g, c, b, bg);
  }
}

static void draw_icon(GContext *g, const WidgetCtx *c, GRect box) {
  const WidgetDesc *d = desc_for(c->id);
  if (!d || ident_of(c) == WL_IDENT_NONE) { return; }
  GColor fg = theme.timeColor, bg = theme.timeBgColor;
  IconKind kind = d->icon;
  char buf[24];
  if (ident_of(c) == WL_IDENT_UNIT) {
    if (d->unit(c, buf, sizeof(buf))) { WidgetUnit_draw(g, buf, box, fg); }
    return;
  }
  if (Widgets_iconOnTop(c->id)) { draw_icon_shadow(g, c, box, bg); }
  switch (kind) {
    case ICON_NONE:      break;
    case ICON_BT_STATE:  draw_bt_rune(g, box, bluetooth_connection_service_peek(), fg, bg); break;
    case ICON_BT_OFF:    draw_bt_rune(g, box, false, fg, bg); break;
    case ICON_BATTERY:   draw_battery(g, box, fg, bg); break;
    case ICON_SLEEP_BAR: draw_sleep_bar(g, box, fg); break;
    case ICON_LEDS:      draw_leds(g, box, fg); break;
    default: {
      GDrawCommandImage *img = icon_image(kind);
      if (img) {
        gdraw_command_image_recolor(img, theme.iconFillColor, theme.iconStrokeColor);
        gdraw_command_image_draw(g, img, box.origin);
      }
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

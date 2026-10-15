#include "settings.h"
#include "languages.h"
#include "crypto.h"
#include "widget_list.h"
#include "widget_fonts.h"
#include "night_window_calc.h"
#include <pebble.h>

Settings settings;
DynamicSettings dynamicSettings;

void Settings_init() {
  Settings_loadFromStorage();
}

void Settings_deinit() { Settings_saveToStorage(); }

static void set_defaults(void) {
  memset(&settings, 0, sizeof(settings));

  settings.timeBgColor = GColorWhite;
  settings.timeColor = GColorBlack;

  settings.languageId = LANGUAGE_EN;
  settings.hourlyVibe = NO_VIBE;
  settings.pollIntervalMin = 30;

  settings.activateDisconnectIcon = true;
  settings.autoBatteryThreshold = 10;
  strncpy(settings.altclockName, "ALT", sizeof(settings.altclockName));
  settings.decimalSeparator = '.';
  settings.fallbackColumn = 0;
  settings.fallbackPosition = 1;

  settings.elecQuietStart = 23;
  settings.elecQuietEnd = 7;
  settings.elecCheapFactorPct = 70;
  settings.elecCheapFloorCenti = 200;   // 2.0 snt/kWh
  settings.elecCheapCeilingCenti = 800; // 8.0 snt/kWh


  settings.nightMode = NIGHT_WINDOW_OFF;
  settings.nightStartHour = 23;
  settings.nightEndHour = 7;
  settings.nightSlowRotation = true;
  settings.nightColors = false;
  settings.nightBgColor = GColorBlack;
  settings.nightFgColor = GColorWhite;

  settings.batteryWarnColor = GColorRed;
  settings.btWarnColor = GColorYellow;

  // A fresh install, every widget in its position's default font. Each edge groups an
  // icon over its value in the top corner (weather icon + temperature, battery icon +
  // percent with its % unit) and one widget in the bottom corner (steps, heart rate); the five Empty
  // slots in between push the pairs apart.
#define D WL_FONT_POS_DEFAULT
#define VALUE_ONLY (WL_FONT_POS_DEFAULT | WL_FLAG_NONE)
#define WITH_UNIT (WL_FONT_POS_DEFAULT | WL_FLAG_UNIT)
#define FIVE_EMPTY EMPTY, D, EMPTY, D, EMPTY, D, EMPTY, D, EMPTY, D
  static const uint8_t LEFT[] = { WEATHER_ICON, D, WEATHER_CURRENT, VALUE_ONLY, FIVE_EMPTY,
                                  STEP_COUNTER, D };
  static const uint8_t RIGHT[] = { BATTERY_ICON, D, BATTERY_METER, WITH_UNIT, FIVE_EMPTY,
                                   HEARTRATE, D };
#undef FIVE_EMPTY
#undef WITH_UNIT
#undef VALUE_ONLY
#undef D
  static const uint8_t TOP[] = { DATE_WKDAY_DD, WL_FONT_POS_DEFAULT };
  static const uint8_t BOTTOM[] = { EMPTY, WL_FONT_POS_DEFAULT };
  Settings_setWidgetList(WIDGET_POS_LEFT, LEFT, sizeof LEFT);
  Settings_setWidgetList(WIDGET_POS_RIGHT, RIGHT, sizeof RIGHT);
  Settings_setWidgetList(WIDGET_POS_TOP, TOP, sizeof TOP);
  Settings_setWidgetList(WIDGET_POS_BOTTOM, BOTTOM, sizeof BOTTOM);
}

void Settings_loadFromStorage() {
  set_defaults();

  if (persist_exists(SETTINGS_PERSIST_KEY) && persist_exists(SETTINGS_VERSION_PERSIST_KEY)
      && persist_read_int(SETTINGS_VERSION_PERSIST_KEY) == CURRENT_SETTINGS_VERSION
      && persist_get_size(SETTINGS_PERSIST_KEY) == (int)sizeof(settings)) {
    persist_read_data(SETTINGS_PERSIST_KEY, &settings, sizeof(settings));
  }

  // Sanitize loaded settings: anything used as an array index is clamped back to a
  // safe default so a garbled blob can never crash every draw.
  if (settings.languageId > LANGUAGE_IW) { settings.languageId = LANGUAGE_EN; }
  if (settings.hourlyVibe > VIBE_EVERY_HALF_HOUR) { settings.hourlyVibe = NO_VIBE; }
  Settings_sanitizeWidgetLists();
  if (settings.decimalSeparator != '.' && settings.decimalSeparator != ',') {
    settings.decimalSeparator = '.';
  }
  settings.altclockName[sizeof(settings.altclockName) - 1] = '\0';
  if (settings.pollIntervalMin < 5 || settings.pollIntervalMin > 240) { settings.pollIntervalMin = 30; }
  if (settings.elecQuietStart > 23) { settings.elecQuietStart = 23; }
  if (settings.elecQuietEnd > 23) { settings.elecQuietEnd = 7; }
  if (settings.elecCheapFactorPct < 1 || settings.elecCheapFactorPct > 100) { settings.elecCheapFactorPct = 70; }
  if (settings.nightMode > NIGHT_WINDOW_CUSTOM) { settings.nightMode = NIGHT_WINDOW_OFF; }
  if (settings.nightStartHour > 23) { settings.nightStartHour = 23; }
  if (settings.nightEndHour > 23) { settings.nightEndHour = 7; }
  if (settings.nightSlowRotation > 1) { settings.nightSlowRotation = 1; }
  if (settings.nightColors > 1) { settings.nightColors = 0; }
  if (settings.autoBatteryThreshold < 1 || settings.autoBatteryThreshold > 100) { settings.autoBatteryThreshold = 10; }
  if (settings.fallbackColumn > 2) { settings.fallbackColumn = 0; }
  if (settings.fallbackPosition < 1 || settings.fallbackPosition > MAX_WIDGET_SLOTS + 1) { settings.fallbackPosition = 1; }
  if (settings.batteryWarnPct > 100) { settings.batteryWarnPct = 0; }
  // 100 tenths = 10 days; a larger threshold would mean "always warning".
  if (settings.batteryWarnDaysTenths > 100) { settings.batteryWarnDaysTenths = 0; }
  if (settings.btWarnBorder > 1) { settings.btWarnBorder = 0; }

#ifdef SCREENSHOT_FIXTURES
  // Appstore screenshot scene config (no phone); not persisted.
  settings.useMetric = true;
  {
    static const uint8_t L[] = { 15, 31, 201, 31, 17, 31, ELECTRICITY, 31 };
    static const uint8_t R[] = { WEATHER_FORECAST_TODAY, 31, WEATHER_UV_INDEX, 31, SLEEP_TIMER, 31, STEP_COUNTER, 31 };
    Settings_setWidgetList(WIDGET_POS_LEFT, L, sizeof L);
    Settings_setWidgetList(WIDGET_POS_RIGHT, R, sizeof R);
  }
#endif

  Settings_updateDynamicSettings();
}

void Settings_saveToStorage() {
  Settings_updateDynamicSettings();

  // We're limited to 256 bytes, so make sure that settings fits
  _Static_assert(sizeof(Settings) <= PERSIST_DATA_MAX_LENGTH,
                 "Warning: settings struct is too large!");
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Current settings size %d", sizeof(settings));

  // Write the data
  persist_write_data(SETTINGS_PERSIST_KEY, &settings, sizeof(settings));
  persist_write_int(SETTINGS_VERSION_PERSIST_KEY, CURRENT_SETTINGS_VERSION);
}

static void dyn_scan_cb(uint8_t w, void *ctx) {
  (void)ctx;
  if (w == WEATHER_CURRENT || w == WEATHER_FORECAST_TODAY || w == WEATHER_UV_INDEX || w == WEATHER_ICON) {
    dynamicSettings.disableWeather = false;
  }
  if (w == SECONDS) { dynamicSettings.updateScreenEverySecond = true; }
  if (w == BATTERY_METER || w == BATTERY_ICON) { dynamicSettings.enableAutoBatteryWidget = false; }
  if (w == BEATS) { dynamicSettings.enableBeats = true; }
  if (w == ALT_TIME_ZONE) { dynamicSettings.enableAltTimeZone = true; }
}

void Settings_updateDynamicSettings() {
  // Seed to "disabled" and let dyn_scan_cb clear it when a weather widget is actually
  // placed -- the same clear-on-match pattern as enableAutoBatteryWidget below. This was
  // `false`, i.e. the SAME value the scan sets, so the flag could never be true and
  // needs_phone_data() (main.c) always returned true: a watchface with a purely local
  // sidebar still woke the phone every poll interval, forever.
  dynamicSettings.disableWeather = true;
  dynamicSettings.updateScreenEverySecond = false;
  dynamicSettings.enableAutoBatteryWidget = true;
  dynamicSettings.enableBeats = false;
  dynamicSettings.enableAltTimeZone = false;

  // Scan the full priority list: a widget anywhere in it (even one currently not
  // visible because it didn't fit) should keep its data source / tick enabled,
  // since it can become visible when the status strip toggles.
  Settings_forEachWidgetId(dyn_scan_cb, NULL);
}

const uint8_t *Settings_widgetList(int pos, int *count) {
  switch (pos) {
    case WIDGET_POS_LEFT:  *count = settings.widgetCount;       return settings.widgetList;
    case WIDGET_POS_RIGHT: *count = settings.rightWidgetCount;  return settings.rightWidgetList;
    case WIDGET_POS_TOP:   *count = settings.topWidgetCount;    return settings.topWidgetList;
    default:               *count = settings.bottomWidgetCount; return settings.bottomWidgetList;
  }
}

void Settings_forEachWidgetId(void (*cb)(uint8_t id, void *ctx), void *ctx) {
  for (int pos = 0; pos < WIDGET_POS_COUNT; pos++) {
    int n;
    const uint8_t *list = Settings_widgetList(pos, &n);
    WidgetList_forEachId(list, n, cb, ctx);
  }
}

int Settings_minSubMinuteIntervalSec(void) {
  int best = 0;
  for (int pos = 0; pos < WIDGET_POS_COUNT; pos++) {
    int n;
    const uint8_t *list = Settings_widgetList(pos, &n);
    int s = WidgetList_minSubMinuteIntervalSec(list, n);
    if (s > 0 && (best == 0 || s < best)) { best = s; }
  }
  return best;
}

// A position's list storage: bytes, length, byte budget and slot budget.
typedef struct { uint8_t *bytes; uint8_t *count; int max_bytes, max_slots; } ListRef;

static ListRef list_ref(int pos) {
  switch (pos) {
    case WIDGET_POS_LEFT:
      return (ListRef){ settings.widgetList, &settings.widgetCount, MAX_WIDGET_LIST, MAX_WIDGET_SLOTS };
    case WIDGET_POS_RIGHT:
      return (ListRef){ settings.rightWidgetList, &settings.rightWidgetCount, MAX_WIDGET_LIST, MAX_WIDGET_SLOTS };
    case WIDGET_POS_TOP:
      return (ListRef){ settings.topWidgetList, &settings.topWidgetCount, MAX_CENTER_WIDGET_LIST, 1 };
    default:
      return (ListRef){ settings.bottomWidgetList, &settings.bottomWidgetCount, MAX_CENTER_WIDGET_LIST, 1 };
  }
}

void Settings_setWidgetList(int pos, const uint8_t *bytes, int len) {
  ListRef r = list_ref(pos);
  if (len > r.max_bytes) { len = r.max_bytes; }
  if (len < 0) { len = 0; }
  memcpy(r.bytes, bytes, len);
  *r.count = (uint8_t)WidgetList_sanitize(r.bytes, len, r.max_bytes, r.max_slots);
}

void Settings_sanitizeWidgetLists(void) {
  for (int pos = 0; pos < WIDGET_POS_COUNT; pos++) {
    ListRef r = list_ref(pos);
    if (*r.count > r.max_bytes) { *r.count = (uint8_t)r.max_bytes; }
    *r.count = (uint8_t)WidgetList_sanitize(r.bytes, *r.count, r.max_bytes, r.max_slots);
  }
}

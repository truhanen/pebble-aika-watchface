#include "settings.h"
#include "languages.h"
#include "crypto.h"
#include "widget_list.h"
#include "widget_fonts.h"
#include "night_window_calc.h"
#include <pebble.h>

Settings settings;
WidgetFonts widgetFonts;
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
  settings.showBatteryPct = true;
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

  memset(&widgetFonts, WIDGET_FONT_UNSET, sizeof(widgetFonts));
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
  if (settings.widgetCount > MAX_WIDGET_LIST) { settings.widgetCount = MAX_WIDGET_LIST; }
  settings.widgetCount = WidgetList_sanitize(settings.widgetList, settings.widgetCount, MAX_WIDGET_LIST);
  if (settings.rightWidgetCount > MAX_WIDGET_LIST) { settings.rightWidgetCount = MAX_WIDGET_LIST; }
  settings.rightWidgetCount = WidgetList_sanitize(settings.rightWidgetList, settings.rightWidgetCount, MAX_WIDGET_LIST);
  if (settings.topWidgetCount > MAX_CENTER_WIDGET_LIST) { settings.topWidgetCount = MAX_CENTER_WIDGET_LIST; }
  settings.topWidgetCount = WidgetList_sanitize(settings.topWidgetList, settings.topWidgetCount, MAX_CENTER_WIDGET_LIST);
  if (settings.bottomWidgetCount > MAX_CENTER_WIDGET_LIST) { settings.bottomWidgetCount = MAX_CENTER_WIDGET_LIST; }
  settings.bottomWidgetCount = WidgetList_sanitize(settings.bottomWidgetList, settings.bottomWidgetCount, MAX_CENTER_WIDGET_LIST);
  if (persist_get_size(WIDGET_FONTS_PERSIST_KEY) == (int)sizeof(widgetFonts)) {
    persist_read_data(WIDGET_FONTS_PERSIST_KEY, &widgetFonts, sizeof(widgetFonts));
  }
  for (int p = 0; p < WIDGET_POS_COUNT; p++) {
    for (int i = 0; i < MAX_WIDGET_LIST; i++) {
      if (widgetFonts.font[p][i] >= WIDGET_FONT_COUNT) { widgetFonts.font[p][i] = WIDGET_FONT_UNSET; }
    }
  }
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
  { uint8_t L[] = {15, 201, 17, 14}; memcpy(settings.widgetList, L, sizeof L); settings.widgetCount = sizeof L; }
  { uint8_t R[] = {8, 13, 9, 10};    memcpy(settings.rightWidgetList, R, sizeof R); settings.rightWidgetCount = sizeof R; }
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
  persist_write_data(WIDGET_FONTS_PERSIST_KEY, &widgetFonts, sizeof(widgetFonts));
}

static void dyn_scan_cb(uint8_t w, void *ctx) {
  (void)ctx;
  if (w == WEATHER_CURRENT || w == WEATHER_FORECAST_TODAY || w == WEATHER_UV_INDEX) {
    dynamicSettings.disableWeather = false;
  }
  if (w == SECONDS) { dynamicSettings.updateScreenEverySecond = true; }
  if (w == BATTERY_METER) { dynamicSettings.enableAutoBatteryWidget = false; }
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

int Settings_widgetFont(int pos, int offset) {
  if (offset >= 0 && offset < MAX_WIDGET_LIST && widgetFonts.font[pos][offset] < WIDGET_FONT_COUNT) {
    return widgetFonts.font[pos][offset];
  }
  return (pos == WIDGET_POS_TOP || pos == WIDGET_POS_BOTTOM) ? WIDGET_FONT_DEFAULT_TOP_BOTTOM
                                                              : WIDGET_FONT_DEFAULT_SIDE;
}

void Settings_setWidgetFonts(int pos, uint8_t font) {
  memset(widgetFonts.font[pos], font < WIDGET_FONT_COUNT ? font : WIDGET_FONT_UNSET, MAX_WIDGET_LIST);
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

void Settings_sanitizeWidgetLists(void) {
  settings.widgetCount = WidgetList_sanitize(settings.widgetList, settings.widgetCount, MAX_WIDGET_LIST);
  settings.rightWidgetCount = WidgetList_sanitize(settings.rightWidgetList, settings.rightWidgetCount, MAX_WIDGET_LIST);
  settings.topWidgetCount = WidgetList_sanitize(settings.topWidgetList, settings.topWidgetCount, MAX_CENTER_WIDGET_LIST);
  settings.bottomWidgetCount = WidgetList_sanitize(settings.bottomWidgetList, settings.bottomWidgetCount, MAX_CENTER_WIDGET_LIST);
}

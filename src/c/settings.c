#include "settings.h"
#include "languages.h"
#include "crypto.h"
#include "widget_list.h"
#include "date_header_calc.h"
#include "night_window_calc.h"
#include <pebble.h>

Settings settings;
DynamicSettings dynamicSettings;

void Settings_init() {
  Settings_loadFromStorage();

  // for anyone who already installed timestyle on an emery, reset
  // their large font setting, since the old "large" setting is
  // roughly equivalent to the new default, and the new "large"
  // setting is EVEN LARGER
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  if (!persist_exists(MIGRATION_EMERY_FONT_PERSIST_KEY)) {
    if (settings.useLargeFonts) {
      settings.useLargeFonts = false;
      Settings_saveToStorage();
    }
    persist_write_bool(MIGRATION_EMERY_FONT_PERSIST_KEY, true);
  }
#endif
}

void Settings_deinit() { Settings_saveToStorage(); }

void Settings_loadFromStorage() {
  // Set defaults
  settings.sidebarTextColor = GColorBlack;

#ifdef PBL_COLOR
  settings.timeBgColor = GColorWhite;
  settings.timeColor = GColorBlack;
  settings.sidebarColor = GColorMintGreen;
#else
  settings.timeBgColor = GColorBlack;
  settings.timeColor = GColorWhite;
  settings.sidebarColor = GColorWhite;
#endif

  settings.widgets[0] = HEARTRATE;
  settings.widgets[1] = BTC_PRICE;
  settings.widgets[2] = EURUSD_RATE;

  settings.activateDisconnectIcon = true;
  strncpy(settings.altclockName, "ALT", sizeof(settings.altclockName));
  settings.altclockOffset = 0;
  settings.decimalSeparator = '.';
  settings.clockFontId = FONT_SETTING_LECO;   // LECO clock font by default (configurable)
  settings.showBatteryPct = true;
  settings.midiVibe = false;   // opt-in default; also the upgrade default (appended field, no settings-version bump)
  settings.midiSecondPrecision = false;   // recording timer off by default; appended field, no settings-version bump
  settings.showBigDate = true;   // on by default (configurable); appended field, no settings-version bump
  settings.showBigDateMonth = true;   // month shown by default (configurable); appended field, no settings-version bump
  settings.bigDateFontId = BIG_DATE_FONT_BITHAM;   // Bitham (current look) default; appended field, no settings-version bump
  settings.twtShowRemaining = false;   // opt-in default; appended field, no settings-version bump
  settings.twtTargetVibe = false;   // opt-in default; appended field, no settings-version bump
  settings.twtBudgetVibe = false;   // opt-in default; appended field, no settings-version bump
  settings.pollIntervalMin = 30;   // default; appended field, no settings-version bump
  settings.widgets2[0] = EMPTY;   // secondary panel off by default; appended field, no settings-version bump
  settings.widgets2[1] = EMPTY;
  settings.widgets2[2] = EMPTY;
  settings.secondaryAlwaysOn = false;   // auto-hide by default; appended field, no settings-version bump
  // widgetCount defaults to 0 so the dual-list migration below (re)builds the list
  // from the legacy widgets[]/widgets2[] arrays -- which carry both the
  // fresh-install defaults (seeded above) and an upgrading user's real config --
  // BUT only when there is no byte-array list to carry (widgetCountV1 == 0). A blob
  // from the single-list era already holds the real list in widgetListV1, which the
  // V2 migration carries verbatim; rebuilding from the lossy widgets[] there would
  // strand a duplicate copy. (An older persisted blob predates these appended
  // fields; persist_read_data leaves the tail at this 0 default, so it fires.)
  settings.widgetCount = 0;
  settings.statusStripFullWidth = false;   // full-height columns by default; appended field
  settings.rightWidgetCount = 0;   // right column empty by default; appended field
  settings.dualListInit = false;   // appended field; one-time split migration below fires once
  settings.widgetListV2Init = false;   // appended field; one-time 16->32-byte list migration below
  settings.widgetCountV1 = 0;          // legacy list counts; a genuine v7 blob overwrites these
  settings.rightWidgetCountV1 = 0;
  settings.elecQuietStart = 23;        // appended field, no settings-version bump
  settings.elecQuietEnd = 7;
  settings.elecCheapFactorPct = 70;
  settings.elecCheapFloorCenti = 200;  // 2.0 snt/kWh
  settings.elecCheapCeilingCenti = 800;// 8.0 snt/kWh
  // Night window OFF by default: it only ever helps a user who explicitly chose a
  // 5/10/30 s rotation interval, and silently changing rotation behaviour for everyone
  // else would be a surprise. Appended fields, no settings-version bump.
  settings.nightMode = NIGHT_WINDOW_OFF;
  settings.nightStartHour = 23;
  settings.nightEndHour = 7;
#ifdef PBL_COLOR
  settings.twtStatusBgColor = GColorMintGreen;    // light-green panels by default
  settings.twtFlashColor = GColorRed;             // bright flash; appended field, no settings-version bump
  settings.dateBgColor = GColorMintGreen;
  settings.sidebarBgColorLeft = GColorMintGreen;
  settings.sidebarBgColorRight = GColorMintGreen;
#else
  settings.twtStatusBgColor = GColorClear;    // inherit watchface bg; appended field
  settings.twtFlashColor = GColorBlack;       // high-contrast flash on b&w; appended field
  settings.dateBgColor = GColorClear;         // inherit watchface bg; appended field
  settings.sidebarBgColorLeft = GColorClear;  // inherit sidebarColor; appended field
  settings.sidebarBgColorRight = GColorClear; // inherit sidebarColor; appended field
#endif
  settings.autoBatteryThreshold = 10;   // legacy hardcoded threshold; appended field, no version bump
  settings.fallbackColumn = 0;          // Automatic placement (legacy behaviour); appended field
  settings.fallbackPosition = 1;        // appended field
  settings.clockStyle = CLOCK_STYLE_DIGITAL;   // digital by default; appended field, no version bump
  settings.analogTickStyle = ANALOG_TICKS_BOLD;   // analog hour ticks bold by default; appended field, no version bump
  settings.analogDigitalClock = false;            // digital line under analog off by default; appended field, no version bump
  settings.statusClockDigital = false;            // status-strip digital swap off by default; appended field, no version bump

  // Warning frame off by default (both triggers 0). Colours are only used once a
  // trigger is switched on; GColorRed/GColorYellow exist only on colour boards.
  // On 1-bit boards this C default (GColorWhite) does NOT agree with the Clay config
  // page's default (0xFF0000, which GColorFromHEX resolves to BLACK on those boards) --
  // so the frame colour flips white->black the first time a user saves the config page.
  // That is safe only because warn_border_calc's invisibility guard substitutes a
  // contrasting colour when the configured colour equals the background (black, by
  // default, on 1-bit boards); it is NOT because these two defaults agree.
  settings.batteryWarnPct = 0;
  settings.batteryWarnDaysTenths = 0;
  settings.btWarnBorder = false;
#ifdef PBL_COLOR
  settings.batteryWarnColor = GColorRed;
  settings.btWarnColor = GColorYellow;
#else
  settings.batteryWarnColor = GColorWhite;
  settings.btWarnColor = GColorWhite;
#endif

  // The night window now also drives an optional palette. nightSlowRotation defaults
  // ON so a user who already had a night window configured keeps exactly today's
  // rotation-slowing behaviour; nightColors defaults OFF since repainting the
  // watchface black unasked is not an upgrade. Appended fields, no settings-version bump.
  settings.nightSlowRotation = true;
  settings.nightColors = false;
  settings.nightBgColor = GColorBlack;
  settings.nightFgColor = GColorWhite;

  // to correct settings migration bug (settings key v6), we must do another
  // migration (nooooooooooo)
  if (persist_exists(SETTINGS_PERSIST_KEY)) {
    int version = 0;

    if (persist_exists(SETTINGS_VERSION_PERSIST_KEY)) {
      version = persist_read_int(SETTINGS_VERSION_PERSIST_KEY);

      if (version < CURRENT_SETTINGS_VERSION) {
        // v6 settings: load via memcpy
        LegacyStoredSettings s;
        persist_read_data(SETTINGS_PERSIST_KEY, &s, sizeof(s));
        memcpy(&settings, &s, sizeof(LegacyStoredSettings));

        // re-save in new v7 format
        Settings_saveToStorage();
      } else {
        // v7 settings: load directly via single persist read
        persist_read_data(SETTINGS_PERSIST_KEY, &settings, sizeof(settings));
        settings.altclockName[sizeof(settings.altclockName) - 1] = '\0';
      }
    }
  }

  // One-time widget-list migration, gated by the dualListInit sentinel (an older
  // blob zero-defaults it to false), so neither half re-runs on later boots:
  //   (a) Rebuild the single widgetList from the legacy widgets[0..2] +
  //       widgets2[0..2] arrays when an older blob predates widgetList
  //       (widgetCount==0). Gated here so it does NOT re-run every boot and
  //       repopulate the left column after (b) clears widgetCount.
  //   (b) Split onto left/right: for an UPGRADE (a persisted blob already
  //       existed) whose primary sidebar was on the right (sidebarOnLeft==false),
  //       move the single list into the right column. A fresh install (no
  //       persisted blob) keeps the default list on the LEFT, matching the Clay
  //       config default.
  bool migrated = false;
  if (!settings.dualListInit) {
    settings.dualListInit = true;
    migrated = true;

    // Rebuild from the legacy widgets[]/widgets2[] arrays only when there is no
    // byte-array list to carry. A single-list-era blob has its real list in
    // widgetListV1 (widgetCountV1 > 0); the V2 migration below carries that
    // verbatim, so rebuilding here (lossy: built-in types only) would leave a
    // duplicate stranded in the other column after the split.
    if (settings.widgetCount == 0 && settings.widgetCountV1 == 0) {
      int n = 0;
      for (int i = 0; i < 3; i++) {
        if (settings.widgets[i] != EMPTY && settings.widgets[i] <= MAX_WIDGET_TYPE)
          settings.widgetList[n++] = settings.widgets[i];
      }
      for (int i = 0; i < 3; i++) {
        if (settings.widgets2[i] != EMPTY && settings.widgets2[i] <= MAX_WIDGET_TYPE)
          settings.widgetList[n++] = settings.widgets2[i];
      }
      settings.widgetCount = n;
    }

    if (persist_exists(SETTINGS_PERSIST_KEY) && !settings.sidebarOnLeft) {
      // upgrade whose primary was on the right -> move the single list right
      int moved = settings.widgetCount;
      if (moved > MAX_WIDGET_LIST) moved = MAX_WIDGET_LIST;
      for (int i = 0; i < moved; i++) {
        settings.rightWidgetList[i] = settings.widgetList[i];
      }
      settings.rightWidgetCount = moved;
      settings.widgetCount = 0;
    }
  }

  // One-time 16->32-byte widget-list migration. Older builds stored the lists in the
  // 16-byte widgetListV1/rightWidgetListV1 fields; carry them into the new, larger
  // widgetList/rightWidgetList arrays once. Only fills a list that is still EMPTY, so
  // it never clobbers a fresh install's just-built list nor a list already populated
  // (e.g. by a config save after a struct-grow build that shipped without this
  // migration). widgetListV2Init makes it a one-time step.
  if (!settings.widgetListV2Init) {
    settings.widgetListV2Init = true;
    migrated = true;   // persist the flag + carried list so this runs exactly once
    if (settings.widgetCount == 0 && settings.widgetCountV1 > 0) {
      int n = settings.widgetCountV1;
      if (n > MAX_WIDGET_LIST_V1) { n = MAX_WIDGET_LIST_V1; }
      memcpy(settings.widgetList, settings.widgetListV1, n);
      settings.widgetCount = n;
    }
    if (settings.rightWidgetCount == 0 && settings.rightWidgetCountV1 > 0) {
      int n = settings.rightWidgetCountV1;
      if (n > MAX_WIDGET_LIST_V1) { n = MAX_WIDGET_LIST_V1; }
      memcpy(settings.rightWidgetList, settings.rightWidgetListV1, n);
      settings.rightWidgetCount = n;
    }
  }

  // Sanitize loaded settings: a value that is out of range (e.g. from an
  // earlier build whose message-key IDs were shifted, writing a color/garbage
  // value into languageId) would index arrays like dayNames[37] out of bounds
  // and crash on every draw -- including at launch, since the bad value is
  // persisted and survives reinstalls. Clamp anything used as an array index
  // back to a safe default so the watchface can always start.
  bool clamped = false;

  if (settings.languageId > LANGUAGE_IW) { settings.languageId = LANGUAGE_EN; clamped = true; }
  if (settings.clockFontId > FONT_SETTING_BITHAM) { settings.clockFontId = FONT_SETTING_DEFAULT; clamped = true; }
  if (settings.bigDateFontId > BIG_DATE_FONT_SERIF) { settings.bigDateFontId = BIG_DATE_FONT_BITHAM; clamped = true; }
  if (settings.hourlyVibe > VIBE_EVERY_HALF_HOUR) { settings.hourlyVibe = NO_VIBE; clamped = true; }
  for (int i = 0; i < 3; i++) {
    if (settings.widgets[i] > MAX_WIDGET_TYPE) { settings.widgets[i] = EMPTY; clamped = true; }
    if (settings.widgets2[i] > MAX_WIDGET_TYPE) { settings.widgets2[i] = EMPTY; clamped = true; }
  }
  if (settings.widgetCount > MAX_WIDGET_LIST) { settings.widgetCount = MAX_WIDGET_LIST; clamped = true; }
  { int before = settings.widgetCount;
    settings.widgetCount = WidgetList_sanitize(settings.widgetList, settings.widgetCount, MAX_WIDGET_LIST);
    if (settings.widgetCount != before) { clamped = true; } }
  if (settings.rightWidgetCount > MAX_WIDGET_LIST) { settings.rightWidgetCount = MAX_WIDGET_LIST; clamped = true; }
  { int before = settings.rightWidgetCount;
    settings.rightWidgetCount = WidgetList_sanitize(settings.rightWidgetList, settings.rightWidgetCount, MAX_WIDGET_LIST);
    if (settings.rightWidgetCount != before) { clamped = true; } }
  if (settings.decimalSeparator != '.' && settings.decimalSeparator != ',') {
    settings.decimalSeparator = '.'; clamped = true;
  }
  settings.altclockName[sizeof(settings.altclockName) - 1] = '\0';
  if (settings.pollIntervalMin < 5 || settings.pollIntervalMin > 240) {
    settings.pollIntervalMin = 30; clamped = true;
  }
  if (settings.elecQuietStart > 23) { settings.elecQuietStart = 23; clamped = true; }
  if (settings.elecQuietEnd > 23) { settings.elecQuietEnd = 7; clamped = true; }
  // Fail safe on a garbled mode: OFF keeps rotation running rather than freezing it.
  if (settings.nightMode > NIGHT_WINDOW_CUSTOM) {
    settings.nightMode = NIGHT_WINDOW_OFF; clamped = true;
  }
  if (settings.nightStartHour > 23) { settings.nightStartHour = 23; clamped = true; }
  if (settings.nightEndHour > 23) { settings.nightEndHour = 7; clamped = true; }
  if (settings.nightSlowRotation > 1) { settings.nightSlowRotation = 1; clamped = true; }
  if (settings.nightColors > 1) { settings.nightColors = 0; clamped = true; }
  if (settings.elecCheapFactorPct < 1 || settings.elecCheapFactorPct > 100) {
    settings.elecCheapFactorPct = 70; clamped = true;
  }
  if (settings.autoBatteryThreshold < 1 || settings.autoBatteryThreshold > 100) {
    settings.autoBatteryThreshold = 10; clamped = true;
  }
  if (settings.fallbackColumn > 2) { settings.fallbackColumn = 0; clamped = true; }
  if (settings.fallbackPosition < 1 || settings.fallbackPosition > MAX_WIDGET_SLOTS + 1) {
    settings.fallbackPosition = 1; clamped = true;
  }
  if (settings.clockStyle > CLOCK_STYLE_ANALOG) { settings.clockStyle = CLOCK_STYLE_DIGITAL; clamped = true; }
  if (settings.analogTickStyle > ANALOG_TICKS_BOLD) { settings.analogTickStyle = ANALOG_TICKS_BOLD; clamped = true; }
  if (settings.analogDigitalClock > 1) { settings.analogDigitalClock = 0; clamped = true; }
  if (settings.statusClockDigital > 1) { settings.statusClockDigital = 0; clamped = true; }
  if (settings.batteryWarnPct > 100) { settings.batteryWarnPct = 0; clamped = true; }
  // 100 tenths = 10 days; a larger threshold would mean "always warning".
  if (settings.batteryWarnDaysTenths > 100) { settings.batteryWarnDaysTenths = 0; clamped = true; }
  if (settings.btWarnBorder > 1) { settings.btWarnBorder = 0; clamped = true; }

  if (migrated) { APP_LOG(APP_LOG_LEVEL_INFO, "settings: one-time widget-list migration applied"); }
  if (clamped)  { APP_LOG(APP_LOG_LEVEL_WARNING, "settings out of range, clamped to safe values"); }
  if (clamped || migrated) {
    persist_write_data(SETTINGS_PERSIST_KEY, &settings, sizeof(settings));
    persist_write_int(SETTINGS_VERSION_PERSIST_KEY, CURRENT_SETTINGS_VERSION);
  }

#ifdef SCREENSHOT_FIXTURES
  // Appstore screenshot scene config (no phone). Applied after sanitize so it wins;
  // not persisted, so it never pollutes real settings.
  settings.timeBgColor = GColorWhite;              // watchface background
  settings.timeColor = GColorBlack;                // clock text (dark on white)
  settings.sidebarTextColor = GColorBlack;
  settings.sidebarBgColorLeft = GColorMintGreen;   // LEFT (primary) column bg
  settings.sidebarBgColorRight = GColorBabyBlueEyes;  // RIGHT (secondary) column bg — pale blue
  settings.dateBgColor = GColorIcterine;           // light-yellow date header
  settings.twtStatusBgColor = GColorCeleste;       // status strip (scenes 2/3) — pale turquoise
  settings.clockFontId = FONT_SETTING_LECO;
  settings.showBigDate = true;
  settings.showBigDateMonth = true;
  settings.clockStyle = CLOCK_STYLE_ANALOG;        // analog face for this scene
  settings.analogTickStyle = ANALOG_TICKS_BOLD;    // bold hour ticks
  settings.analogDigitalClock = true;              // digital time line under the analog clock
  settings.secondaryAlwaysOn = true;               // keep the right column up without a status strip
  settings.useMetric = true;                       // weather in °C (18/11)
  settings.midiSecondPrecision = false;
  // Feature-rich two-sidebar showcase scene. (A clean single-sidebar variant is
  // a manual tweak: L={2,6,3}=battery/week/alt-tz, rightWidgetCount=0,
  // secondaryAlwaysOn=false.)
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
  WidgetList_forEachId(settings.widgetList, settings.widgetCount, dyn_scan_cb, NULL);
  WidgetList_forEachId(settings.rightWidgetList, settings.rightWidgetCount, dyn_scan_cb, NULL);
}

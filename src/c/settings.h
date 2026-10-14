#pragma once
#include <pebble.h>
#include "sidebar_widgets.h"

#define CURRENT_SETTINGS_VERSION 1

// persistent storage keys
#define SETTINGS_PERSIST_KEY 100
#define SETTINGS_VERSION_PERSIST_KEY 4

// Analog hour-tick style (SettingAnalogTicks): 0=hide, 1=normal, 2=bold.
#define ANALOG_TICKS_HIDE   0
#define ANALOG_TICKS_NORMAL 1
#define ANALOG_TICKS_BOLD   2

typedef enum {
  NO_VIBE = 0,
  VIBE_EVERY_HOUR = 1,
  VIBE_EVERY_HALF_HOUR = 2
} VibeIntervalType;

// Settings struct. Persisted as one raw blob (SETTINGS_PERSIST_KEY); a blob written
// with a different CURRENT_SETTINGS_VERSION is discarded and defaults are used.
typedef struct {
  // Colors. A colour a user can configure must ALSO be mapped in Theme_update
  // (src/c/theme.c): drawing code reads theme.*, never settings.*Color, directly.
  GColor timeColor;
  GColor timeBgColor;
  GColor sidebarColor;
  GColor sidebarTextColor;
  GColor sidebarBgColorLeft;   // GColorClear = inherit sidebarColor
  GColor sidebarBgColorRight;  // GColorClear = inherit sidebarColor

  // general
  uint8_t languageId;
  bool showLeadingZero;
  bool btVibe;
  VibeIntervalType hourlyVibe;
  uint8_t pollIntervalMin;   // watch->phone data request interval (min)

  // widget options
  bool useLargeFonts;
  bool activateDisconnectIcon;
  bool useMetric;
  bool showBatteryPct;
  bool disableAutobattery;
  uint8_t autoBatteryThreshold;  // auto-battery shows at or below this percent (or while charging)
  char altclockName[8];
  int altclockOffset;
  char decimalSeparator;

  // Fallback (auto-battery / BT-disconnect) placement:
  //   fallbackColumn: 0 = Automatic, 1 = Left list, 2 = Right list.
  //   fallbackPosition: 1-based slot; <= count replaces, == count+1 appends.
  uint8_t fallbackColumn;
  uint8_t fallbackPosition;

  // cheap-electricity widgets
  uint8_t elecQuietStart;        // quiet-hours start hour 0-23
  uint8_t elecQuietEnd;          // quiet-hours end hour 0-23
  uint8_t elecCheapFactorPct;    // cheapBar = mean*pct/100
  int16_t elecCheapFloorCenti;   // cheapBar floor, 0.01 snt
  int16_t elecCheapCeilingCenti; // cheapBar ceiling, 0.01 snt

  // analog clock hour-tick style: 0=hide, 1=normal, 2=bold
  uint8_t analogTickStyle;

  // widget lists (see widget_list.h for the byte format)
  uint8_t widgetList[MAX_WIDGET_LIST];
  uint8_t widgetCount;
  uint8_t rightWidgetList[MAX_WIDGET_LIST];
  uint8_t rightWidgetCount;

  // Night window (NightWindowMode; [start, end) hours, may wrap midnight) and its
  // two consumers: slowing sub-minute rotation and the night palette.
  uint8_t nightMode;
  uint8_t nightStartHour;
  uint8_t nightEndHour;
  uint8_t nightSlowRotation;   // bool
  uint8_t nightColors;         // bool
  GColor nightBgColor;
  GColor nightFgColor;

  // Warning frame: battery percent / battery-days (tenths) triggers (0 = off) and
  // the BT-disconnect trigger.
  uint8_t batteryWarnPct;
  uint8_t batteryWarnDaysTenths;
  GColor batteryWarnColor;
  uint8_t btWarnBorder;   // bool
  GColor btWarnColor;
} Settings;

// Dynamic settings (calculated at runtime based on currently-selected widgets)
typedef struct {
  bool disableWeather;
  bool updateScreenEverySecond;
  bool enableAutoBatteryWidget;
  bool enableBeats;
  bool enableAltTimeZone;
} DynamicSettings;

extern Settings settings;
extern DynamicSettings dynamicSettings;

void Settings_init();
void Settings_deinit();
void Settings_loadFromStorage();
void Settings_saveToStorage();
void Settings_updateDynamicSettings();

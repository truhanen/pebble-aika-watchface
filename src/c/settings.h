#pragma once
#include <pebble.h>
#include "widgets.h"

#define CURRENT_SETTINGS_VERSION 1

// persistent storage keys
#define SETTINGS_PERSIST_KEY 100
#define SETTINGS_VERSION_PERSIST_KEY 4
#define WIDGET_FONTS_PERSIST_KEY 101

// Widget positions. The numbers equal WlPosition (widget_layout_calc.h).
#define WIDGET_POS_LEFT 0
#define WIDGET_POS_RIGHT 1
#define WIDGET_POS_TOP 2
#define WIDGET_POS_BOTTOM 3
#define WIDGET_POS_COUNT 4
#define MAX_CENTER_WIDGET_LIST 16   // byte budget of a top / bottom list

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

  // general
  uint8_t languageId;
  bool showLeadingZero;
  bool btVibe;
  VibeIntervalType hourlyVibe;
  uint8_t pollIntervalMin;   // watch->phone data request interval (min)

  // widget options
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

  // widget lists (see widget_list.h for the byte format)
  uint8_t widgetList[MAX_WIDGET_LIST];
  uint8_t widgetCount;
  uint8_t rightWidgetList[MAX_WIDGET_LIST];
  uint8_t rightWidgetCount;
  // Top / bottom centre lists, same byte format; only the first slot is shown. Nothing
  // fills them from the phone yet (payload v2), the emulator test hooks do.
  uint8_t topWidgetList[MAX_CENTER_WIDGET_LIST];
  uint8_t topWidgetCount;
  uint8_t bottomWidgetList[MAX_CENTER_WIDGET_LIST];
  uint8_t bottomWidgetCount;

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

// Per-widget font (widget_fonts.h index), kept in a persist key of its own: one byte per
// byte of the position's widget list (the member's offset in it, see WidgetSlot.offset),
// WIDGET_FONT_UNSET = the position's default.
#define WIDGET_FONT_UNSET 0xFF
typedef struct {
  uint8_t font[WIDGET_POS_COUNT][MAX_WIDGET_LIST];
} WidgetFonts;

extern Settings settings;
extern WidgetFonts widgetFonts;
extern DynamicSettings dynamicSettings;

void Settings_init();
void Settings_deinit();
void Settings_loadFromStorage();
void Settings_saveToStorage();
void Settings_updateDynamicSettings();

// The bytes and length of a position's widget list.
const uint8_t *Settings_widgetList(int pos, int *count);
// Font of the widget at byte `offset` of a position's list: the stored one, else the
// position's default (side widgets Gothic medium, top / bottom Bitham large).
int Settings_widgetFont(int pos, int offset);
// Set every font of a position (what a single-font config, or a test, wants).
void Settings_setWidgetFonts(int pos, uint8_t font);
// Every drawable widget id in all four lists (group members included).
void Settings_forEachWidgetId(void (*cb)(uint8_t id, void *ctx), void *ctx);
// Shortest sub-minute rotation interval over all four lists, or 0.
int Settings_minSubMinuteIntervalSec(void);
// Re-sanitize the lists after they changed.
void Settings_sanitizeWidgetLists(void);

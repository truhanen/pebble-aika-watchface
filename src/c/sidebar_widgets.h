#pragma once
#include <pebble.h>

/*
 * A global x offset used for nudging the widgets left and right
 * Included for round support
 */
extern int SidebarWidgets_xOffset;

/*
 * Set to the widget type/id about to be drawn, immediately before each
 * widget.draw() call (same idiom as SidebarWidgets_xOffset). The generic crypto
 * draw reads it to know WHICH coin (wid) it is rendering — the draw fn pointer
 * carries no id.
 */
extern uint8_t SidebarWidgets_currentWidgetType;

/*
 * Set to true immediately before a widget's getHeight()/draw() when that widget's
 * identifier (icon or title label) should be hidden (per-widget config flag).
 * Same idiom as SidebarWidgets_currentWidgetType.
 */
extern bool SidebarWidgets_hideIdentifier;

/*
 * The different types of sidebar widgets:
 * we'll give them numbers so that we can index them in settings
 */
typedef enum {
  EMPTY                     = 0,
  BLUETOOTH_DISCONNECT      = 1,
  BATTERY_METER             = 2,
  ALT_TIME_ZONE             = 3,
  DATE                      = 4,
  SECONDS                   = 5,
  WEEK_NUMBER               = 6,
  WEATHER_CURRENT           = 7,
  WEATHER_FORECAST_TODAY    = 8,
  SLEEP_TIMER               = 9,
  STEP_COUNTER              = 10,
  BEATS                     = 11,
  HEARTRATE                 = 12,
  WEATHER_UV_INDEX          = 13,
  ELECTRICITY               = 14,
  BTC_PRICE                 = 15,
  XMR_PRICE                 = 16,
  EURUSD_RATE               = 17,
  NEXT_CHEAP_ELEC           = 18,
  CHEAPEST_ELEC_HOUR        = 19,
  DEEP_SLEEP_TIMER          = 20,
  DISTANCE                  = 21,
  BATTERY_DAYS              = 22,
  TUYA_LEDS                 = 23,
  SLEEP_COMBINED            = 24,
  SLEEP_DEEP_BAR            = 25
} SidebarWidgetType;

// Highest valid widget id; bump when appending a widget type. Used for the
// settings clamp and the config-message bounds checks.
#define MAX_WIDGET_TYPE SLEEP_DEEP_BAR

// Maximum length of the configurable widget priority list (storage + protocol
// buffer). The watch shows as many as fit by height; this only bounds the buffer.
#define MAX_WIDGET_LIST 32

typedef struct {
  /*
   * Returns the pixel height of the widget, taking into account all current
   * settings that would affect this, such as font size
   */
  int (*getHeight)();

  /*
   * Draws the widget using the provided graphics context
   */
  void (*draw)(GContext* ctx, int yPosition);
} SidebarWidget;

void SidebarWidgets_init();
void SidebarWidgets_deinit();
SidebarWidget getSidebarWidgetByType(SidebarWidgetType type);
void SidebarWidgets_updateFonts();
void SidebarWidgets_updateTime(struct tm* timeInfo);

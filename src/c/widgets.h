#pragma once
#include <pebble.h>

/*
 * The different types of widgets:
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
} WidgetType;

// Highest valid widget id; bump when appending a widget type. Used for the
// settings clamp and the config-message bounds checks.
#define MAX_WIDGET_TYPE SLEEP_DEEP_BAR

// Maximum length of the configurable widget list (storage + protocol buffer). The
// layer shows as many as fit by height; this only bounds the buffer.
#define MAX_WIDGET_LIST 32

// What the widget layer hands every widget call.
typedef struct {
  uint8_t id;         // WidgetType, or a crypto / currency / tuya wid
  uint8_t font;       // widget_fonts.h index of the value font
  bool hide_icon;     // identifier (icon or unit) hidden by the user
} WidgetCtx;

// What a widget reports about itself. Everything is 0 for a widget that draws nothing.
typedef struct {
  int16_t value_w;    // advance width of the value text
  int16_t ink_h;      // height of the value's ink = ink height of the font's digits
  int16_t icon_w;     // width of the icon or unit beside the value (0 = none / hidden)
  int16_t icon_h;     // its height; the icon is centred on the value's ink
} WidgetSize;

// A widget is drawn in two passes: draw_icon happens below the clock hands, draw_value
// above them. The boxes come from widget_layer.c; the widget draws into them as given.
typedef struct {
  void (*measure)(const WidgetCtx *ctx, WidgetSize *out);
  void (*draw_icon)(GContext *g, const WidgetCtx *ctx, GRect icon_box);
  void (*draw_value)(GContext *g, const WidgetCtx *ctx, GRect value_box, GTextAlignment align);
} Widget;

// The widget for an id; a widget not ported to this API yet measures to zero and
// draws nothing.
const Widget *Widgets_get(uint8_t id);

void Widgets_init(void);
void Widgets_deinit(void);

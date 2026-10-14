#include "sidebar.h"
#include "languages.h"
#include "settings.h"
#include "theme.h"
#include "sidebar_widgets.h"
#include "weather.h"
#include "twt_status.h"
#include "crypto.h"
#include "widget_list.h"
#include <ctype.h>
#include <math.h>
#include <pebble.h>

GRect screen_rect;
int sidebarWidth;

static void update_sidebar_width() {
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  sidebarWidth = settings.useLargeFonts ? 39 : 34;
#else
  sidebarWidth = 30;
#endif
}

// "private" functions
// layer update callbacks
#ifndef PBL_ROUND
void updateRectSidebar(Layer *l, GContext *ctx);
void updateRectSecondarySidebar(Layer *l, GContext *ctx);
#else

void updateRoundSidebarLeft(Layer *l, GContext *ctx);
void updateRoundSidebarRight(Layer *l, GContext *ctx);

// shared drawing stuff between all layers
void drawRoundSidebar(GContext *ctx, GRect bgBounds,
                      SidebarWidgetType widgetType, int widgetXOffset);
#endif

Layer *sidebarLayer;

#ifndef PBL_ROUND
Layer *secondarySidebarLayer;   // right column; shown when rightWidgetList is non-empty
#endif

#ifdef PBL_ROUND
Layer *sidebarLayer2;
#endif

void Sidebar_init(Window *window) {
  update_sidebar_width();
  // init the sidebar layer
  screen_rect = layer_get_bounds(window_get_root_layer(window));
  GRect bounds;

#ifdef PBL_ROUND
  GRect bounds2;
  bounds = GRect(0, 0, 40, screen_rect.size.h);
  bounds2 = GRect(screen_rect.size.w - 40, 0, 40, screen_rect.size.h);
#else
  if (!settings.sidebarOnLeft) {
    bounds = GRect(screen_rect.size.w - sidebarWidth, 0, sidebarWidth,
                   screen_rect.size.h);
  } else {
    bounds = GRect(0, 0, sidebarWidth, screen_rect.size.h);
  }
#endif

  // init the widgets
  SidebarWidgets_init();

  sidebarLayer = layer_create(bounds);
  layer_add_child(window_get_root_layer(window), sidebarLayer);

#ifdef PBL_ROUND
  layer_set_update_proc(sidebarLayer, updateRoundSidebarLeft);
#else
  layer_set_update_proc(sidebarLayer, updateRectSidebar);
#endif

#ifndef PBL_ROUND
  // Secondary panel: created hidden; main.c positions/shows it while a status
  // display is visible. Only meaningful where the status layout runs (rect,
  // non-aplite), but creating it hidden everywhere rect is harmless.
  secondarySidebarLayer = layer_create(GRect(0, 0, sidebarWidth, screen_rect.size.h));
  layer_set_update_proc(secondarySidebarLayer, updateRectSecondarySidebar);
  layer_set_hidden(secondarySidebarLayer, true);
  layer_add_child(window_get_root_layer(window), secondarySidebarLayer);
#endif

#ifdef PBL_ROUND
  sidebarLayer2 = layer_create(bounds2);
  layer_add_child(window_get_root_layer(window), sidebarLayer2);
  layer_set_update_proc(sidebarLayer2, updateRoundSidebarRight);
#endif
}

void Sidebar_deinit() {
  layer_destroy(sidebarLayer);

#ifndef PBL_ROUND
  layer_destroy(secondarySidebarLayer);
#endif

#ifdef PBL_ROUND
  layer_destroy(sidebarLayer2);
#endif

  SidebarWidgets_deinit();
}

void Sidebar_redraw() {
  update_sidebar_width();
#ifndef PBL_ROUND
  // On platforms where the status layout runs, main.c owns the panel frames
  // (apply_twt_layout); don't fight it here. Elsewhere, keep positioning the
  // primary ourselves.
  if (!TwtStatus_isSupported()) {
    // No status layout here (aplite): single sidebar = the LEFT (primary) list,
    // drawn in full (overflow clipped). The secondary panel stays hidden.
    // Known limitation: aplite has no right column, so an upgrade user whose list
    // was migrated to the right (sidebarOnLeft==false) sees an empty sidebar until
    // they reconfigure. Aplite is a legacy/non-goal platform; accepted.
    Sidebar_distributeWidgets(NULL, NULL);
    if (!settings.sidebarOnLeft) {
      layer_set_frame(sidebarLayer, GRect(screen_rect.size.w - sidebarWidth, 0,
                                          sidebarWidth, screen_rect.size.h));
    } else {
      layer_set_frame(sidebarLayer,
                      GRect(0, 0, sidebarWidth, screen_rect.size.h));
    }
  }
  layer_mark_dirty(secondarySidebarLayer);
#endif

  // redraw the layer
  layer_mark_dirty(sidebarLayer);

#ifdef PBL_ROUND
  layer_mark_dirty(sidebarLayer2);
#endif
}

void Sidebar_updateTime(struct tm *timeInfo) {
  SidebarWidgets_updateTime(timeInfo);
}

bool isAutoBatteryShown() {
  if (!settings.disableAutobattery) {
    BatteryChargeState chargeState = battery_state_service_peek();

    if (dynamicSettings.enableAutoBatteryWidget) {
      if (chargeState.charge_percent <= settings.autoBatteryThreshold || chargeState.is_charging) {
        return true;
      }
    }
  }

  return false;
}

#ifdef PBL_ROUND

// returns the best candidate widget for replacement by the auto battery
// or the disconnection icon
int getReplacableWidget() {
  if (settings.widgets[0] == EMPTY) {
    return 0;
  } else if (settings.widgets[2] == EMPTY) {
    return 2;
  }

  if (settings.widgets[0] == WEATHER_CURRENT ||
      settings.widgets[0] == WEATHER_FORECAST_TODAY) {
    return 0;
  } else if (settings.widgets[2] == WEATHER_CURRENT ||
             settings.widgets[2] == WEATHER_FORECAST_TODAY) {
    return 2;
  }

  // if we don't have any of those things, just replace the left widget
  return 0;
}

// Which round slot (0=left, 2=right) the fallback occupies. Manual settings pin a
// side; Automatic (0) keeps the legacy weather/empty heuristic. Position is N/A on
// round (one slot per side).
static int roundFallbackSlot() {
  if (settings.fallbackColumn == 1) { return 0; }
  if (settings.fallbackColumn == 2) { return 2; }
  return getReplacableWidget();
}

#else

// Best slot for the auto-battery / disconnect-icon substitution. A rotating group is
// opaque (only a PLAIN weather slot is a preferred target); EMPTY slots no longer
// exist (WidgetList_parse drops them).
static int getReplacableWidget(const WidgetSlot slots[], int count) {
  for (int i = 0; i < count; i++) {
    if (slots[i].count == 1 &&
        (slots[i].members[0] == WEATHER_CURRENT || slots[i].members[0] == WEATHER_FORECAST_TODAY)) {
      return i;
    }
  }
  return (count > 0) ? (count - 1) : 0;
}

#endif

#ifdef PBL_ROUND

void updateRoundSidebarRight(Layer *l, GContext *ctx) {
  GRect bounds = layer_get_bounds(l);
  GRect bgBounds = GRect(bounds.origin.x, bounds.size.h / -2, bounds.size.h * 2,
                         bounds.size.h * 2);

  bool showDisconnectIcon = !bluetooth_connection_service_peek();
  bool showAutoBattery = isAutoBatteryShown();

  SidebarWidgetType displayWidget = settings.widgets[2];

  if ((showAutoBattery || showDisconnectIcon) && roundFallbackSlot() == 2) {
    if (showAutoBattery) {
      displayWidget = BATTERY_METER;
    } else if (showDisconnectIcon) {
      displayWidget = BLUETOOTH_DISCONNECT;
    }
  }

  drawRoundSidebar(ctx, bgBounds, displayWidget, 3);
}

void updateRoundSidebarLeft(Layer *l, GContext *ctx) {
  GRect bounds = layer_get_bounds(l);
  GRect bgBounds =
      GRect(bounds.origin.x - bounds.size.h * 2 + bounds.size.w,
            bounds.size.h / -2, bounds.size.h * 2, bounds.size.h * 2);

  bool showDisconnectIcon = !bluetooth_connection_service_peek();
  bool showAutoBattery = isAutoBatteryShown();
  SidebarWidgetType displayWidget = settings.widgets[0];

  if ((showAutoBattery || showDisconnectIcon) && roundFallbackSlot() == 0) {
    if (showAutoBattery) {
      displayWidget = BATTERY_METER;
    } else if (showDisconnectIcon) {
      displayWidget = BLUETOOTH_DISCONNECT;
    }
  }

  drawRoundSidebar(ctx, bgBounds, displayWidget, 7);
}

void Sidebar_distributeWidgets(int *primaryCountOut, int *secondaryCountOut) {
  // Round keeps its fixed two-widget rendering (settings.widgets[0]/[2]).
  if (primaryCountOut) { *primaryCountOut = 0; }
  if (secondaryCountOut) { *secondaryCountOut = 0; }
}

void drawRoundSidebar(GContext *ctx, GRect bgBounds,
                      SidebarWidgetType widgetType, int widgetXOffset) {
  SidebarWidgets_updateFonts();

  // Round has a single continuous sidebar -> use the primary background color
  // (legacy key sidebarBgColorLeft); GColorClear = inherit theme.sidebarColor.
  GColor roundBg = gcolor_equal(theme.sidebarBgColorLeft, GColorClear)
      ? theme.sidebarColor : theme.sidebarBgColorLeft;
  graphics_context_set_fill_color(ctx, roundBg);

  graphics_fill_radial(ctx, bgBounds, GOvalScaleModeFillCircle, 100,
                       DEG_TO_TRIGANGLE(0), TRIG_MAX_ANGLE);

  SidebarWidgets_xOffset = widgetXOffset;
  SidebarWidget widget = getSidebarWidgetByType(widgetType);

  // calculate center position of the widget
  int widgetPosition = bgBounds.size.h / 4 - widget.getHeight() / 2;
  SidebarWidgets_currentWidgetType = (uint8_t)widgetType;
  SidebarWidgets_hideIdentifier = false;   // round shows a single widget; toggle is a no-op
  widget.draw(ctx, widgetPosition);
}

#else

// Display columns computed from settings.widgetList by Sidebar_distributeWidgets();
// the rect update procs draw these instead of reading the settings arrays.
static WidgetSlot primaryColumn[MAX_WIDGET_SLOTS];
static int primaryColumnCount = 0;
static WidgetSlot secondaryColumn[MAX_WIDGET_SLOTS];
static int secondaryColumnCount = 0;

// Seconds-of-day drives the rotating-slot member selection. A build-time override
// pins it for deterministic rotation screenshots.
static int currentSecondsOfDay(void) {
#ifdef ROTATION_FAKE_SECONDS
  return ROTATION_FAKE_SECONDS;
#else
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);
  return lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec;
#endif
}

// A rotating slot reserves the MAX height over its members, so the column layout is
// stable as the active member changes. Sets SidebarWidgets_currentWidgetType as a
// side effect (crypto getHeight reads it); callers re-set it before each draw.
static int widgetSlotHeight(const WidgetSlot *slot) {
  int maxH = 0;
  for (int m = 0; m < slot->count; m++) {
    SidebarWidgets_currentWidgetType = slot->members[m];
    SidebarWidgets_hideIdentifier = slot->hide[m];
    int h = getSidebarWidgetByType(slot->members[m]).getHeight();
    if (h > maxH) { maxH = h; }
  }
  return maxH;
}

// Number of slots (from the top) FULLY visible in a column of inner height
// `innerHeight`. Uses each slot's reserved (max-member) height.
static int columnVisibleCount(const WidgetSlot slots[], int count, int innerHeight) {
  if (count <= 0) { return 0; }
  int total = 0;
  for (int i = 0; i < count; i++) { total += widgetSlotHeight(&slots[i]); }
  if (total + (count - 1) * V_PADDING_DEFAULT <= innerHeight) { return count; }
  int y = 0, visible = 0;
  for (int i = 0; i < count; i++) {
    int h = widgetSlotHeight(&slots[i]);
    if (y + h <= innerHeight) { visible++; y += h + V_PADDING_DEFAULT; } else { break; }
  }
  return visible;
}

void Sidebar_distributeWidgets(int *primaryCountOut, int *secondaryCountOut) {
  SidebarWidgets_updateFonts();  // ensure layout heights are valid before packing
  primaryColumnCount = WidgetList_parse(settings.widgetList, settings.widgetCount,
                                        primaryColumn, MAX_WIDGET_SLOTS);
  secondaryColumnCount = WidgetList_parse(settings.rightWidgetList, settings.rightWidgetCount,
                                          secondaryColumn, MAX_WIDGET_SLOTS);
  if (primaryCountOut) { *primaryCountOut = primaryColumnCount; }
  if (secondaryCountOut) { *secondaryCountOut = secondaryColumnCount; }
}

static void drawWidgetColumn(Layer *l, GContext *ctx,
                             const WidgetSlot slots[], int slotCount,
                             bool allowReplacement, bool isPrimary) {
  GRect bounds = layer_get_unobstructed_bounds(l);

  SidebarWidgets_xOffset = (sidebarWidth - 30) / 2;   // zero on every rect platform besides emery
  SidebarWidgets_updateFonts();

  GColor sidebarBg = isPrimary ? theme.sidebarBgColorLeft : theme.sidebarBgColorRight;
  if (gcolor_equal(sidebarBg, GColorClear)) { sidebarBg = theme.sidebarColor; }
  graphics_context_set_fill_color(ctx, sidebarBg);
  graphics_fill_rect(ctx, layer_get_bounds(l), 0, GCornerNone);
  graphics_context_set_text_color(ctx, theme.sidebarTextColor);

  int v_padding = V_PADDING_DEFAULT;
  int innerTop = v_padding;
  int innerHeight = bounds.size.h - v_padding * 2;
  int sod = currentSecondsOfDay();

  // Resolve each configured slot to the widget it shows now + its reserved height.
  SidebarWidgetType activeType[MAX_WIDGET_SLOTS];
  bool activeHide[MAX_WIDGET_SLOTS];
  int slotHeight[MAX_WIDGET_SLOTS];
  int n = (slotCount > MAX_WIDGET_SLOTS) ? MAX_WIDGET_SLOTS : slotCount;
  for (int i = 0; i < n; i++) {
    activeType[i] = (SidebarWidgetType)WidgetSlot_activeMember(&slots[i], sod);
    activeHide[i] = WidgetSlot_activeHide(&slots[i], sod);
    slotHeight[i] = widgetSlotHeight(&slots[i]);
  }

  // Auto-battery / disconnect-icon fallback (battery has priority over BT).
  if (allowReplacement) {
    bool showDisconnectIcon = settings.activateDisconnectIcon && !bluetooth_connection_service_peek();
    bool showAutoBattery = isAutoBatteryShown();
    if (showAutoBattery || showDisconnectIcon) {
      SidebarWidgetType fb = showAutoBattery ? BATTERY_METER : BLUETOOTH_DISCONNECT;
      int fbHeight = getSidebarWidgetByType(fb).getHeight();   // side-effects reset before draw below

      if (settings.fallbackColumn == 0) {
        // AUTOMATIC: legacy host-column + getReplacableWidget, replacement only.
        int myVisible = columnVisibleCount(slots, n, innerHeight);
        bool hostHere;
        if (isPrimary) {
          hostHere = (myVisible > 0);
        } else {
          int leftVisible = columnVisibleCount(primaryColumn, primaryColumnCount, innerHeight);
          hostHere = (leftVisible == 0 && myVisible > 0);
        }
        if (hostHere) {
          int idx = getReplacableWidget(slots, myVisible);
          activeType[idx] = fb;
          activeHide[idx] = false;
          slotHeight[idx] = fbHeight;
        }
      } else {
        // MANUAL: fallback pinned to the chosen column at the configured position.
        bool chosenColumn = (settings.fallbackColumn == 1 && isPrimary) ||
                            (settings.fallbackColumn == 2 && !isPrimary);
        if (chosenColumn) {
          int myVisible = columnVisibleCount(slots, n, innerHeight);
          int totalH = 0;
          for (int i = 0; i < n; i++) { totalH += slotHeight[i]; }
          bool appendFits = (myVisible == n) && (n < MAX_WIDGET_SLOTS) &&
                            (totalH + fbHeight + n * v_padding <= innerHeight);
          int idx; bool append;
          WidgetList_fallbackPlace(n, myVisible, appendFits, settings.fallbackPosition, &idx, &append);
          if (append) {
            activeType[n] = fb; activeHide[n] = false; slotHeight[n] = fbHeight;
            n++;
          } else {
            activeType[idx] = fb; activeHide[idx] = false; slotHeight[idx] = fbHeight;
          }
        }
      }
    }
  }

  if (n == 0) { return; }

  if (n == 1) {
    int y = innerTop + (innerHeight - slotHeight[0]) / 2;
    SidebarWidgets_currentWidgetType = (uint8_t)activeType[0];
    SidebarWidgets_hideIdentifier = activeHide[0];
    getSidebarWidgetByType(activeType[0]).draw(ctx, y);
    return;
  }

  int totalHeight = 0;
  for (int i = 0; i < n; i++) { totalHeight += slotHeight[i]; }

  int gap;
  if (totalHeight + (n - 1) * v_padding <= innerHeight) {
    int slack = innerHeight - totalHeight;
    gap = slack / (n - 1);
  } else {
    gap = v_padding;
  }

  int y = innerTop;
  for (int i = 0; i < n; i++) {
    SidebarWidgets_currentWidgetType = (uint8_t)activeType[i];
    SidebarWidgets_hideIdentifier = activeHide[i];
    getSidebarWidgetByType(activeType[i]).draw(ctx, y);
    y += slotHeight[i] + gap;
  }
}

void updateRectSidebar(Layer *l, GContext *ctx) {
  drawWidgetColumn(l, ctx, primaryColumn, primaryColumnCount, /*allowReplacement=*/true, /*isPrimary=*/true);
}

void updateRectSecondarySidebar(Layer *l, GContext *ctx) {
  drawWidgetColumn(l, ctx, secondaryColumn, secondaryColumnCount, /*allowReplacement=*/true, /*isPrimary=*/false);
}

#endif

// Frame setters: rect implementations move the real layers; no-ops on round so
// the shared header stays simple and main.c links on every platform.
void Sidebar_setPrimaryFrame(GRect frame) {
#ifndef PBL_ROUND
  layer_set_frame(sidebarLayer, frame);
#endif
}

void Sidebar_setPrimaryHidden(bool hidden) {
#ifndef PBL_ROUND
  layer_set_hidden(sidebarLayer, hidden);
#endif
}

void Sidebar_setSecondaryFrame(GRect frame) {
#ifndef PBL_ROUND
  layer_set_frame(secondarySidebarLayer, frame);
#endif
}

void Sidebar_setSecondaryHidden(bool hidden) {
#ifndef PBL_ROUND
  layer_set_hidden(secondarySidebarLayer, hidden);
#endif
}

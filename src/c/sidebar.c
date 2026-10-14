#include "sidebar.h"
#include "languages.h"
#include "settings.h"
#include "theme.h"
#include "sidebar_widgets.h"
#include "weather.h"
#include "crypto.h"
#include "widget_list.h"
#include <pebble.h>
#include "test_hooks.h"

GRect screen_rect;
int sidebarWidth;

static void update_sidebar_width() {
  sidebarWidth = settings.useLargeFonts ? 39 : 34;
}

void updateRectSidebar(Layer *l, GContext *ctx);
void updateRectSecondarySidebar(Layer *l, GContext *ctx);

Layer *sidebarLayer;
Layer *secondarySidebarLayer;   // right column; shown when rightWidgetList is non-empty

void Sidebar_init(Window *window) {
  update_sidebar_width();
  screen_rect = layer_get_bounds(window_get_root_layer(window));

  SidebarWidgets_init();

  // Both columns are created hidden; main.c positions and shows them.
  sidebarLayer = layer_create(GRect(0, 0, sidebarWidth, screen_rect.size.h));
  layer_set_update_proc(sidebarLayer, updateRectSidebar);
  layer_set_hidden(sidebarLayer, true);
  layer_add_child(window_get_root_layer(window), sidebarLayer);

  secondarySidebarLayer = layer_create(GRect(0, 0, sidebarWidth, screen_rect.size.h));
  layer_set_update_proc(secondarySidebarLayer, updateRectSecondarySidebar);
  layer_set_hidden(secondarySidebarLayer, true);
  layer_add_child(window_get_root_layer(window), secondarySidebarLayer);
}

void Sidebar_deinit() {
  layer_destroy(sidebarLayer);
  layer_destroy(secondarySidebarLayer);
  SidebarWidgets_deinit();
}

void Sidebar_redraw() {
  update_sidebar_width();
  layer_mark_dirty(secondarySidebarLayer);
  layer_mark_dirty(sidebarLayer);
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

  SidebarWidgets_xOffset = (sidebarWidth - 30) / 2;
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

void Sidebar_setPrimaryFrame(GRect frame) {
  layer_set_frame(sidebarLayer, frame);
}

void Sidebar_setPrimaryHidden(bool hidden) {
  layer_set_hidden(sidebarLayer, hidden);
}

void Sidebar_setSecondaryFrame(GRect frame) {
  layer_set_frame(secondarySidebarLayer, frame);
}

void Sidebar_setSecondaryHidden(bool hidden) {
  layer_set_hidden(secondarySidebarLayer, hidden);
}

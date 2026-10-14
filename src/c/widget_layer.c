#include "widget_layer.h"
#include "widgets.h"
#include "settings.h"
#include "widget_list.h"
#include "widget_fonts.h"
#include "widget_layout_calc.h"
#include "clock_face_calc.h"
#include "test_hooks.h"

// One slot as shown right now: the active member, its size, and the height the slot
// reserves (the tallest member, so the column does not reflow as a group rotates).
typedef struct {
  WidgetCtx ctx;
  WidgetSize size;
  int16_t slot_h;
} Active;

// An item with its final boxes.
typedef struct {
  WidgetCtx ctx;
  GRect icon_box, value_box;
  bool has_icon, has_value;
} Placed;

#define MAX_PLACED (2 * MAX_WIDGET_SLOTS + 2)

static WidgetSlot s_slots[WIDGET_POS_COUNT][MAX_WIDGET_SLOTS];
static int s_slot_count[WIDGET_POS_COUNT];
static Placed s_placed[MAX_PLACED];
static int s_placed_count;

void WidgetLayer_init(void) {
  Widgets_init();
  WidgetLayer_reload();
}

void WidgetLayer_deinit(void) { Widgets_deinit(); }

void WidgetLayer_reload(void) {
  for (int pos = 0; pos < WIDGET_POS_COUNT; pos++) {
    int n;
    const uint8_t *list = Settings_widgetList(pos, &n);
    bool center = pos == WIDGET_POS_TOP || pos == WIDGET_POS_BOTTOM;   // one slot at most
    s_slot_count[pos] = WidgetList_parse(list, n, s_slots[pos], center ? 1 : MAX_WIDGET_SLOTS);
  }
}

static int widget_height(const WidgetSize *s) {
  return s->ink_h > s->icon_h ? s->ink_h : s->icon_h;
}

// The font of a member: the test-hook override, else the one in its flags, else the
// position's default (side widgets Gothic medium, top / bottom Bitham large).
static int font_for(int pos, uint8_t flags) {
  int forced = TestHooks_font(pos);
  if (forced >= 0) { return forced; }
  int f = WidgetList_flagFont(flags);
  if (f != WL_FONT_POS_DEFAULT) { return f; }
  return (pos == WIDGET_POS_TOP || pos == WIDGET_POS_BOTTOM) ? WIDGET_FONT_DEFAULT_TOP_BOTTOM
                                                              : WIDGET_FONT_DEFAULT_SIDE;
}

static void measure_member(int pos, const WidgetSlot *slot, int member, WidgetCtx *ctx, WidgetSize *size) {
  ctx->id = slot->members[member];
  ctx->font = (uint8_t)font_for(pos, slot->flags[member]);
  ctx->hide_icon = WidgetList_flagHidden(slot->flags[member]);
  Widgets_get(ctx->id)->measure(ctx, size);
}

static void resolve_slot(int pos, const WidgetSlot *slot, int sod, Active *a) {
  int active = WidgetSlot_activeIndex(slot, sod);
  a->slot_h = 0;
  for (int m = 0; m < slot->count; m++) {
    WidgetCtx c;
    WidgetSize s;
    measure_member(pos, slot, m, &c, &s);
    if (m == active) { a->ctx = c; a->size = s; }
    if (widget_height(&s) > a->slot_h) { a->slot_h = (int16_t)widget_height(&s); }
  }
}

// Seconds-of-day drives the rotating-slot member selection. A build-time override
// pins it for deterministic rotation screenshots.
static int seconds_of_day(void) {
#ifdef ROTATION_FAKE_SECONDS
  return ROTATION_FAKE_SECONDS;
#else
  time_t t = time(NULL);
  struct tm *lt = localtime(&t);
  return lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec;
#endif
}

static bool auto_battery_shown(void) {
  if (settings.disableAutobattery || !dynamicSettings.enableAutoBatteryWidget) { return false; }
  BatteryChargeState st = battery_state_service_peek();
  return st.charge_percent <= settings.autoBatteryThreshold || st.is_charging;
}

// Best slot for the auto-battery / disconnect-icon substitution. A rotating group is
// opaque (only a PLAIN weather slot is a preferred target).
static int replaceable_slot(const WidgetSlot *slots, int count) {
  for (int i = 0; i < count; i++) {
    if (slots[i].count == 1 &&
        (slots[i].members[0] == WEATHER_CURRENT || slots[i].members[0] == WEATHER_FORECAST_TODAY)) {
      return i;
    }
  }
  return (count > 0) ? (count - 1) : 0;
}

static void column_heights(const Active *col, int n, int16_t *h) {
  for (int i = 0; i < n; i++) { h[i] = col[i].slot_h; }
}

static void set_fallback(int pos, Active *a, uint8_t id, int font) {
  a->ctx.id = id;
  a->ctx.font = (uint8_t)font;
  a->ctx.hide_icon = false;
  Widgets_get(id)->measure(&a->ctx, &a->size);
  a->slot_h = (int16_t)widget_height(&a->size);
  (void)pos;
}

// Auto-battery / BT-disconnect substitution into the edge columns (battery has
// priority over BT).
static void apply_fallback(Active col[2][MAX_WIDGET_SLOTS + 1], int n[2], int inner) {
  bool disconnected = settings.activateDisconnectIcon && !bluetooth_connection_service_peek();
  bool battery = auto_battery_shown();
  if (!battery && !disconnected) { return; }
  uint8_t fb = battery ? BATTERY_METER : BLUETOOTH_DISCONNECT;

  int16_t h[2][MAX_WIDGET_SLOTS + 1];
  int vis[2];
  for (int c = 0; c < 2; c++) {
    column_heights(col[c], n[c], h[c]);
    vis[c] = wl_column_visible(h[c], n[c], inner, WL_MIN_COLUMN_GAP);
  }

  if (settings.fallbackColumn == 0) {
    // AUTOMATIC: the left column hosts it if anything is visible there, else the right
    int c = vis[0] > 0 ? 0 : (vis[1] > 0 ? 1 : -1);
    if (c >= 0) {
      int idx = replaceable_slot(s_slots[c], vis[c]);
      set_fallback(c, &col[c][idx], fb, col[c][idx].ctx.font);
    }
    return;
  }
  // MANUAL: pinned to the chosen column at the configured position
  int c = settings.fallbackColumn - 1;
  Active probe;
  set_fallback(c, &probe, fb, WIDGET_FONT_DEFAULT_SIDE);
  int total = 0;
  for (int i = 0; i < n[c]; i++) { total += h[c][i]; }
  bool append_fits = vis[c] == n[c] && n[c] < MAX_WIDGET_SLOTS &&
                     total + probe.slot_h + n[c] * WL_MIN_COLUMN_GAP <= inner;
  int idx;
  bool append;
  WidgetList_fallbackPlace(n[c], vis[c], append_fits, settings.fallbackPosition, &idx, &append);
  if (append) {
    col[c][n[c]++] = probe;
  } else {
    set_fallback(c, &col[c][idx], fb, col[c][idx].ctx.font);
  }
}

// Place an item whose slot is slot_h high from slot_top. The value's ink is centred in
// the slot and the icon / unit on the value's ink; a widget without a value (icon
// only) is centred in the slot as it is.
static void place(const Active *a, int pos, int screen_w, int slot_top, int slot_h) {
  if (s_placed_count >= MAX_PLACED) { return; }
  WlHPlace hp;
  wl_place_h((WlPosition)pos, screen_w, CF_STRIP_W, a->size.value_w, a->size.icon_w, &hp);
  Placed *p = &s_placed[s_placed_count++];
  int ink_top = slot_top + wl_center_offset(slot_h, a->size.ink_h);
  int icon_top = a->size.ink_h > 0 ? ink_top + wl_center_offset(a->size.ink_h, a->size.icon_h)
                                   : slot_top + wl_center_offset(slot_h, a->size.icon_h);
  p->ctx = a->ctx;
  p->has_value = a->size.value_w > 0;
  p->has_icon = a->size.icon_w > 0;
  p->value_box = GRect(hp.value_x, ink_top, a->size.value_w, a->size.ink_h);
  p->icon_box = GRect(hp.icon_x, icon_top, a->size.icon_w, a->size.icon_h);
}

static void place_column(const Active *col, int n, int pos, GRect bounds) {
  int top = CF_STRIP_W + WL_EDGE_INSET, bottom = bounds.size.h - top;
  int16_t h[MAX_WIDGET_SLOTS + 1], ys[MAX_WIDGET_SLOTS + 1];
  column_heights(col, n, h);
  int shown = wl_column_visible(h, n, bottom - top, WL_MIN_COLUMN_GAP);
  if (n > 0 && shown == 0) { shown = 1; }
  wl_column_layout(h, n, top, bottom, WL_MIN_COLUMN_GAP, ys);
  for (int i = 0; i < shown; i++) {
    place(&col[i], pos, bounds.size.w, ys[i], col[i].slot_h);
  }
}

void WidgetLayer_prepare(GRect bounds) {
  s_placed_count = 0;
  int sod = seconds_of_day();
  int inner = bounds.size.h - 2 * (CF_STRIP_W + WL_EDGE_INSET);

  static Active col[2][MAX_WIDGET_SLOTS + 1];
  int n[2];
  for (int c = 0; c < 2; c++) {
    n[c] = s_slot_count[c];
    for (int i = 0; i < n[c]; i++) { resolve_slot(c, &s_slots[c][i], sod, &col[c][i]); }
  }
  apply_fallback(col, n, inner);
  place_column(col[0], n[0], WIDGET_POS_LEFT, bounds);
  place_column(col[1], n[1], WIDGET_POS_RIGHT, bounds);

  int edge = CF_STRIP_W + WL_EDGE_INSET;
  for (int pos = WIDGET_POS_TOP; pos <= WIDGET_POS_BOTTOM; pos++) {
    if (s_slot_count[pos] < 1) { continue; }
    Active a;
    resolve_slot(pos, &s_slots[pos][0], sod, &a);
    int top = (pos == WIDGET_POS_TOP) ? edge : bounds.size.h - edge - a.slot_h;
    place(&a, pos, bounds.size.w, top, a.slot_h);
  }
}

void WidgetLayer_drawIcons(GContext *ctx) {
  for (int i = 0; i < s_placed_count; i++) {
    const Placed *p = &s_placed[i];
    if (p->has_icon) { Widgets_get(p->ctx.id)->draw_icon(ctx, &p->ctx, p->icon_box); }
  }
}

void WidgetLayer_drawValues(GContext *ctx) {
  for (int i = 0; i < s_placed_count; i++) {
    const Placed *p = &s_placed[i];
    if (p->has_value) {
      Widgets_get(p->ctx.id)->draw_value(ctx, &p->ctx, p->value_box, GTextAlignmentLeft);
    }
  }
}

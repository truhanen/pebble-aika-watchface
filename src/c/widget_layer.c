#include "widget_layer.h"
#include "widgets.h"
#include "settings.h"
#include "widget_list.h"
#include "widget_fonts.h"
#include "widget_layout_calc.h"
#include "widget_text.h"
#include "shadow_skip_calc.h"
#include "clock_face_calc.h"
#include "test_hooks.h"

// One slot as shown right now: the active member, its size, and the height the slot
// reserves (the tallest member, so the column does not reflow as a group rotates).
typedef struct {
  WidgetCtx ctx;
  WidgetSize size;
  int16_t slot_h;
  WlItemDims dims;   // of the active member
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
static SsScene s_scene;   // this frame's hands and widget boxes, for skipping invisible shadows

void WidgetLayer_init(void) {
  Widgets_init();
  WidgetLayer_reload();
}

void WidgetLayer_deinit(void) { Widgets_deinit(); }

void WidgetLayer_reload(void) {
  for (int pos = 0; pos < WIDGET_POS_COUNT; pos++) {
    int n;
    const uint8_t *list = Settings_widgetList(pos, &n);
    s_slot_count[pos] = WidgetList_parse(list, n, s_slots[pos], MAX_WIDGET_SLOTS);
  }
}

static WlItemDims dims_of(const WidgetCtx *c, const WidgetSize *s) {
  WlItemDims d = { c->font, s->value_w, s->ink_h, s->icon_w, s->icon_h, s->unit_inline,
                   s->value_desc, s->unit_desc };
  return d;
}

// Height a widget reserves: everything it draws, descenders included.
static int widget_height(const WidgetCtx *c, const WidgetSize *s) {
  WlItemDims d = dims_of(c, s);
  return wl_item_height(&d);
}

// The font of a member: the test-hook override, else the one in its flags, else the
// position's default (side widgets Gothic medium, centre Bitham large).
static int font_for(int pos, uint8_t flags) {
  int forced = TestHooks_font(pos);
  if (forced >= 0) { return forced; }
  int f = WidgetList_flagFont(flags);
  if (f != WL_FONT_POS_DEFAULT) { return f; }
  return pos == WIDGET_POS_CENTER ? WIDGET_FONT_DEFAULT_CENTER : WIDGET_FONT_DEFAULT_SIDE;
}

static void measure_member(int pos, const WidgetSlot *slot, int member, WidgetCtx *ctx, WidgetSize *size) {
  ctx->id = slot->members[member];
  ctx->font = (uint8_t)font_for(pos, slot->flags[member]);
  ctx->ident = (uint8_t)WidgetList_flagIdent(slot->flags[member]);
  Widgets_get(ctx->id)->measure(ctx, size);
}

static void resolve_slot(int pos, const WidgetSlot *slot, int sod, Active *a) {
  int active = WidgetSlot_activeIndex(slot, sod);
  a->slot_h = 0;
  for (int m = 0; m < slot->count; m++) {
    WidgetCtx c;
    WidgetSize s;
    measure_member(pos, slot, m, &c, &s);
    if (m == active) { a->ctx = c; a->size = s; a->dims = dims_of(&c, &s); }
    if (widget_height(&c, &s) > a->slot_h) { a->slot_h = (int16_t)widget_height(&c, &s); }
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
  a->ctx.ident = WL_IDENT_ICON;
  Widgets_get(id)->measure(&a->ctx, &a->size);
  a->dims = dims_of(&a->ctx, &a->size);
  a->slot_h = (int16_t)widget_height(&a->ctx, &a->size);
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

// Place an item whose slot is slot_h high from slot_top (wl_place_v keeps everything it
// draws, descenders included, inside the slot; a widget without a value is centred as it is).
static void place(const Active *a, int pos, int screen_w, int slot_top, int slot_h) {
  if (s_placed_count >= MAX_PLACED) { return; }
  WlHPlace hp;
  wl_place_h((WlPosition)pos, screen_w, CF_STRIP_W, a->size.value_w, a->size.icon_w, &hp);
  WlVPlace vp;
  wl_place_v(&a->dims, slot_h, &vp);
  Placed *p = &s_placed[s_placed_count++];
  p->ctx = a->ctx;
  p->has_value = a->size.value_w > 0;
  p->has_icon = a->size.icon_w > 0;
  p->value_box = GRect(hp.value_x, slot_top + vp.ink_top, a->size.value_w, a->size.ink_h);
  p->icon_box = GRect(hp.icon_x, slot_top + vp.icon_top, a->size.icon_w, a->size.icon_h);
  // The slot rect (everything the widget draws, see wl_place_v) is what other shadows must not touch.
  SsItem *it = &s_scene.item[s_placed_count - 1];
  int x0 = 1000, x1 = -1000;
  if (p->has_value) { x0 = p->value_box.origin.x; x1 = x0 + p->value_box.size.w; }
  if (p->has_icon) {
    if (p->icon_box.origin.x < x0) { x0 = p->icon_box.origin.x; }
    if (p->icon_box.origin.x + p->icon_box.size.w > x1) { x1 = p->icon_box.origin.x + p->icon_box.size.w; }
  }
  it->used = p->has_value || p->has_icon;
  it->has_icon = p->has_icon;
  it->icon = ss_rect(p->icon_box.origin.x, p->icon_box.origin.y, p->icon_box.size.w, p->icon_box.size.h);
  it->slot = ss_rect(x0, slot_top, x1 - x0, slot_h);
  s_scene.n_items = s_placed_count;
}

static void place_column(const Active *col, int n, int pos, GRect bounds) {
  int top, bottom;
  wl_column_bounds(bounds.size.h, CF_STRIP_W, &top, &bottom);
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
  s_scene.n_items = 0;
  s_scene.has_hands = false;
  int sod = seconds_of_day();
  int ctop, cbottom;
  wl_column_bounds(bounds.size.h, CF_STRIP_W, &ctop, &cbottom);
  int inner = cbottom - ctop;

  static Active col[WIDGET_POS_COUNT][MAX_WIDGET_SLOTS + 1];
  int n[WIDGET_POS_COUNT];
  for (int c = 0; c < WIDGET_POS_COUNT; c++) {
    n[c] = s_slot_count[c];
    for (int i = 0; i < n[c]; i++) { resolve_slot(c, &s_slots[c][i], sod, &col[c][i]); }
  }
  apply_fallback(col, n, inner);
  place_column(col[0], n[0], WIDGET_POS_LEFT, bounds);
  place_column(col[1], n[1], WIDGET_POS_RIGHT, bounds);

  place_column(col[2], n[2], WIDGET_POS_CENTER, bounds);
}

void WidgetLayer_drawIcons(GContext *ctx) {
  for (int i = 0; i < s_placed_count; i++) {
    const Placed *p = &s_placed[i];
    if (p->has_icon && !Widgets_iconOnTop(p->ctx.id)) { Widgets_get(p->ctx.id)->draw_icon(ctx, &p->ctx, p->icon_box); }
  }
}

void WidgetLayer_drawValues(GContext *ctx, const CfHandPose *hour, const CfHandPose *minute, GPoint pivot) {
  s_scene.hand[0] = *hour;
  s_scene.hand[1] = *minute;
  s_scene.cx = pivot.x;
  s_scene.cy = pivot.y;
  s_scene.has_hands = true;
  for (int i = 0; i < s_placed_count; i++) {
    const Placed *p = &s_placed[i];
    WidgetText_setShadowScene(&s_scene, i);
    if (p->has_value) {
      Widgets_get(p->ctx.id)->draw_value(ctx, &p->ctx, p->value_box, GTextAlignmentLeft);
    }
    if (p->has_icon && Widgets_iconOnTop(p->ctx.id)) {   // icon-only: above the hands, shadowed
      Widgets_get(p->ctx.id)->draw_icon(ctx, &p->ctx, p->icon_box);
    }
  }
  WidgetText_setShadowScene(NULL, 0);
}

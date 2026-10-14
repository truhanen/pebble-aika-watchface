// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include "twt_status.h"
#include "settings.h"
#include "theme.h"
#include "twt_calc.h"

TwtStatus twt_status;

static Layer* s_status_layer;

// Status-strip flash (target-reached alert): ~5s = 10 toggles x 500ms.
static AppTimer* s_flash_timer;
static int s_flash_toggles_left;
static bool s_flash_on;
#define TWT_FLASH_TOGGLES 10
#define TWT_FLASH_INTERVAL_MS 500

bool TwtStatus_isSupported() {
#if defined(PBL_RECT) && !defined(PBL_PLATFORM_APLITE)
  return true;
#else
  return false;
#endif
}

void TwtStatus_load() {
  bool versionMatches = persist_exists(TWT_STATUS_VERSION_PERSIST_KEY)
      && persist_read_int(TWT_STATUS_VERSION_PERSIST_KEY) == TWT_STATUS_VERSION;
  if (versionMatches && persist_exists(TWT_STATUS_PERSIST_KEY)) {
    persist_read_data(TWT_STATUS_PERSIST_KEY, &twt_status, sizeof(TwtStatus));
  } else {
    // no data, or a blob from a different struct layout -> start clean
    twt_status = (TwtStatus){0};
    twt_status.taskName[0] = '\0';
#ifdef SCREENSHOT_FIXTURES
#if SCREENSHOT_SCENE == 2
    // Deterministic demo data for the TWT-active appstore scene (no phone needed).
    twt_status.isTracking = true;
    strncpy(twt_status.taskName, "Client work", TWT_TASK_NAME_LEN);
    twt_status.taskName[TWT_TASK_NAME_LEN] = '\0';
    twt_status.workedBeforeMin = 372;      // 6:12 worked today
    twt_status.taskWorkedBeforeMin = 145;  // 2:25 on this task today
    twt_status.taskTotalBeforeMin = 145;
    twt_status.dailyTargetMin = 450;       // 7:30 target -> ~83%, bar partly filled
    twt_status.taskBudgetMin = 0;
    twt_status.segmentStartEpoch = 0;      // no live delta -> deterministic
#endif
#endif
  }
}

void TwtStatus_save() {
  persist_write_int(TWT_STATUS_VERSION_PERSIST_KEY, TWT_STATUS_VERSION);
  persist_write_data(TWT_STATUS_PERSIST_KEY, &twt_status, sizeof(TwtStatus));
}

// Line 1: the day total (big & bold) with its percent of the configured workday target in
// parentheses just after it, spanning the whole strip width. A thin bar below shows workday
// completion (only when a target is set). Line 2: the task name (left, truncated) with the
// current-task time right-aligned to the edge (no task percent — TWT Control shows those per
// task). The task time is on line 2 (not crammed onto line 1) so a legible font fits without
// clipping on 144 px. Both times add the running segment live (it belongs to the current
// task); the day percent is hidden when its base is 0. When settings.twtShowRemaining is set
// (and a target exists), the big day-total number instead shows remaining = target - worked
// (overtime renders as "+h:mm" worked over target); the percent and the bar always reflect
// worked progress, regardless of that toggle.
int32_t TwtStatus_workedTotalMin(void) {
  int32_t running = 0;
  if (twt_status.isTracking && twt_status.segmentStartEpoch > 0) {
    running = ((int32_t)time(NULL) - twt_status.segmentStartEpoch) / 60;
    if (running < 0) running = 0;
  }
  return twt_status.workedBeforeMin + running;
}

int32_t TwtStatus_taskTotalMin(void) {
  int32_t running = 0;
  if (twt_status.isTracking && twt_status.segmentStartEpoch > 0) {
    running = ((int32_t)time(NULL) - twt_status.segmentStartEpoch) / 60;
    if (running < 0) running = 0;
  }
  return twt_status.taskTotalBeforeMin + running;
}

// Memoized graphics_text_layout_get_content_size. Text measurement is one of the more
// expensive graphics calls, and the strip does three of them on EVERY frame -- while a
// sub-minute rotating group repaints the whole window several times a minute -- for
// strings that change at most once a minute (they are whole minutes and a percent).
//
// `slot` is the CALL SITE, not a hash: each of the three measurements keeps its own
// entry, so the once-a-minute string change refreshes that entry in place instead of
// evicting the others (a shared table would thrash, since every string changes together).
static GSize twt_measure_cached(int slot, const char *text, GFont font, int w, int h) {
  enum { TWT_MEASURE_SLOTS = 3 };
  static char keyText[TWT_MEASURE_SLOTS][12];   // matches the caller buffers
  static GFont keyFont[TWT_MEASURE_SLOTS];
  static int16_t keyW[TWT_MEASURE_SLOTS], keyH[TWT_MEASURE_SLOTS];
  static GSize value[TWT_MEASURE_SLOTS];
  static bool valid[TWT_MEASURE_SLOTS];

  if (slot < 0 || slot >= TWT_MEASURE_SLOTS) {   // defensive: never cache an unknown slot
    return graphics_text_layout_get_content_size(text, font, GRect(0, 0, w, h),
                                                 GTextOverflowModeFill, GTextAlignmentLeft);
  }
  if (valid[slot] && keyFont[slot] == font && keyW[slot] == (int16_t)w
      && keyH[slot] == (int16_t)h
      && strncmp(keyText[slot], text, sizeof(keyText[slot])) == 0) {
    return value[slot];
  }
  value[slot] = graphics_text_layout_get_content_size(text, font, GRect(0, 0, w, h),
                                                      GTextOverflowModeFill,
                                                      GTextAlignmentLeft);
  strncpy(keyText[slot], text, sizeof(keyText[slot]) - 1);
  keyText[slot][sizeof(keyText[slot]) - 1] = '\0';
  keyFont[slot] = font;
  keyW[slot] = (int16_t)w;
  keyH[slot] = (int16_t)h;
  valid[slot] = true;
  return value[slot];
}

static void status_update_proc(Layer* layer, GContext* ctx) {
  GRect b = layer_get_bounds(layer);

  // During a flash "on" tick, fill the whole strip with the bright flash colour and
  // draw text/bar in a contrasting colour, so the alert is unmistakable. Otherwise the
  // configurable strip background (GColorClear = inherit watchface bg, no fill).
  GColor text_color = theme.timeColor;
  if (s_flash_on) {
    graphics_context_set_fill_color(ctx, theme.twtFlashColor);
    graphics_fill_rect(ctx, b, 0, GCornerNone);
    // luminance of the flash colour -> black text on light, white text on dark
    unsigned r = theme.twtFlashColor.r, g = theme.twtFlashColor.g, bl = theme.twtFlashColor.b;
    text_color = ((r * 3 + g * 6 + bl) >= 15) ? GColorBlack : GColorWhite;  // (3+6+1)*1.5 threshold on 0..3 channels
  } else if (!gcolor_equal(theme.twtStatusBgColor, GColorClear)) {
    graphics_context_set_fill_color(ctx, theme.twtStatusBgColor);
    graphics_fill_rect(ctx, b, 0, GCornerNone);
  }

  int32_t total = TwtStatus_workedTotalMin();
  int32_t running = total - twt_status.workedBeforeMin;            // live current segment, whole minutes
  int32_t task_today = twt_status.taskWorkedBeforeMin + running;   // today's task time
  int32_t task_total = twt_status.taskTotalBeforeMin + running;    // all-time task time

  int day_pct = twt_percent(total, twt_status.dailyTargetMin);     // -1 -> hide
  // budgeted: all-time total; unbudgeted: today's time
  int32_t task_shown = (twt_status.taskBudgetMin > 0) ? task_total : task_today;

  char total_buf[12];
  char task_buf[12];
  char day_pct_buf[12] = "";
  if (settings.twtShowRemaining && twt_status.dailyTargetMin > 0) {
    // remaining = target - worked; overtime renders as "+h:mm" (worked over target)
    twt_fmt_hhmm_remaining(total_buf, sizeof(total_buf), twt_status.dailyTargetMin - total);
  } else {
    twt_fmt_hhmm_signed(total_buf, sizeof(total_buf), total);  // worked time
  }
  snprintf(task_buf, sizeof(task_buf), "%d:%02d", (int)(task_shown / 60), (int)(task_shown % 60));
  if (day_pct >= 0)  snprintf(day_pct_buf, sizeof(day_pct_buf), "(%d%%)", day_pct > 999 ? 999 : day_pct);

  graphics_context_set_text_color(ctx, text_color);

  GFont big = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont small = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  // --- Line 1: day total (big) + day percent (small) after it, centred together on the strip. ---
  // The day percent (worked / daily target) is the headline metric and gets line 1 to itself;
  // the task time + percent move to line 2 so a legible font fits without clipping on 144 px.
  GSize tot_sz = twt_measure_cached(0, total_buf, big, b.size.w, 30);
  int pct_gap_w = 0;
  if (day_pct_buf[0]) {
    GSize pct_sz = twt_measure_cached(1, day_pct_buf, small, b.size.w, 22);
    pct_gap_w = 4 + pct_sz.w;
  }
  int line1_x = (b.size.w - (tot_sz.w + pct_gap_w)) / 2;
  if (line1_x < 0) line1_x = 0;
  graphics_draw_text(ctx, total_buf, big,
      GRect(line1_x, -2, b.size.w - line1_x, 30),
      GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  if (day_pct_buf[0]) {
    int px = line1_x + tot_sz.w + 4;
    graphics_draw_text(ctx, day_pct_buf, small,
        GRect(px, 6, b.size.w - px, 22),
        GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  }

  // --- Thin workday-completion bar (only when a target exists) ---
  if (twt_status.dailyTargetMin > 0) {
    int bar_y = 28, bar_h = 3, bar_w = b.size.w;
    int fill = twt_bar_fill_px(total, twt_status.dailyTargetMin, bar_w);
    graphics_context_set_stroke_color(ctx, text_color);
    graphics_draw_rect(ctx, GRect(0, bar_y, bar_w, bar_h));
    graphics_context_set_fill_color(ctx, text_color);
    graphics_fill_rect(ctx, GRect(0, bar_y, fill, bar_h), 0, GCornerNone);
  }

  // --- Line 2: task name (left, truncated) + task time (right-aligned). ---
  GSize tl = twt_measure_cached(2, task_buf, small, b.size.w, 22);
  int task_x = b.size.w - tl.w;
  if (task_x < 0) task_x = 0;
  graphics_draw_text(ctx, task_buf, small,
      GRect(task_x, 40, b.size.w - task_x, 22),
      GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, twt_status.taskName, big,
      GRect(0, 32, task_x - 2, 30),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void flash_timer_cb(void* data) {
  s_flash_timer = NULL;
  if (s_flash_toggles_left <= 0) {
    s_flash_on = false;
    if (s_status_layer) layer_mark_dirty(s_status_layer);
    return;
  }
  s_flash_toggles_left--;
  s_flash_on = !s_flash_on;
  if (s_status_layer) layer_mark_dirty(s_status_layer);
  s_flash_timer = app_timer_register(TWT_FLASH_INTERVAL_MS, flash_timer_cb, NULL);
}

void TwtStatus_startFlash(void) {
  if (!s_status_layer) return;
  if (s_flash_timer) { app_timer_cancel(s_flash_timer); s_flash_timer = NULL; }
  s_flash_toggles_left = TWT_FLASH_TOGGLES;
  s_flash_on = true;
  layer_mark_dirty(s_status_layer);
  s_flash_timer = app_timer_register(TWT_FLASH_INTERVAL_MS, flash_timer_cb, NULL);
}

void TwtStatus_redraw() {
  if (s_status_layer) layer_mark_dirty(s_status_layer);
}

void TwtStatus_setFrame(GRect frame) {
  if (s_status_layer) layer_set_frame(s_status_layer, frame);
}

void TwtStatus_initLayer(Layer* parent, GRect frame) {
  if (!TwtStatus_isSupported()) return;
  s_status_layer = layer_create(frame);
  layer_set_update_proc(s_status_layer, status_update_proc);
  layer_add_child(parent, s_status_layer);
  layer_set_hidden(s_status_layer, true); // shown by main.c only while tracking
}

void TwtStatus_setHidden(bool hidden) {
  if (s_status_layer) layer_set_hidden(s_status_layer, hidden);
}

void TwtStatus_deinitLayer() {
  if (s_flash_timer) { app_timer_cancel(s_flash_timer); s_flash_timer = NULL; }
  s_flash_on = false;
  if (s_status_layer) {
    layer_destroy(s_status_layer);
    s_status_layer = NULL;
  }
}

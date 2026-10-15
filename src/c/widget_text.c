#include "widget_text.h"
#include "widget_fonts.h"
#include "widget_layout_calc.h"
#include <string.h>

#define MAX_RUNS 6
#define UNIT_PART_LEN 16   // room for a whole part of any unit label (a data-slot label is up to 11 chars)

static int advance(const char *text, int len, int font) {
  char buf[40];
  if (len >= (int)sizeof(buf)) { len = (int)sizeof(buf) - 1; }
  memcpy(buf, text, len);
  buf[len] = '\0';
  return graphics_text_layout_get_content_size(buf, WidgetFonts_get((uint8_t)font),
      GRect(0, 0, 400, 100), GTextOverflowModeFill, GTextAlignmentLeft).w;
}

static void run_font(const WidgetFontMetrics *m, int font, const WlRun *r, int *out_font) {
  *out_font = r->fallback ? m->fallback : font;
}

int WidgetText_width(const char *text, int font) {
  const WidgetFontMetrics *m = wl_metrics(font);
  WlRun runs[MAX_RUNS];
  int n = wl_split_runs(m, text, runs, MAX_RUNS), w = 0;
  for (int i = 0; i < n; i++) {
    int f;
    run_font(m, font, &runs[i], &f);
    w += advance(text + runs[i].start, runs[i].len, f);
  }
  return w;
}

int WidgetText_inkHeight(int font) { return wl_metrics(font)->digit_h; }

// A string split into runs, each with its font, text, width and box top measured once.
typedef struct {
  int n;
  struct { char buf[40]; GFont font; uint8_t font_idx; int w, y, h; } run[MAX_RUNS];
} PreparedText;

static void prepare(PreparedText *pt, const char *text, int font, int baseline) {
  const WidgetFontMetrics *m = wl_metrics(font);
  WlRun runs[MAX_RUNS];
  pt->n = wl_split_runs(m, text, runs, MAX_RUNS);
  int primary_top = baseline - m->base;
  for (int i = 0; i < pt->n; i++) {
    int f;
    run_font(m, font, &runs[i], &f);
    int len = runs[i].len < (int)sizeof(pt->run[i].buf) - 1 ? runs[i].len : (int)sizeof(pt->run[i].buf) - 1;
    memcpy(pt->run[i].buf, text + runs[i].start, len);
    pt->run[i].buf[len] = '\0';
    pt->run[i].font = WidgetFonts_get((uint8_t)f);
    pt->run[i].font_idx = (uint8_t)f;
    pt->run[i].w = advance(pt->run[i].buf, len, f);
    pt->run[i].y = wl_run_box_top(m, wl_metrics(f), primary_top);
    pt->run[i].h = wl_metrics(f)->base + 16;
  }
}

static void draw_run(GContext *ctx, const PreparedText *pt, int i, int x, int dy) {
  // box wider than the text so the Fill overflow mode never wraps it
  graphics_draw_text(ctx, pt->run[i].buf, pt->run[i].font, GRect(x, pt->run[i].y + dy, pt->run[i].w + 16, pt->run[i].h),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

static void draw_prepared(GContext *ctx, const PreparedText *pt, int x, int dy) {
  for (int i = 0; i < pt->n; i++) {
    draw_run(ctx, pt, i, x, dy);
    x += pt->run[i].w;
  }
}

void WidgetText_drawAtBaseline(GContext *ctx, const char *text, int font, int x, int baseline) {
  PreparedText pt;
  prepare(&pt, text, font, baseline);
  draw_prepared(ctx, &pt, x, 0);
}

void WidgetText_draw(GContext *ctx, const char *text, int font, int x, int ink_top) {
  const WidgetFontMetrics *m = wl_metrics(font);
  WidgetText_drawAtBaseline(ctx, text, font, x, ink_top - m->digit_top + m->base);
}

// Value shadow offsets {dx, dy} (y grows down), chosen per run font. Font 0 has 1 px
// strokes, so it gets a 1 px ring (the 8 neighbours). Thicker fonts need only the
// distance-2 ring, plus one left and one down to fill the gaps it leaves.
static const int8_t SHADOW_THIN[][2] = {
  { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 },
};
static const int8_t SHADOW_THICK[][2] = {
  { -2, 0 }, { 2, 0 }, { 0, -2 }, { 0, 2 }, { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 },
  { -1, 0 }, { 0, 1 },
};

// Measured once; the shadow passes and the value only move the boxes.
void WidgetText_drawValue(GContext *ctx, const char *text, int font, int x, int ink_top,
                          GColor fg, GColor bg) {
  const WidgetFontMetrics *m = wl_metrics(font);
  PreparedText pt;
  prepare(&pt, text, font, ink_top - m->digit_top + m->base);
  graphics_context_set_text_color(ctx, bg);
  int rx = x;
  for (int i = 0; i < pt.n; i++) {
    bool thin = pt.run[i].font_idx == WIDGET_FONT_UNIT;
    int count = thin ? (int)(sizeof(SHADOW_THIN) / sizeof(SHADOW_THIN[0]))
                     : (int)(sizeof(SHADOW_THICK) / sizeof(SHADOW_THICK[0]));
    const int8_t (*offs)[2] = thin ? SHADOW_THIN : SHADOW_THICK;
    for (int k = 0; k < count; k++) {
      draw_run(ctx, &pt, i, rx + offs[k][0], offs[k][1]);
    }
    rx += pt.run[i].w;
  }
  graphics_context_set_text_color(ctx, fg);
  draw_prepared(ctx, &pt, x, 0);
}

bool WidgetUnit_isInline(const char *label) {
  char num[UNIT_PART_LEN], den[UNIT_PART_LEN];
  return !wl_split_fraction(label, num, den, UNIT_PART_LEN);
}

void WidgetUnit_measure(const char *label, int *w, int *h) {
  char num[UNIT_PART_LEN], den[UNIT_PART_LEN];
  if (wl_split_fraction(label, num, den, UNIT_PART_LEN)) {
    int nw = WidgetText_width(num, WIDGET_FONT_UNIT), dw = WidgetText_width(den, WIDGET_FONT_UNIT);
    WlFraction fr;
    wl_fraction_layout(wl_metrics(WIDGET_FONT_UNIT), num, &fr);
    *w = nw > dw ? nw : dw;
    *h = fr.h;
  } else {
    *w = WidgetText_width(label, WIDGET_FONT_UNIT);
    *h = WidgetText_inkHeight(WIDGET_FONT_UNIT);
  }
}

void WidgetUnit_draw(GContext *ctx, const char *label, GRect box, GColor fg) {
  char num[UNIT_PART_LEN], den[UNIT_PART_LEN];
  graphics_context_set_text_color(ctx, fg);
  if (wl_split_fraction(label, num, den, UNIT_PART_LEN)) {
    WlFraction fr;
    wl_fraction_layout(wl_metrics(WIDGET_FONT_UNIT), num, &fr);
    int nw = WidgetText_width(num, WIDGET_FONT_UNIT), dw = WidgetText_width(den, WIDGET_FONT_UNIT);
    WidgetText_drawAtBaseline(ctx, num, WIDGET_FONT_UNIT, box.origin.x + (box.size.w - nw) / 2,
                              box.origin.y + fr.num_base);
    WidgetText_drawAtBaseline(ctx, den, WIDGET_FONT_UNIT, box.origin.x + (box.size.w - dw) / 2,
                              box.origin.y + fr.h);
    graphics_context_set_stroke_color(ctx, fg);
    graphics_context_set_stroke_width(ctx, 1);
    int y = box.origin.y + fr.rule_y;
    graphics_draw_line(ctx, GPoint(box.origin.x, y), GPoint(box.origin.x + box.size.w - 1, y));
  } else {
    int lw = WidgetText_width(label, WIDGET_FONT_UNIT);
    WidgetText_draw(ctx, label, WIDGET_FONT_UNIT, box.origin.x + (box.size.w - lw) / 2, box.origin.y);
  }
}

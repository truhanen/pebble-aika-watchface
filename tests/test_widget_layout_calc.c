// Host tests for src/c/widget_layout_calc.c (font runs, edge columns, placement).
//   gcc -std=c11 -Wall -I src/c tests/test_widget_layout_calc.c src/c/widget_layout_calc.c
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "widget_layout_calc.h"

#define W 200
#define STRIP 4

static int has_all(const WidgetFontMetrics *m, const char *chars) {
  for (const char *p = chars; *p; p++) {
    if (!wl_font_has(m, (uint8_t)*p)) { return 0; }
  }
  return 1;
}

static void test_metrics_table(void) {
  for (int f = 0; f < WL_FONT_COUNT; f++) {
    const WidgetFontMetrics *m = wl_metrics(f);
    assert(m->digit_h > 0 && m->cap_h > 0);
    assert(m->digit_top + m->digit_h <= m->base + 1);   // digits sit on the baseline
    assert(has_all(m, "0123456789"));
    const WidgetFontMetrics *fb = wl_metrics(m->fallback);
    // the fallback really has every glyph this font lacks
    for (const char *p = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz%.,:-/+@()'*#!?=&;_<>"; *p; p++) {
      if (!wl_font_has(m, (uint8_t)*p)) { assert(wl_font_has(fb, (uint8_t)*p)); }
    }
    if (m->fallback != f) { assert(fb->fallback != 99); }
  }
  assert(wl_metrics(-1) == wl_metrics(0));
  assert(wl_metrics(99) == wl_metrics(0));
}

static void test_cp_and_coverage(void) {
  int n;
  assert(wl_next_cp("A", &n) == 'A' && n == 1);
  assert(wl_next_cp("\xc2\xb0", &n) == 0xB0 && n == 2);     // degree sign
  assert(wl_next_cp("", &n) == 0 && n == 0);
  assert(wl_next_cp("\xc2", &n) == 0xC2 && n == 1);          // stray lead byte
  const WidgetFontMetrics *leco60 = wl_metrics(15);
  assert(wl_font_has(leco60, '7'));
  assert(wl_font_has(leco60, ' '));
  assert(!wl_font_has(leco60, '%'));                          // measured: Leco 60 lacks %
  assert(!wl_font_has(leco60, 'M'));
  assert(wl_font_has(wl_metrics(5), 0xB0));                   // Gothic has the degree sign
}

static void test_runs(void) {
  WlRun r[4];
  const WidgetFontMetrics *leco60 = wl_metrics(15);
  assert(wl_split_runs(leco60, "87", r, 4) == 1 && !r[0].fallback && r[0].len == 2);
  assert(wl_split_runs(leco60, "87%", r, 4) == 2);
  assert(r[0].start == 0 && r[0].len == 2 && !r[0].fallback);
  assert(r[1].start == 2 && r[1].len == 1 && r[1].fallback);
  assert(wl_split_runs(leco60, "5% 7", r, 4) == 3);           // the space joins the % run
  assert(r[1].fallback && r[1].len == 2 && !r[2].fallback && r[2].start == 3);
  assert(wl_split_runs(leco60, "", r, 4) == 0);
  assert(wl_split_runs(leco60, "1%2%3%", r, 2) == 2);         // overflow merges into the last
  assert(r[1].start + r[1].len == 6);
  assert(wl_split_runs(wl_metrics(5), "-3\xc2\xb0", r, 4) == 1);   // all in Gothic
}

static void test_vertical(void) {
  const WidgetFontMetrics *p = wl_metrics(15), *f = wl_metrics(p->fallback);
  assert(wl_box_top_for_ink(p, 20) + p->digit_top == 20);
  // a fallback run shares the primary's baseline
  int primary_top = 30;
  int run_top = wl_run_box_top(p, f, primary_top);
  assert(run_top + f->base == primary_top + p->base);
  assert(wl_center_offset(20, 10) == 5);
  assert(wl_center_offset(21, 10) == 5);
  assert(wl_center_offset(10, 21) == -6);                     // floor of -5.5
  assert(wl_center_offset(10, 10) == 0);
}

static void test_icon_fit(void) {
  // a 25 px view whose ink spans raw 4..188 (1/8 px), 2 px stroke, to WL_ICON_H
  WlIconScale s = wl_icon_scale(184, 8, 2, WL_ICON_H);
  assert(s.num == (WL_ICON_H - 2) * 8 && s.den == 184);
  assert(wl_icon_coord(4, 4, 8, s) == 8);                              // min lands on the half stroke
  assert(wl_icon_coord(188, 4, 8, s) == 8 + (WL_ICON_H - 2) * 8);      // max lands on the far side
  assert((8 + (WL_ICON_H - 2) * 8 + 8 + 7) / 8 == WL_ICON_H);        // so the ink is WL_ICON_H high
  assert(wl_icon_width_px(184, 8, 2, s) == WL_ICON_H);                 // square extent -> square box
  WlIconScale half = wl_icon_scale(92, 8, 2, WL_ICON_H);
  assert(wl_icon_width_px(92, 8, 2, half) == WL_ICON_H);
  assert(wl_icon_scale(0, 8, 2, WL_ICON_H).num == 1);                  // degenerate: identity
  assert(wl_icon_coord(10, 10, 8, wl_icon_scale(0, 8, 2, WL_ICON_H)) == 8);
}

static void test_columns(void) {
  int16_t h[4] = { 18, 18, 18, 18 }, ys[4];
  // space-between: first at top, last ends at bottom
  wl_column_layout(h, 4, 6, 222, 5, ys);
  assert(ys[0] == 6 && ys[3] + 18 == 222);
  int gap0 = ys[1] - ys[0] - 18, gap2 = ys[3] - ys[2] - 18;
  assert(gap0 == gap2 && gap0 == (216 - 72) / 3);
  // single widget is centred
  wl_column_layout(h, 1, 6, 222, 5, ys);
  assert(ys[0] == 6 + (216 - 18) / 2);
  // no room: min gap from the top, and visible count honours it
  int16_t big[3] = { 90, 90, 90 };
  wl_column_layout(big, 3, 6, 222, 5, ys);
  assert(ys[0] == 6 && ys[1] == 101 && ys[2] == 196);
  assert(wl_column_visible(big, 3, 216, 5) == 2);
  assert(wl_column_visible(h, 4, 216, 5) == 4);
  assert(wl_column_visible(h, 4, 17, 5) == 0);
  wl_column_layout(h, 0, 6, 222, 5, ys);                      // n == 0 is a no-op
}

static void test_horizontal(void) {
  WlHPlace p;
  int edge = STRIP + WL_EDGE_INSET;
  wl_place_h(WL_LEFT, W, STRIP, 30, 12, &p);
  assert(p.value_x == edge && p.icon_x == edge + 30 + WL_ICON_GAP);
  wl_place_h(WL_RIGHT, W, STRIP, 30, 12, &p);
  assert(p.value_x + 30 == W - edge);
  assert(p.icon_x + 12 + WL_ICON_GAP == p.value_x);          // icon towards the centre
  wl_place_h(WL_TOP, W, STRIP, 40, 20, &p);
  int total = 40 + WL_ICON_GAP + 20;
  assert(p.value_x == (W - total) / 2 && p.icon_x == p.value_x + 40 + WL_ICON_GAP);
  wl_place_h(WL_BOTTOM, W, STRIP, 40, 0, &p);                 // no icon: no gap
  assert(p.value_x == (W - 40) / 2);
  wl_place_h(WL_LEFT, W, STRIP, 0, 12, &p);                   // icon only
  assert(p.icon_x == edge);
  wl_place_h(WL_RIGHT, W, STRIP, 0, 12, &p);
  assert(p.icon_x == W - edge - 12);
}

static void test_fraction(void) {
  char n[8], d[8];
  assert(wl_split_fraction("BTC/USD", n, d, 8) && !strcmp(n, "BTC") && !strcmp(d, "USD"));
  assert(wl_split_fraction("c/kWh", n, d, 8) && !strcmp(n, "c") && !strcmp(d, "kWh"));
  assert(!wl_split_fraction("BTC", n, d, 8) && !strcmp(n, "BTC") && !d[0]);
  assert(!wl_split_fraction("BTC/", n, d, 8) && !strcmp(n, "BTC/"));
  assert(!wl_split_fraction("/USD", n, d, 8) && !strcmp(n, "/USD"));
  assert(wl_split_fraction("ABCDEFGHIJ/KLMNOPQR", n, d, 4) && !strcmp(n, "ABC") && !strcmp(d, "KLM"));
  // the buffers widget_text.c uses (16) hold every whole part of a unit label: a data
  // slot label (7 chars), the cheap-electricity "-12.5c/kWh", nothing is cut
  { char n16[16], d16[16];
    assert(wl_split_fraction("BTC/USD", n16, d16, 16) && !strcmp(n16, "BTC") && !strcmp(d16, "USD"));
    assert(wl_split_fraction("-12.5c/kWh", n16, d16, 16) && !strcmp(n16, "-12.5c") && !strcmp(d16, "kWh"));
    assert(wl_split_fraction("ABCDE/FGHIJKL", n16, d16, 16) && !strcmp(n16, "ABCDE") && !strcmp(d16, "FGHIJKL")); }
  const WidgetFontMetrics *u = wl_metrics(0);
  WlFraction fr;
  wl_fraction_layout(u, &fr);
  assert(fr.num_base == u->cap_h);
  assert(fr.rule_y == fr.num_base + WL_FRACTION_GAP);
  assert(fr.den_top == fr.rule_y + 1 + WL_FRACTION_GAP);
  assert(fr.h == fr.den_top + u->cap_h);
}

int main(void) {
  test_metrics_table();
  test_cp_and_coverage();
  test_runs();
  test_vertical();
  test_icon_fit();
  test_columns();
  test_horizontal();
  test_fraction();
  puts("widget_layout_calc tests passed");
  return 0;
}

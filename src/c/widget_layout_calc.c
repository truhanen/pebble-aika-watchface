#include "widget_layout_calc.h"

const WidgetFontMetrics *wl_metrics(int font) {
  if (font < 0 || font >= WL_FONT_COUNT) { font = 0; }
  return &WIDGET_FONT_METRICS[font];
}

uint32_t wl_next_cp(const char *s, int *len) {
  const uint8_t *p = (const uint8_t *)s;
  if (p[0] == 0) { *len = 0; return 0; }
  if (p[0] < 0x80) { *len = 1; return p[0]; }
  if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
    *len = 2;
    return ((uint32_t)(p[0] & 0x1F) << 6) | (p[1] & 0x3F);
  }
  if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
    *len = 3;
    return ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
  }
  *len = 1;
  return p[0];   // stray byte: treated as its own code point
}

bool wl_font_has(const WidgetFontMetrics *m, uint32_t cp) {
  if (cp <= 0x20) { return true; }
  const char *c = m->chars;
  int n;
  uint32_t have;
  while ((have = wl_next_cp(c, &n)) != 0) {
    if (have == cp) { return true; }
    c += n;
  }
  return false;
}

int wl_split_runs(const WidgetFontMetrics *m, const char *text, WlRun *runs, int max_runs) {
  int count = 0, pos = 0;
  while (text[pos]) {
    int n;
    uint32_t cp = wl_next_cp(text + pos, &n);
    bool fallback = !wl_font_has(m, cp);
    if (cp == ' ' && count > 0) { fallback = runs[count - 1].fallback; }
    if (count > 0 && runs[count - 1].fallback == fallback) {
      runs[count - 1].len += n;
    } else if (count < max_runs) {
      runs[count].start = (int16_t)pos;
      runs[count].len = (int16_t)n;
      runs[count].fallback = fallback;
      count++;
    } else if (count > 0) {
      runs[count - 1].len += n;
    }
    pos += n;
  }
  return count;
}

int wl_box_top_for_ink(const WidgetFontMetrics *m, int ink_top) {
  return ink_top - m->digit_top;
}

int wl_run_box_top(const WidgetFontMetrics *primary, const WidgetFontMetrics *run_font,
                   int primary_box_top) {
  return primary_box_top + primary->base - run_font->base;
}

int wl_inline_unit_ink_top(const WidgetFontMetrics *value, int value_ink_top,
                           const WidgetFontMetrics *unit) {
  int value_box_top = wl_box_top_for_ink(value, value_ink_top);
  int unit_box_top = value_box_top + value->base - unit->base;
  return unit_box_top + unit->digit_top;
}

int wl_column_visible(const int16_t *heights, int n, int inner, int min_gap) {
  int y = 0, visible = 0;
  for (int i = 0; i < n; i++) {
    if (y + heights[i] > inner) { break; }
    visible++;
    y += heights[i] + min_gap;
  }
  return visible;
}

void wl_column_layout(const int16_t *heights, int n, int top, int bottom, int min_gap,
                      int16_t *ys) {
  if (n <= 0) { return; }
  int inner = bottom - top, total = 0;
  for (int i = 0; i < n; i++) { total += heights[i]; }
  if (n == 1) {
    ys[0] = (int16_t)(top + (inner - heights[0]) / 2);
    return;
  }
  if (total + (n - 1) * min_gap > inner) {
    int y = top;
    for (int i = 0; i < n; i++) {
      ys[i] = (int16_t)y;
      y += heights[i] + min_gap;
    }
    return;
  }
  // Spread the slack so the gaps differ by at most 1 px and the last item ends
  // exactly at bottom (a single floored gap left the remainder below it).
  int slack = inner - total, y = top;
  for (int i = 0; i < n; i++) {
    ys[i] = (int16_t)y;
    y += heights[i] + slack * (i + 1) / (n - 1) - slack * i / (n - 1);
  }
}

void wl_place_h(WlPosition pos, int screen_w, int strip_w, int value_w, int icon_w,
                WlHPlace *out) {
  int gap = (value_w > 0 && icon_w > 0) ? WL_ICON_GAP : 0;
  int edge = strip_w + WL_EDGE_INSET;
  switch (pos) {
    case WL_LEFT:
      out->value_x = (int16_t)edge;
      out->icon_x = (int16_t)(edge + value_w + gap);
      break;
    case WL_RIGHT:
      out->value_x = (int16_t)(screen_w - edge - value_w);
      out->icon_x = (int16_t)(out->value_x - gap - icon_w);
      break;
    default: {
      int x0 = (screen_w - (value_w + gap + icon_w)) / 2;
      out->value_x = (int16_t)x0;
      out->icon_x = (int16_t)(x0 + value_w + gap);
      break;
    }
  }
}

int wl_center_offset(int outer_h, int inner_h) {
  int d = outer_h - inner_h;
  return d >= 0 ? d / 2 : -((-d + 1) / 2);   // floor, also when inner is the larger
}

bool wl_split_fraction(const char *label, char *num, char *den, int cap) {
  const char *slash = label;
  while (*slash && *slash != '/') { slash++; }
  int n = 0;
  for (const char *p = label; p < slash && n < cap - 1; p++) { num[n++] = *p; }
  num[n] = '\0';
  den[0] = '\0';
  if (!*slash) { return false; }
  n = 0;
  for (const char *p = slash + 1; *p && n < cap - 1; p++) { den[n++] = *p; }
  den[n] = '\0';
  if (den[0] != '\0' && num[0] != '\0') { return true; }
  n = 0;
  for (const char *p = label; *p && n < cap - 1; p++) { num[n++] = *p; }
  num[n] = '\0';
  den[0] = '\0';
  return false;
}

void wl_fraction_layout(const WidgetFontMetrics *m, WlFraction *out) {
  int cap = m->cap_h > 0 ? m->cap_h : m->digit_h;
  out->num_base = (int16_t)cap;
  out->rule_y = (int16_t)(cap + WL_FRACTION_GAP);
  out->den_top = (int16_t)(out->rule_y + 1 + WL_FRACTION_GAP);
  out->h = (int16_t)(out->den_top + cap);
}

WlIconScale wl_icon_scale(int raw_extent, int raw_per_px, int stroke, int target_px) {
  WlIconScale s = { 1, 1 };
  if (raw_extent <= 0 || target_px <= stroke) { return s; }
  s.num = (int32_t)(target_px - stroke) * raw_per_px;
  s.den = raw_extent;
  return s;
}

int wl_icon_coord(int v, int v_min, int origin, WlIconScale s) {
  int64_t d = (int64_t)(v - v_min) * s.num;
  return origin + (int)((d + (d >= 0 ? s.den / 2 : -s.den / 2)) / s.den);
}

int wl_icon_width_px(int raw_w, int raw_per_px, int stroke, WlIconScale s) {
  int64_t raw = (int64_t)raw_w * s.num / s.den;
  return (int)((raw + raw_per_px - 1) / raw_per_px) + stroke;
}

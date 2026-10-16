#include "shadow_skip_calc.h"

SsRect ss_rect(int x, int y, int w, int h) {
  SsRect r = { (int16_t)x, (int16_t)y, (int16_t)(x + w), (int16_t)(y + h) };
  return r;
}

static float fmaxf3(float a, float b, float c) { return a > b ? (a > c ? a : c) : (b > c ? b : c); }

// Squared distance from point to the closed rectangle (0 inside).
static float pt_rect_d2(float px, float py, float x0, float y0, float x1, float y1) {
  float dx = fmaxf3(x0 - px, 0.0f, px - x1), dy = fmaxf3(y0 - py, 0.0f, py - y1);
  return dx * dx + dy * dy;
}

static float pt_seg_d2(float px, float py, float ax, float ay, float bx, float by) {
  float vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
  float len2 = vx * vx + vy * vy, t = len2 > 0.0f ? (wx * vx + wy * vy) / len2 : 0.0f;
  if (t < 0.0f) { t = 0.0f; }
  if (t > 1.0f) { t = 1.0f; }
  float dx = wx - t * vx, dy = wy - t * vy;
  return dx * dx + dy * dy;
}

// True if the segment crosses the closed rectangle (Liang-Barsky clip).
static bool seg_hits_rect(float ax, float ay, float bx, float by, float x0, float y0, float x1, float y1) {
  float t0 = 0.0f, t1 = 1.0f;
  float d[2] = { bx - ax, by - ay }, lo[2] = { x0 - ax, y0 - ay }, hi[2] = { x1 - ax, y1 - ay };
  for (int i = 0; i < 2; i++) {
    if (d[i] == 0.0f) {
      if (lo[i] > 0.0f || hi[i] < 0.0f) { return false; }
      continue;
    }
    float ta = lo[i] / d[i], tb = hi[i] / d[i];
    if (ta > tb) { float t = ta; ta = tb; tb = t; }
    if (ta > t0) { t0 = ta; }
    if (tb < t1) { t1 = tb; }
    if (t0 > t1) { return false; }
  }
  return true;
}

// Squared distance from the segment to the rectangle is <= d2 limit.
static bool seg_rect_within(float ax, float ay, float bx, float by, float x0, float y0,
                            float x1, float y1, float lim2) {
  if (seg_hits_rect(ax, ay, bx, by, x0, y0, x1, y1)) { return true; }
  if (pt_rect_d2(ax, ay, x0, y0, x1, y1) <= lim2 || pt_rect_d2(bx, by, x0, y0, x1, y1) <= lim2) { return true; }
  float cx[4] = { x0, x1, x1, x0 }, cy[4] = { y0, y0, y1, y1 };
  for (int i = 0; i < 4; i++) {
    if (pt_seg_d2(cx[i], cy[i], ax, ay, bx, by) <= lim2) { return true; }
  }
  return false;
}

bool ss_rect_near_hand(const CfHandPose *h, SsRect r) {
  float x0 = (float)r.x0 - SS_MARGIN, y0 = (float)r.y0 - SS_MARGIN;
  float x1 = (float)r.x1 + SS_MARGIN, y1 = (float)r.y1 + SS_MARGIN;
  float px[5] = { h->inner_left.x, h->inner_right.x, h->outer_right.x, h->apex.x, h->outer_left.x };
  float py[5] = { h->inner_left.y, h->inner_right.y, h->outer_right.y, h->apex.y, h->outer_left.y };
  float lim = (float)SS_HAND_REACH, lim2 = lim * lim;
  // bounding box reject
  float bx0 = px[0], bx1 = px[0], by0 = py[0], by1 = py[0];
  for (int i = 1; i < 5; i++) {
    if (px[i] < bx0) { bx0 = px[i]; }
    if (px[i] > bx1) { bx1 = px[i]; }
    if (py[i] < by0) { by0 = py[i]; }
    if (py[i] > by1) { by1 = py[i]; }
  }
  if (x1 < bx0 - lim || x0 > bx1 + lim || y1 < by0 - lim || y0 > by1 + lim) { return false; }
  // the rect inside the polygon: a corner is enough (convex, either winding)
  float cx[4] = { x0, x1, x1, x0 }, cy[4] = { y0, y0, y1, y1 };
  for (int c = 0; c < 4; c++) {
    bool pos = false, neg = false;
    for (int i = 0; i < 5; i++) {
      int j = (i + 1) % 5;
      float cr = (px[j] - px[i]) * (cy[c] - py[i]) - (py[j] - py[i]) * (cx[c] - px[i]);
      if (cr > 0.0f) { pos = true; } else if (cr < 0.0f) { neg = true; }
    }
    if (!(pos && neg)) { return true; }
  }
  for (int i = 0; i < 5; i++) {
    int j = (i + 1) % 5;
    if (seg_rect_within(px[i], py[i], px[j], py[j], x0, y0, x1, y1, lim2)) { return true; }
  }
  return false;
}

static bool overlap(SsRect a, SsRect b, int grow) {
  return a.x0 < b.x1 + grow && b.x0 - grow < a.x1 && a.y0 < b.y1 + grow && b.y0 - grow < a.y1;
}

bool ss_needs_shadow(const SsScene *s, SsRect ink, int reach, int self, bool check_own_icon) {
  SsRect fp = { (int16_t)(ink.x0 - reach), (int16_t)(ink.y0 - reach),
                (int16_t)(ink.x1 + reach), (int16_t)(ink.y1 + reach) };
  if (s->has_hands) {
    if (ss_rect_near_hand(&s->hand[0], fp) || ss_rect_near_hand(&s->hand[1], fp)) { return true; }
    float lim = (float)CF_PIVOT_R;
    if (pt_rect_d2(s->cx, s->cy, (float)fp.x0 - SS_MARGIN, (float)fp.y0 - SS_MARGIN,
                   (float)fp.x1 + SS_MARGIN, (float)fp.y1 + SS_MARGIN) <= lim * lim) { return true; }
  }
  for (int i = 0; i < s->n_items; i++) {
    const SsItem *it = &s->item[i];
    if (!it->used) { continue; }
    if (i == self) {
      if (check_own_icon && it->has_icon && overlap(fp, it->icon, 0)) { return true; }
    } else if (overlap(fp, it->slot, SS_MARGIN)) {
      return true;
    }
  }
  return false;
}

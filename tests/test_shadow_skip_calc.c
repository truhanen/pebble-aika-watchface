// Host tests for src/c/shadow_skip_calc.c (when a widget shadow can change a pixel).
// gcc -std=c11 -Wall -I src/c tests/test_shadow_skip_calc.c src/c/shadow_skip_calc.c src/c/clock_face_calc.c -lm
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "shadow_skip_calc.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CX 100
#define CY 114

static CfHandPose pose(int shoulder, int width, int ext, double deg) {
  CfHandPose p;
  double a = deg * M_PI / 180.0;
  cf_hand_pose(CX, CY, (float)sin(a), (float)cos(a), shoulder, width, ext, CF_HAND_TAIL, &p);
  return p;
}

static SsScene scene(double hour_deg, double min_deg) {
  SsScene s;
  memset(&s, 0, sizeof s);
  s.hand[0] = pose(CF_HOUR_SHOULDER, CF_HOUR_WIDTH, CF_HOUR_APEX_EXT, hour_deg);
  s.hand[1] = pose(CF_MINUTE_SHOULDER, CF_MINUTE_WIDTH, CF_MINUTE_APEX_EXT, min_deg);
  s.has_hands = true;
  s.cx = CX;
  s.cy = CY;
  return s;
}

// Brute force: paint both hands' polygons (dense sampling of the filled pentagon) and the
// halo disc of SS_HAND_REACH, report whether any painted point lies in the rect.
static int in_poly(const CfHandPose *h, double x, double y) {
  double px[5] = { h->inner_left.x, h->inner_right.x, h->outer_right.x, h->apex.x, h->outer_left.x };
  double py[5] = { h->inner_left.y, h->inner_right.y, h->outer_right.y, h->apex.y, h->outer_left.y };
  int pos = 0, neg = 0;
  for (int i = 0; i < 5; i++) {
    int j = (i + 1) % 5;
    double c = (px[j] - px[i]) * (y - py[i]) - (py[j] - py[i]) * (x - px[i]);
    if (c > 0) { pos = 1; } else if (c < 0) { neg = 1; }
  }
  return !(pos && neg);
}

static double dist_poly(const CfHandPose *h, double x, double y) {
  if (in_poly(h, x, y)) { return 0; }
  double px[5] = { h->inner_left.x, h->inner_right.x, h->outer_right.x, h->apex.x, h->outer_left.x };
  double py[5] = { h->inner_left.y, h->inner_right.y, h->outer_right.y, h->apex.y, h->outer_left.y };
  double best = 1e9;
  for (int i = 0; i < 5; i++) {
    int j = (i + 1) % 5;
    double vx = px[j] - px[i], vy = py[j] - py[i], wx = x - px[i], wy = y - py[i];
    double t = (wx * vx + wy * vy) / (vx * vx + vy * vy);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    double dx = wx - t * vx, dy = wy - t * vy, d = sqrt(dx * dx + dy * dy);
    if (d < best) { best = d; }
  }
  return best;
}

// Smallest distance from any pixel of the rect to the hand polygon (pixel-centre grid).
static double rect_dist(const CfHandPose *h, SsRect r) {
  double best = 1e9;
  for (int y = r.y0; y < r.y1; y++) {
    for (int x = r.x0; x < r.x1; x++) {
      double d = dist_poly(h, x, y);
      if (d < best) { best = d; }
    }
  }
  return best;
}

// Never a false "skip": whenever a pixel of the rect is within the stroke reach, it is needed.
static void test_no_false_negatives(void) {
  int checked = 0, hit = 0;
  for (int hd = 0; hd < 360; hd += 37) {
    for (int md = 0; md < 360; md += 6) {
      SsScene s = scene(hd, md);
      for (int y = 0; y < 228; y += 7) {
        for (int x = 0; x < 200; x += 7) {
          SsRect r = ss_rect(x, y, 13, 9);
          double d0 = rect_dist(&s.hand[0], r), d1 = rect_dist(&s.hand[1], r);
          bool near = d0 <= SS_HAND_REACH || d1 <= SS_HAND_REACH;
          bool got = ss_needs_shadow(&s, r, 0, -1, false);
          checked++;
          if (near) { assert(got); hit++; }
        }
      }
    }
  }
  printf("  no false skips over %d rects (%d near a hand)\n", checked, hit);
}

static void test_clear(void) {
  SsScene s = scene(0, 0);   // both hands point up from the pivot
  // far to the lower right, well away from both
  assert(!ss_needs_shadow(&s, ss_rect(150, 170, 30, 12), 2, -1, false));
  assert(!ss_rect_near_hand(&s.hand[1], ss_rect(150, 170, 30, 12)));
  // the hand's own tip region is hit
  assert(ss_needs_shadow(&s, ss_rect(95, 20, 10, 10), 2, -1, false));
  // pivot-adjacent
  assert(ss_needs_shadow(&s, ss_rect(CX - 2, CY - 2, 4, 4), 1, -1, false));
}

// Vertical minute hand at 0: polygon x in [CX-7, CX+7] (width 15) near the pivot; the stroke
// reaches SS_HAND_REACH beyond. A rect whose left edge sits exactly at the reach touches
// (true); the same rect SS_MARGIN+1 px farther is clear.
static void test_edge_and_margin(void) {
  CfHandPose h = pose(CF_MINUTE_SHOULDER, CF_MINUTE_WIDTH, CF_MINUTE_APEX_EXT, 0);
  int right = h.outer_right.x;   // straight section, x of the right side
  int y = 60;
  // closed rect x0 - margin == right + reach  -> touching
  int x_touch = right + SS_HAND_REACH + SS_MARGIN;
  assert(ss_rect_near_hand(&h, ss_rect(x_touch, y, 10, 10)));
  // one px beyond the safety margin: clear
  assert(!ss_rect_near_hand(&h, ss_rect(x_touch + 1, y, 10, 10)));
  // inside the safety margin band: still needed
  assert(ss_rect_near_hand(&h, ss_rect(right + SS_HAND_REACH + 1, y, 10, 10)));
  assert(ss_rect_near_hand(&h, ss_rect(right + 1, y, 10, 10)));
  // left side too
  int left = h.outer_left.x;
  assert(ss_rect_near_hand(&h, ss_rect(left - SS_HAND_REACH - SS_MARGIN - 10, y, 10, 10)));
  assert(!ss_rect_near_hand(&h, ss_rect(left - SS_HAND_REACH - SS_MARGIN - 11, y, 10, 10)));
}

static void test_rect_inside_polygon(void) {
  CfHandPose h = pose(CF_HOUR_SHOULDER, CF_HOUR_WIDTH, CF_HOUR_APEX_EXT, 90);
  // small rect fully inside the filled hour hand pointing right
  assert(ss_rect_near_hand(&h, ss_rect(CX + 20, CY - 2, 4, 4)));
  // rect much larger than the hand, containing it
  assert(ss_rect_near_hand(&h, ss_rect(0, 0, 200, 228)));
}

static void test_rotations(void) {
  for (int deg = 0; deg < 360; deg += 15) {
    CfHandPose h = pose(CF_MINUTE_SHOULDER, CF_MINUTE_WIDTH, CF_MINUTE_APEX_EXT, deg);
    double a = deg * M_PI / 180.0;
    // a point on the axis 60 px out is on the hand; one 40 px to the side is clear
    int ax = (int)lround(CX + 60 * sin(a)), ay = (int)lround(CY - 60 * cos(a));
    assert(ss_rect_near_hand(&h, ss_rect(ax - 3, ay - 3, 6, 6)));
    int sx = (int)lround(CX + 60 * sin(a) + 40 * cos(a)), sy = (int)lround(CY - 60 * cos(a) + 40 * sin(a));
    assert(!ss_rect_near_hand(&h, ss_rect(sx - 3, sy - 3, 6, 6)));
  }
}

static void test_pivot(void) {
  // hands pointing up: a rect just below the pivot, beyond the 18 px tail plus reach, is
  // clear of the hands; only the pivot test is exercised by a rect on the pivot itself.
  SsScene s = scene(0, 0);
  assert(ss_needs_shadow(&s, ss_rect(CX, CY, 1, 1), 0, -1, false));
  // pivot-only scene: no hand near, the pivot circle (radius CF_PIVOT_R) decides
  SsScene p;
  memset(&p, 0, sizeof p);
  p.has_hands = true;
  p.cx = CX;
  p.cy = CY;
  // degenerate hands far away (all points at the origin)
  assert(!ss_needs_shadow(&p, ss_rect(CX + CF_PIVOT_R + SS_MARGIN + 1, CY, 5, 5), 0, -1, false));
  assert(ss_needs_shadow(&p, ss_rect(CX + CF_PIVOT_R + SS_MARGIN, CY, 5, 5), 0, -1, false));
  assert(ss_needs_shadow(&p, ss_rect(CX - 20, CY - 1, 20, 2), 0, -1, false));
}

static void test_items(void) {
  SsScene s;
  memset(&s, 0, sizeof s);
  s.n_items = 2;
  s.item[0].used = true;
  s.item[0].has_icon = true;
  s.item[0].icon = ss_rect(30, 50, 12, 12);
  s.item[0].slot = ss_rect(5, 45, 37, 25);
  s.item[1].used = true;
  s.item[1].slot = ss_rect(5, 80, 40, 20);
  // self value 3 px left of its own icon (the layout gap): reach 2 does not touch
  assert(!ss_needs_shadow(&s, ss_rect(10, 50, 18, 12), 2, 0, true));
  assert(ss_needs_shadow(&s, ss_rect(10, 50, 19, 12), 2, 0, true));
  // another item's slot (item 0 ends at y 70) grown by the margin: ink 5 px below is clear
  assert(!ss_needs_shadow(&s, ss_rect(5, 75, 10, 3), 2, 1, false));
  assert(ss_needs_shadow(&s, ss_rect(5, 72, 10, 3), 2, 1, false));
  assert(ss_needs_shadow(&s, ss_rect(5, 76, 10, 3), 2, 0, false));   // reaches item 1's slot at 80 (+margin)
  // no hands, no other items: nothing to show through
  SsScene e;
  memset(&e, 0, sizeof e);
  assert(!ss_needs_shadow(&e, ss_rect(10, 10, 20, 10), 2, 0, true));
}

int main(void) {
  test_clear();
  test_edge_and_margin();
  test_rect_inside_polygon();
  test_rotations();
  test_pivot();
  test_items();
  test_no_false_negatives();
  printf("shadow_skip_calc: all tests passed\n");
  return 0;
}

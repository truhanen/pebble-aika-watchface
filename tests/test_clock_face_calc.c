// Host tests for src/c/clock_face_calc.c (hand geometry + edge ticks).
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "clock_face_calc.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define W 200
#define H 228
#define CX 100
#define CY 114

static double rad_of_minute(double m) { return m * 6.0 * M_PI / 180.0; }

static CfHandPose minute_pose(int shoulder, double m) {
  CfHandPose p;
  double a = rad_of_minute(m);
  cf_hand_pose(CX, CY, (float)sin(a), (float)cos(a), shoulder, CF_MINUTE_WIDTH,
               CF_MINUTE_APEX_EXT, CF_HAND_TAIL, &p);
  return p;
}

static int offscreen(CfPoint p) {
  int m = CF_OFFSCREEN_MARGIN;
  return p.x <= -m || p.x >= W - 1 + m || p.y <= -m || p.y >= H - 1 + m;
}

static int shoulders_offscreen(int shoulder) {
  for (int m = 0; m < 60; m++) {
    CfHandPose p = minute_pose(shoulder, m);
    if (!offscreen(p.outer_left) || !offscreen(p.outer_right)) { return 0; }
  }
  return 1;
}

static void test_snap(void) {
  assert(cf_snap(92.5f) == 93);
  assert(cf_snap(92.49f) == 92);
  assert(cf_snap(-7.5f) == -8);
  assert(cf_snap(0.0f) == 0);
}

// Nyquist's emery poses at 12 o'clock (pivot-relative), reproduced exactly
// when the pivot is at integer screen coordinates.
static void test_nyquist_twelve_oclock(void) {
  CfHandPose p;
  cf_hand_pose(CX, CY, 0.0f, 1.0f, 86, CF_MINUTE_WIDTH, CF_MINUTE_APEX_EXT, CF_HAND_TAIL, &p);
  assert(p.inner_left.x - CX == -7 && p.inner_left.y - CY == 18);
  assert(p.inner_right.x - CX == 8 && p.inner_right.y - CY == 18);
  assert(p.outer_left.x - CX == -7 && p.outer_left.y - CY == -86);
  assert(p.outer_right.x - CX == 8 && p.outer_right.y - CY == -86);
  assert(p.apex.x - CX == 0 && p.apex.y - CY == -92);

  cf_hand_pose(CX, CY, 0.0f, 1.0f, CF_HOUR_SHOULDER, CF_HOUR_WIDTH, CF_HOUR_APEX_EXT, CF_HAND_TAIL, &p);
  assert(p.inner_left.x - CX == -12 && p.inner_right.x - CX == 13);
  assert(p.outer_left.y - CY == -54 && p.apex.y - CY == -64);
}

// Nyquist minute pose 1 (minute 1): spot-check a rotated pose.
static void test_nyquist_minute_one(void) {
  CfHandPose p = minute_pose(86, 1);
  assert(p.outer_left.x - CX == 2 && p.outer_left.y - CY == -86);
  assert(p.outer_right.x - CX == 16 && p.outer_right.y - CY == -85);
  assert(p.apex.x - CX == 10 && p.apex.y - CY == -91);
  assert(p.inner_left.x - CX == -9 && p.inner_left.y - CY == 17);
}

static void test_minute_shoulder_is_minimal_offscreen(void) {
  assert(shoulders_offscreen(CF_MINUTE_SHOULDER));
  assert(!shoulders_offscreen(CF_MINUTE_SHOULDER - 1));
}

static int min_edge_dist(CfPoint p) {
  int d = p.x;
  if (W - 1 - p.x < d) { d = W - 1 - p.x; }
  if (p.y < d) { d = p.y; }
  if (H - 1 - p.y < d) { d = H - 1 - p.y; }
  return d;
}

static int depth_of(int b) { return b % 5 == 0 ? CF_STRIP_W : CF_MINOR_TICK_DEPTH; }

// Minor ticks sit CF_MINOR_TICK_INSET px in from the edge; 5-minute ticks start at it.
static int inset_of(int b) { return b % 5 == 0 ? 0 : CF_MINOR_TICK_INSET; }

static int tick_at(int r, int b, int depth, CfPoint out[CF_TICK_MAX_PIXELS]) {
  double a = rad_of_minute(b);
  float hx, hy;
  cf_boundary_hit(W, H, r, (float)sin(a), (float)cos(a), &hx, &hy);
  return cf_tick_pixels(W, H, r, hx, hy, depth, inset_of(b), out);
}

static int tick_of(int b, CfPoint out[CF_TICK_MAX_PIXELS]) {
  return tick_at(CF_CORNER_R, b, depth_of(b), out);
}

// ---- independent float model of the rounded rectangle ----------------------

// Inset distance of the pixel centre from the rounded boundary (negative: outside).
static double inset_f(int r, int x, int y) {
  double px = x + 0.5, py = y + 0.5;
  double ax = px < r ? r : (px > W - r ? W - r : px);
  double ay = py < r ? r : (py > H - r ? H - r : py);
  if (r > 0 && (px < r || px > W - r) && (py < r || py > H - r)) {
    return r - hypot(px - ax, py - ay);
  }
  double m = px;
  if (W - px < m) { m = W - px; }
  if (py < m) { m = py; }
  if (H - py < m) { m = H - py; }
  return m;
}

// ---- reference for r = 0: the previous (screen-rectangle) rule, brute force ----

// Doubled perimeter position -> doubled point on the rectangle of outer-ring centres.
static void ref_perimeter_point(int P, int *X, int *Y) {
  int A = 2 * (W - 1), B = 2 * (H - 1), len = 2 * A + 2 * B;
  P = ((P % len) + len) % len;
  if (P < A)  { *X = 1 + P; *Y = 1; return; }
  P -= A;
  if (P < B)  { *X = 2 * W - 1; *Y = 1 + P; return; }
  P -= B;
  if (P < A)  { *X = 2 * W - 1 - P; *Y = 2 * H - 1; return; }
  P -= A;
  *X = 1; *Y = 2 * H - 1 - P;
}

// Independent double-precision ray / rectangle [0,W]x[0,H] intersection, as the
// clockwise ring index position (top x, right W-1 + y, bottom W+H-2 + (W - x),
// left 2W+H-3 + (H - y)).
static double perimeter_of_boundary(int b) {
  double a = rad_of_minute(b);
  double dx = sin(a), dy = -cos(a), best = 1e30;
  double ts[4] = { dy < 0 ? CY / -dy : 1e30, dx > 0 ? (W - CX) / dx : 1e30,
                   dy > 0 ? (H - CY) / dy : 1e30, dx < 0 ? CX / -dx : 1e30 };
  int edge = 0;
  for (int e = 0; e < 4; e++) { if (ts[e] < best) { best = ts[e]; edge = e; } }
  double x = CX + dx * best, y = CY + dy * best;
  switch (edge) {
    case 0:  return x;
    case 1:  return W - 1 + y;
    case 2:  return W + H - 2 + (W - x);
    default: return 2 * W + H - 3 + (H - y);
  }
}

static int g_ref[H][W];

// Fills g_ref with the tick's pixels (1) under the old rule; returns the count.
static int ref_tick(int b) {
  int o = (int)floor(perimeter_of_boundary(b) + 0.001), depth = depth_of(b), n = 0;
  int x1, y1, x2, y2;
  ref_perimeter_point(2 * (o - 2) - 1, &x1, &y1);
  ref_perimeter_point(2 * (o + 2) + 1, &x2, &y2);
  x1 -= W; y1 -= H; x2 -= W; y2 -= H;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      g_ref[y][x] = 0;
      if (min_edge_dist((CfPoint){ (int16_t)x, (int16_t)y }) >= depth) { continue; }
      int vx = 2 * x + 1 - W, vy = 2 * y + 1 - H;
      if (x1 * vy - y1 * vx >= 0 && vx * y2 - vy * x2 >= 0) { g_ref[y][x] = 1; n++; }
    }
  }
  return n;
}

static int g_owner[H][W];

static void test_band(void) {
  // cf_in_band against the float model, every pixel, several radii and depths.
  for (int r = 0; r <= 20; r++) {
    for (int depth = 1; depth <= CF_STRIP_W; depth++) {
      for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
          double in = inset_f(r, x, y);
          assert(cf_in_band(W, H, r, x, y, depth) == (in >= 0 && in < depth));
        }
      }
    }
  }
  assert(!cf_in_band(W, H, 10, 0, 0, 4) && !cf_in_band(W, H, 10, 1, 1, 4));   // hidden corner
  assert(cf_in_band(W, H, 10, 50, 0, 1) && cf_in_band(W, H, 10, 0, 50, 1));
  assert(!cf_in_band(W, H, 10, 50, 1, 1) && cf_in_band(W, H, 10, 50, 3, 4) && !cf_in_band(W, H, 10, 50, 4, 4));
}

// R = 0 reproduces the previous screen-rectangle ticks and shadow exactly.
static void test_radius_zero_is_old_geometry(void) {
  static CfPoint all[CF_TICK_COUNT * CF_TICK_MAX_PIXELS];
  int total = 0;
  for (int b = 0; b < CF_TICK_COUNT; b++) {
    CfPoint px[CF_TICK_MAX_PIXELS];
    int n = tick_at(0, b, depth_of(b), px);
    if (inset_of(b) == 0) {   // 5-minute ticks: exactly the old rule (minor ticks moved inward, see below)
      int nref = ref_tick(b);
      assert(n == nref);
      for (int i = 0; i < n; i++) { assert(g_ref[px[i].y][px[i].x] == 1); }
    } else {                  // minor ticks: only the inset row, 1 px deep
      for (int i = 0; i < n; i++) { assert(min_edge_dist(px[i]) == CF_MINOR_TICK_INSET); }
    }
    for (int i = 0; i < n; i++) { all[total++] = px[i]; }
  }
  // Exactly 5 outer pixels per tick, as before.
  for (int b = 0; b < CF_TICK_COUNT; b++) {
    CfPoint px[CF_TICK_MAX_PIXELS];
    int n = tick_at(0, b, depth_of(b), px), n0 = 0;
    for (int i = 0; i < n; i++) { n0 += min_edge_dist(px[i]) == inset_of(b); }
    assert(n0 == CF_TICK_LEN);
  }
  // Shadow: on-screen non-tick 4-neighbours (same rule; cf_shadow_pixels is radius-free).
  assert(total > 0);
}

// The tick's first row (the one nearest the edge): pixels at inset distance `inset` exactly.
static int outer_row(int r, int inset, const CfPoint *px, int n, CfPoint *row) {
  int k = 0;
  for (int i = 0; i < n; i++) {
    int in_band = cf_in_band(W, H, r, px[i].x, px[i].y, inset + 1);
    int in_gap = inset > 0 && cf_in_band(W, H, r, px[i].x, px[i].y, inset);
    if (in_band && !in_gap) { row[k++] = px[i]; }
  }
  return k;
}

// Is pixel (x,y) within `margin` of one of the four corner squares of side r?
static int near_corner(int r, int x, int y, int margin) {
  int dx = x < W / 2 ? x : W - 1 - x, dy = y < H / 2 ? y : H - 1 - y;
  return dx < r + margin && dy < r + margin;
}

// Index of the pixel's ring row: 0 outermost, 1 the next, ... (-1 beyond CF_STRIP_W rows or hidden).
static int ring_row(int r, int x, int y) {
  for (int d = 0; d < CF_STRIP_W; d++) { if (cf_in_band(W, H, r, x, y, d + 1)) { return d; } }
  return -1;
}

static void test_ticks_geometry_for_radius(int r, int verbose) {
  CfPoint row0[CF_TICK_MAX_PIXELS];
  int min_n = 1000, max_n = 0, min_outer = 1000, max_outer = 0, off_corner = 0, near_ticks = 0;
  double worst_centre = 0;
  for (int y = 0; y < H; y++) { for (int x = 0; x < W; x++) { g_owner[y][x] = 0; } }
  for (int b = 0; b < CF_TICK_COUNT; b++) {
    CfPoint px[CF_TICK_MAX_PIXELS], p0[CF_TICK_MAX_PIXELS];
    int depth = depth_of(b);
    int n = tick_at(r, b, depth, px);
    assert(n > 0 && n <= CF_TICK_MAX_PIXELS);
    if (n < min_n) { min_n = n; }
    if (n > max_n) { max_n = n; }
    for (int i = 0; i < n; i++) {
      double in = inset_f(r, px[i].x, px[i].y);
      assert(in >= 0);                                   // inside the rounded rect: never a hidden corner pixel
      assert(in >= inset_of(b) && in < inset_of(b) + depth);   // within the band, past the gap
      assert(g_owner[px[i].y][px[i].x] == 0);            // none shared between ticks
      g_owner[px[i].y][px[i].x] = b + 1;
    }
    int n0 = outer_row(r, inset_of(b), px, n, row0);
    if (n0 < min_outer) { min_outer = n0; }
    if (n0 > max_outer) { max_outer = n0; }
    // Outer row: about 5 px of arc, 8-connected, hugging the boundary.
    assert(n0 >= CF_TICK_LEN - 1 && n0 <= CF_TICK_LEN + 1);
    for (int i = 0; i < n0; i++) {
      int linked = n0 == 1;
      for (int j = 0; j < n0; j++) {
        linked |= i != j && abs(row0[i].x - row0[j].x) <= 1 && abs(row0[i].y - row0[j].y) <= 1;
      }
      assert(linked);
    }
    // Centre of the outer row vs the float ray intersection with the rounded boundary:
    // the row's centroid lies within 1 px (half a cell along, half a cell across).
    double a = rad_of_minute(b), dx = sin(a), dy = -cos(a);
    // (a minor tick is centred on the ray's exit from the area shrunk by its inset)
    int ins = inset_of(b), ri = r > ins ? r - ins : 0, Wi = W - 2 * ins, Hi = H - 2 * ins;
    double tlo = 0, thi = 400;   // bisect the exit of the ray from the rounded rect (float model)
    for (int it = 0; it < 60; it++) {
      double m = 0.5 * (tlo + thi), qx = CX + dx * m - ins, qy = CY + dy * m - ins;
      double ax = qx < ri ? ri : (qx > Wi - ri ? Wi - ri : qx), ay = qy < ri ? ri : (qy > Hi - ri ? Hi - ri : qy);
      int inside = qx >= 0 && qx <= Wi && qy >= 0 && qy <= Hi &&
                   ((ri == 0) || !((qx < ri || qx > Wi - ri) && (qy < ri || qy > Hi - ri)) ||
                    hypot(qx - ax, qy - ay) <= ri);
      if (inside) { tlo = m; } else { thi = m; }
    }
    double hx = CX + dx * tlo, hy = CY + dy * tlo, cxm = 0, cym = 0;   // global coords
    for (int i = 0; i < n0; i++) { cxm += row0[i].x + 0.5; cym += row0[i].y + 0.5; }
    cxm /= n0; cym /= n0;
    double dist = hypot(cxm - hx, cym - hy);
    if (dist > worst_centre) { worst_centre = dist; }
    assert(dist <= 1.0);
    // Off-corner ticks (every pixel of the r = 0 tick well clear of the corner squares)
    // are exactly the r = 0 ticks.
    int n_p0 = tick_at(0, b, depth, p0), clear = 1;
    for (int i = 0; i < n_p0; i++) { clear &= !near_corner(r, p0[i].x, p0[i].y, 3); }
    if (clear) {
      off_corner++;
      assert(n == n_p0);
      for (int i = 0; i < n; i++) {
        int found = 0;
        for (int j = 0; j < n_p0; j++) { found |= px[i].x == p0[j].x && px[i].y == p0[j].y; }
        assert(found);
      }
    } else {
      near_ticks++;
    }
  }
  // No touching: pixels of different ticks in the same ring row (outermost row, or the
  // inset row of the minor ticks) are never 8-neighbours.
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      if (!g_owner[y][x]) { continue; }
      int row = ring_row(r, x, y);
      for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
          int nx = x + dx, ny = y + dy;
          if (nx < 0 || ny < 0 || nx >= W || ny >= H || ring_row(r, nx, ny) != row) { continue; }
          assert(g_owner[ny][nx] == 0 || g_owner[ny][nx] == g_owner[y][x]);
        }
      }
    }
  }
  if (r == 0) { assert(min_outer == CF_TICK_LEN && max_outer == CF_TICK_LEN && near_ticks >= 4); }
  if (r > 0) { assert(off_corner >= 36); }
  if (verbose || r == 0 || r == 20) {
    printf("R=%2d: tick pixels %d..%d, outer row %d..%d, off-corner ticks %d, corner-near %d, "
           "worst centre offset %.2f px\n", r, min_n, max_n, min_outer, max_outer, off_corner,
           near_ticks, worst_centre);
  }
}

// Minor ticks: 1 px deep, entirely in the inset row, the outer row under them free of tick pixels
// of any tick (their shadow covers it); on the straights the row is exactly 5 px (checked at
// r = 0 above and for off-corner ticks at CF_CORNER_R here).
static void test_minor_inset(void) {
  assert(CF_MINOR_TICK_INSET == 1 && CF_MINOR_TICK_DEPTH == 1);
  int straight = 0;
  for (int b = 0; b < CF_TICK_COUNT; b++) {
    if (b % 5 == 0) { continue; }
    CfPoint px[CF_TICK_MAX_PIXELS];
    int n = tick_of(b, px), on_straight = 1;
    for (int i = 0; i < n; i++) {
      assert(inset_f(CF_CORNER_R, px[i].x, px[i].y) >= CF_MINOR_TICK_INSET);
      assert(!cf_in_band(W, H, CF_CORNER_R, px[i].x, px[i].y, CF_MINOR_TICK_INSET));   // gap row empty
      assert(cf_in_band(W, H, CF_CORNER_R, px[i].x, px[i].y, CF_MINOR_TICK_INSET + CF_MINOR_TICK_DEPTH));
      on_straight &= !near_corner(CF_CORNER_R, px[i].x, px[i].y, 3);
    }
    if (on_straight) { assert(n == CF_TICK_LEN); straight++; }
  }
  assert(straight >= 30);
}

static void test_ticks_geometry(void) {
  test_minor_inset();
  assert(CF_TICK_COUNT == 60 && CF_TICK_LEN == 5);
  for (int r = 0; r <= 20; r++) {
    if (r == 0 || r >= 1) { test_ticks_geometry_for_radius(r, r == CF_CORNER_R); }
  }
}

// Corner-near ticks at the configured radius: non-empty with a 5 px (+-1) outer run on the arc.
static void test_corner_ticks(void) {
  CfPoint px[CF_TICK_MAX_PIXELS], row[CF_TICK_MAX_PIXELS];
  int corner_minutes[8] = { 7, 8, 22, 23, 37, 38, 52, 53 }, seen = 0;
  for (int c = 0; c < 8; c++) {
    int b = corner_minutes[c], n = tick_of(b, px);
    int near = 0;
    for (int i = 0; i < n; i++) { near |= near_corner(CF_CORNER_R, px[i].x, px[i].y, 0); }
    if (!near) { continue; }
    seen++;
    int k = outer_row(CF_CORNER_R, inset_of(b), px, n, row);
    assert(n > 0 && k >= CF_TICK_LEN - 1 && k <= CF_TICK_LEN + 1);
    for (int i = 0; i < n; i++) { assert(inset_f(CF_CORNER_R, px[i].x, px[i].y) >= 0); }
  }
  assert(seen >= 4);
  // The four corner-most cells of the old ring (screen corners) are hidden: never drawn.
  for (int b = 0; b < CF_TICK_COUNT; b++) {
    int n = tick_of(b, px);
    for (int i = 0; i < n; i++) { assert(!(px[i].x == 0 && px[i].y == 0) && !(px[i].x == W - 1 && px[i].y == 0)); }
  }
}

static uint8_t g_scratch[2 * ((W * H + 7) / 8)];
static CfPoint g_shadow[CF_TICK_COUNT * CF_SHADOW_MAX_PER_TICK];

// The shadow is exactly the on-screen non-tick pixels within Manhattan distance
// CF_SHADOW_R of a tick pixel (the 12 diamond offsets).
static void shadow_check(int r) {
  static CfPoint all[CF_TICK_COUNT * CF_TICK_MAX_PIXELS];
  static int is_tick[H][W], is_shadow[H][W], expect[H][W];
  for (int y = 0; y < H; y++) { for (int x = 0; x < W; x++) { is_tick[y][x] = is_shadow[y][x] = expect[y][x] = 0; } }
  int n = 0;
  for (int b = 0; b < CF_TICK_COUNT; b++) { n += tick_at(r, b, depth_of(b), all + n); }
  for (int i = 0; i < n; i++) { is_tick[all[i].y][all[i].x] = 1; }
  for (int i = 0; i < n; i++) {
    for (int dy = -CF_SHADOW_R; dy <= CF_SHADOW_R; dy++) {
      for (int dx = -CF_SHADOW_R; dx <= CF_SHADOW_R; dx++) {
        int d = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        int x = all[i].x + dx, y = all[i].y + dy;
        if (d >= 1 && d <= CF_SHADOW_R && x >= 0 && y >= 0 && x < W && y < H && !is_tick[y][x]) {
          expect[y][x] = 1;
        }
      }
    }
  }
  int sn = cf_shadow_pixels(W, H, all, n, g_scratch, g_shadow, CF_TICK_COUNT * CF_SHADOW_MAX_PER_TICK);
  assert(sn > 0 && sn < CF_TICK_COUNT * CF_SHADOW_MAX_PER_TICK);
  for (int i = 0; i < sn; i++) {
    int x = g_shadow[i].x, y = g_shadow[i].y;
    assert(x >= 0 && y >= 0 && x < W && y < H);   // on screen
    assert(!is_tick[y][x]);                        // never overlaps a tick
    assert(!is_shadow[y][x]);                      // listed once
    is_shadow[y][x] = 1;
  }
  int expected = 0;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) { assert(is_shadow[y][x] == expect[y][x]); expected += expect[y][x]; }
  }
  assert(expected == sn);
  if (r == CF_CORNER_R) { printf("R=%d: %d tick pixels, %d shadow pixels\n", r, n, sn); }
}

static void test_shadow_offsets(void) {
  // exactly the diamond: every (dx,dy) with 1 <= |dx|+|dy| <= CF_SHADOW_R once
  int seen[2 * CF_SHADOW_R + 1][2 * CF_SHADOW_R + 1] = { { 0 } };
  for (int i = 0; i < CF_SHADOW_OFFSET_COUNT; i++) {
    int dx = CF_SHADOW_OFFSETS[i][0], dy = CF_SHADOW_OFFSETS[i][1];
    int d = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
    assert(d >= 1 && d <= CF_SHADOW_R);
    assert(++seen[dy + CF_SHADOW_R][dx + CF_SHADOW_R] == 1);
  }
  int expect = 0;
  for (int dy = -CF_SHADOW_R; dy <= CF_SHADOW_R; dy++) {
    for (int dx = -CF_SHADOW_R; dx <= CF_SHADOW_R; dx++) {
      int d = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
      if (d >= 1 && d <= CF_SHADOW_R) { expect++; }
    }
  }
  assert(expect == CF_SHADOW_OFFSET_COUNT);
}

static void test_shadow(void) {
  test_shadow_offsets();
  for (int r = 0; r <= 20; r++) { shadow_check(r); }
}

static void test_boundary_hit(void) {
  float x, y;
  for (int r = 0; r <= 20; r += 10) {   // the axes hit the straights, whatever the radius
    cf_boundary_hit(W, H, r, 0.0f, 1.0f, &x, &y);       // 12 o'clock
    assert(fabsf(y) < 1e-3f && fabsf(x - 100.0f) < 1e-3f);
    cf_boundary_hit(W, H, r, 1.0f, 0.0f, &x, &y);       // 3 o'clock
    assert(fabsf(x - 200.0f) < 1e-3f && fabsf(y - 114.0f) < 1e-3f);
    cf_boundary_hit(W, H, r, 0.0f, -1.0f, &x, &y);      // 6 o'clock
    assert(fabsf(y - 228.0f) < 1e-3f && fabsf(x - 100.0f) < 1e-3f);
    cf_boundary_hit(W, H, r, -1.0f, 0.0f, &x, &y);      // 9 o'clock
    assert(fabsf(x) < 1e-3f && fabsf(y - 114.0f) < 1e-3f);
  }
  // A ray through a corner square exits on the arc: distance r from the arc centre.
  for (int r = 1; r <= 20; r++) {
    for (int b = 0; b < 60; b++) {
      double a = rad_of_minute(b);
      cf_boundary_hit(W, H, r, (float)sin(a), (float)cos(a), &x, &y);
      assert(inset_f(r, (int)floor(x - 1e-3), (int)floor(y - 1e-3)) > -0.8);   // on / next to the boundary
      if ((x < r || x > W - r) && (y < r || y > H - r)) {
        double ax = x < r ? r : W - r, ay = y < r ? r : H - r;
        assert(fabs(hypot(x - ax, y - ay) - r) < 1e-2);
      }
    }
  }
}

static void test_tick_colour(void) {
  // Test that major tick detection is correct: b%5==0 → major, others → minor.
  int majors = 0;
  for (int b = 0; b < CF_TICK_COUNT; b++) {
    if (cf_tick_is_major(b)) {
      assert(b % 5 == 0);
      majors++;
    } else {
      assert(b % 5 != 0);
    }
  }
  assert(majors == 12);  // 12 major (five-minute) ticks: 0, 5, 10, ..., 55
}

// Moving clock centre: for every pivot y in 100..128 the rays from (W/2, y) give a tick
// table that is non-empty, within CF_TICK_MAX_PIXELS and with no pixel in two ticks; at
// the screen centre it equals the unparametrised cf_tick_pixels; the ticks at 12/6/3
// o'clock lie on the axes through the pivot.
static void test_moving_pivot(void) {
  static unsigned char seen[W * H];
  for (int cy = 100; cy <= 128; cy++) {
    for (int i = 0; i < W * H; i++) { seen[i] = 0; }
    for (int b = 0; b < 60; b++) {
      double a = rad_of_minute(b);
      float hx, hy;
      cf_boundary_hit_from(W, H, CF_CORNER_R, W / 2.0f, (float)cy, (float)sin(a), (float)cos(a), &hx, &hy);
      CfPoint px[CF_TICK_MAX_PIXELS];
      bool major = cf_tick_is_major(b);
      int depth = major ? CF_STRIP_W : CF_MINOR_TICK_DEPTH, inset = major ? 0 : CF_MINOR_TICK_INSET;
      int n = cf_tick_pixels_from(W, H, CF_CORNER_R, W / 2.0f, (float)cy, hx, hy, depth, inset, px);
      assert(n > 0 && n <= CF_TICK_MAX_PIXELS);
      for (int i = 0; i < n; i++) {
        assert(!seen[px[i].y * W + px[i].x]);
        seen[px[i].y * W + px[i].x] = 1;
      }
      if (b == 0) { assert(fabsf(hx - W / 2.0f) < 1e-3f && fabsf(hy) < 1e-3f); }
      if (b == 30) { assert(fabsf(hx - W / 2.0f) < 1e-3f && fabsf(hy - H) < 1e-3f); }
      if (b == 15) { assert(fabsf(hx - W) < 1e-3f && fabsf(hy - cy) < 1e-3f); }
      if (cy == CY) {
        float qx, qy;
        cf_boundary_hit(W, H, CF_CORNER_R, (float)sin(a), (float)cos(a), &qx, &qy);
        CfPoint q[CF_TICK_MAX_PIXELS];
        int m = cf_tick_pixels(W, H, CF_CORNER_R, qx, qy, depth, inset, q);
        assert(m == n);
        for (int i = 0; i < n; i++) { assert(q[i].x == px[i].x && q[i].y == px[i].y); }
      }
    }
  }
}

// The pivot profile: dwell at the top/bottom, linear glide while the minute hand points
// at the side edges, the prototype's values, and the same distance from the pivot to the
// edge the minute hand points at (W/2) at :00, :15, :30 and :45.
static void test_pivot_profile(void) {
  static const int want_first_half[31] = {
    100, 100, 100, 100, 100, 100, 100, 100,                       // 0..7
    101, 103, 105, 107, 108, 110, 112, 114, 116, 118, 120, 121, 123, 125, 127,   // 8..22
    128, 128, 128, 128, 128, 128, 128, 128 };                     // 23..30
  for (int m = 0; m <= 30; m++) {
    assert(cf_pivot_y(W, H, m) == want_first_half[m]);
    if (m > 0 && m < 30) { assert(cf_pivot_y(W, H, 60 - m) == want_first_half[m]); }   // mirror
  }
  assert(cf_pivot_y(W, H, 37) == 128 && cf_pivot_y(W, H, 38) == 127 && cf_pivot_y(W, H, 52) == 101);
  assert(cf_pivot_y(W, H, 53) == 100 && cf_pivot_y(W, H, 0) == 100 && cf_pivot_y(W, H, 60) == 100);
  for (int m = 8; m < 23; m++) { assert(cf_pivot_y(W, H, m) >= cf_pivot_y(W, H, m - 1)); }      // down
  for (int m = 38; m < 53; m++) { assert(cf_pivot_y(W, H, m) <= cf_pivot_y(W, H, m - 1)); }     // up
  // Distance from the pivot to the edge the minute hand points at.
  assert(cf_pivot_y(W, H, 0) == W / 2);                // :00 to the top edge: 100
  assert(H - cf_pivot_y(W, H, 30) == W / 2);           // :30 to the bottom edge: 100
  assert(W - W / 2 == W / 2);                          // :15 and :45 to the side edges: 100
  // The tick hit of every minute lies on its ray from the pivot (hands point at their ticks).
  for (int m = 0; m < 60; m++) {
    int cy = cf_pivot_y(W, H, m);
    double a = rad_of_minute(m);
    float hx, hy;
    cf_boundary_hit_from(W, H, CF_CORNER_R, W / 2.0f, (float)cy, (float)sin(a), (float)cos(a), &hx, &hy);
    assert(fabs((hx - W / 2.0) * cos(a) + (hy - cy) * sin(a)) < 1e-2);   // cross product = 0
  }
}

int main(void) {
  test_moving_pivot();
  test_pivot_profile();
  test_snap();
  test_nyquist_twelve_oclock();
  test_nyquist_minute_one();
  test_minute_shoulder_is_minimal_offscreen();
  test_boundary_hit();
  test_band();
  test_radius_zero_is_old_geometry();
  test_ticks_geometry();
  test_corner_ticks();
  test_shadow();
  test_tick_colour();
  printf("test_clock_face_calc: OK\n");
  return 0;
}

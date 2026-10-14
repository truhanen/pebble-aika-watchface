// Host tests for src/c/clock_face_calc.c (hand geometry + edge ticks).
#include <assert.h>
#include <math.h>
#include <stdio.h>
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

#define RING0 (2 * (W - 1) + 2 * (H - 1))

static int min_edge_dist(CfPoint p) {
  int d = p.x;
  if (W - 1 - p.x < d) { d = W - 1 - p.x; }
  if (p.y < d) { d = p.y; }
  if (H - 1 - p.y < d) { d = H - 1 - p.y; }
  return d;
}

// Ring index of an outermost pixel (clockwise from (0,0), each corner once).
static int ring0_index(CfPoint p) {
  if (p.y == 0)     { return p.x; }
  if (p.x == W - 1) { return W - 1 + p.y; }
  if (p.y == H - 1) { return W + H - 2 + (W - 1 - p.x); }
  return 2 * W + H - 3 + (H - 1 - p.y);
}

// Independent double-precision ray / rectangle [0,W]x[0,H] intersection, as the
// clockwise perimeter position in ring-index units (top t = x, right W-1 + y,
// bottom W+H-2 + (W - x), left 2W+H-3 + (H - y)).
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

static int tick_of(int b, CfPoint out[CF_TICK_MAX_PIXELS]) {
  double a = rad_of_minute(b);
  int edge;
  float t;
  cf_boundary_hit(W, H, (float)sin(a), (float)cos(a), &edge, &t);
  // 5-minute ticks are CF_STRIP_W deep; minor ticks are CF_MINOR_TICK_DEPTH deep.
  int depth = b % 5 == 0 ? CF_STRIP_W : CF_MINOR_TICK_DEPTH;
  return cf_tick_pixels(W, H, edge, t, depth, out);
}

static int g_owner[H][W];

// Float perimeter position (doubled-free, ring-index units) -> point on the
// rectangle of outer-ring pixel centres [0.5,W-0.5] x [0.5,H-0.5].
static void ring_point(double q, double *x, double *y) {
  double A = W - 1, B = H - 1, len = 2 * A + 2 * B;
  q = fmod(fmod(q, len) + len, len);
  if (q < A)      { *x = 0.5 + q; *y = 0.5; }
  else if (q < A + B)     { *x = W - 0.5; *y = 0.5 + q - A; }
  else if (q < 2 * A + B) { *x = W - 0.5 - (q - A - B); *y = H - 0.5; }
  else            { *x = 0.5; *y = H - 0.5 - (q - 2 * A - B); }
}

// Clockwise angle (radians, [0,2pi)) of the direction from the centre to (x,y).
static double cw_angle(double x, double y) {
  double a = atan2(x - CX, -(y - CY));
  return a < 0 ? a + 2 * M_PI : a;
}

// Float: is pixel (px,py) strictly between the rays to E1 and E2 (clockwise)?
// Returns -1/1 when clearly out/in, 0 when within 1e-9 rad of a ray.
static int in_sector(double a1, double a2, int px, int py) {
  double v = cw_angle(px + 0.5, py + 0.5);
  double span = fmod(a2 - a1 + 2 * M_PI, 2 * M_PI), off = fmod(v - a1 + 2 * M_PI, 2 * M_PI);
  if (fabs(off) < 1e-9 || fabs(off - span) < 1e-9) { return 0; }
  return off < span ? 1 : -1;
}

static void test_ticks_geometry(void) {
  assert(CF_TICK_COUNT == 60 && CF_TICK_LEN == 5);
  int corner_ticks = 0, min_n = 1000, max_n = 0;
  for (int b = 0; b < CF_TICK_COUNT; b++) {
    CfPoint px[CF_TICK_MAX_PIXELS];
    int n = tick_of(b, px);
    assert(n > 0 && n <= CF_TICK_MAX_PIXELS);
    if (n < min_n) { min_n = n; }
    if (n > max_n) { max_n = n; }
    int on_ring0[CF_TICK_MAX_PIXELS], n0 = 0;
    int depth = b % 5 == 0 ? CF_STRIP_W : CF_MINOR_TICK_DEPTH;
    for (int i = 0; i < n; i++) {
      assert(px[i].x >= 0 && px[i].x < W && px[i].y >= 0 && px[i].y < H);
      assert(min_edge_dist(px[i]) < depth);        // inside the tick's depth
      assert(g_owner[px[i].y][px[i].x] == 0);           // none shared
      g_owner[px[i].y][px[i].x] = b + 1;
      if (min_edge_dist(px[i]) == 0) { on_ring0[n0++] = ring0_index(px[i]); }
    }
    // Exactly 5 outer pixels: a run of consecutive ring cells.
    assert(n0 == CF_TICK_LEN);
    int first = -1;
    for (int i = 0; i < n0; i++) {
      int prev = (on_ring0[i] + RING0 - 1) % RING0, has_prev = 0;
      for (int j = 0; j < n0; j++) { has_prev |= on_ring0[j] == prev; }
      if (!has_prev) { assert(first < 0); first = on_ring0[i]; }
    }
    assert(first >= 0);
    for (int j = 0; j < CF_TICK_LEN; j++) {
      int idx = (first + j) % RING0, found = 0;
      for (int i = 0; i < n0; i++) { found |= on_ring0[i] == idx; }
      assert(found);
    }
    // Centre of the 5 cells [first, first + 5) vs the exact boundary point.
    double centre = first + CF_TICK_LEN / 2.0;
    double diff = fmod(centre - perimeter_of_boundary(b) + 1.5 * RING0, RING0) - RING0 / 2.0;
    assert(fabs(diff) <= 0.5 + 1e-3);
    // Float angular check, over every strip pixel: member iff between the end rays.
    double e1x, e1y, e2x, e2y;
    ring_point(first - 0.5, &e1x, &e1y);
    ring_point(first + CF_TICK_LEN - 0.5, &e2x, &e2y);
    double a1 = cw_angle(e1x, e1y), a2 = cw_angle(e2x, e2y);
    int max_check_depth = CF_STRIP_W;  // Only check up to max depth when scanning
    for (int y = 0; y < H; y++) {
      for (int x = 0; x < W; x++) {
        int edge_dist = min_edge_dist((CfPoint){ (int16_t)x, (int16_t)y });
        if (edge_dist >= max_check_depth) { continue; }
        // Skip pixels beyond this tick's depth (minor ticks only 1 px deep).
        if (!cf_tick_is_major(b) && edge_dist >= CF_MINOR_TICK_DEPTH) { continue; }
        int s = in_sector(a1, a2, x, y), member = g_owner[y][x] == b + 1;
        if (s != 0) { assert(member == (s > 0)); }
      }
    }
    // Does it wrap a corner: some pixel in a 4x4 corner square, plus pixels on both
    // adjoining edges outside it.
    int in_corner = 0, horiz = 0, vert = 0;
    for (int i = 0; i < n; i++) {
      int xe = px[i].x < CF_STRIP_W || px[i].x >= W - CF_STRIP_W;
      int ye = px[i].y < CF_STRIP_W || px[i].y >= H - CF_STRIP_W;
      in_corner |= xe && ye;
      horiz |= ye && !xe;
      vert |= xe && !ye;
    }
    if (in_corner && horiz && vert) { corner_ticks++; }
  }
  printf("tick pixel count range: %d..%d\n", min_n, max_n);
  (void)corner_ticks;
  // No two ticks touch on the outer row: >= 1 free pixel between neighbours.
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      if (!g_owner[y][x] || min_edge_dist((CfPoint){ (int16_t)x, (int16_t)y }) != 0) { continue; }
      for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
          int nx = x + dx, ny = y + dy;
          if (nx < 0 || ny < 0 || nx >= W || ny >= H) { continue; }
          if (min_edge_dist((CfPoint){ (int16_t)nx, (int16_t)ny }) != 0) { continue; }
          assert(g_owner[ny][nx] == 0 || g_owner[ny][nx] == g_owner[y][x]);
        }
      }
    }
  }
}

static int has_pixel(const CfPoint *p, int n, int x, int y) {
  for (int i = 0; i < n; i++) { if (p[i].x == x && p[i].y == y) { return 1; } }
  return 0;
}

// A tick centred on the top-right corner (cell k = 199, the corner pixel): outer ring
// cells 197..201 = top x=197,198, the corner, right y=1,2. Test with full depth (5-minute tick).
static void test_corner_wrap(void) {
  CfPoint px[CF_TICK_MAX_PIXELS];
  int n = cf_tick_pixels(W, H, CF_EDGE_TOP, 199.2f, CF_STRIP_W, px);
  assert(n <= CF_TICK_MAX_PIXELS);
  assert(has_pixel(px, n, 197, 0) && has_pixel(px, n, 198, 0) && has_pixel(px, n, 199, 0));
  assert(has_pixel(px, n, 199, 1) && has_pixel(px, n, 199, 2));
  assert(!has_pixel(px, n, 196, 0) && !has_pixel(px, n, 199, 3));
  // All four corners: pixels distinct, in the strip, outer row exactly 5.
  int bases[4][2] = { { CF_EDGE_RIGHT, H - 1 }, { CF_EDGE_BOTTOM, W - 1 }, { CF_EDGE_LEFT, H - 1 },
                      { CF_EDGE_TOP, 0 } };
  for (int c = 0; c < 4; c++) {
    n = cf_tick_pixels(W, H, bases[c][0], (float)bases[c][1] + 0.5f, CF_STRIP_W, px);
    assert(n > 0 && n <= CF_TICK_MAX_PIXELS);
    int n0 = 0;
    for (int i = 0; i < n; i++) {
      for (int j = i + 1; j < n; j++) { assert(px[i].x != px[j].x || px[i].y != px[j].y); }
      assert(min_edge_dist(px[i]) < CF_STRIP_W);
      n0 += min_edge_dist(px[i]) == 0;
    }
    assert(n0 == CF_TICK_LEN);
  }
}

static uint8_t g_scratch[2 * ((W * H + 7) / 8)];
static CfPoint g_shadow[CF_TICK_COUNT * CF_SHADOW_MAX_PER_TICK];

// The shadow is exactly the on-screen non-tick 4-neighbours of tick pixels.
static void test_shadow(void) {
  static CfPoint all[CF_TICK_COUNT * CF_TICK_MAX_PIXELS];
  static int is_tick[H][W], is_shadow[H][W], expect[H][W];
  int n = 0;
  for (int b = 0; b < CF_TICK_COUNT; b++) { n += tick_of(b, all + n); }
  for (int i = 0; i < n; i++) { is_tick[all[i].y][all[i].x] = 1; }
  for (int i = 0; i < n; i++) {
    static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    for (int k = 0; k < 4; k++) {
      int x = all[i].x + dx[k], y = all[i].y + dy[k];
      if (x >= 0 && y >= 0 && x < W && y < H && !is_tick[y][x]) { expect[y][x] = 1; }
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
  printf("shadow pixels: %d (%.1f per tick)\n", sn, sn / 60.0);
}

static void test_boundary_hit(void) {
  int edge;
  float t;
  cf_boundary_hit(W, H, 0.0f, 1.0f, &edge, &t);       // 12 o'clock
  assert(edge == CF_EDGE_TOP && fabsf(t - 100.0f) < 1e-3f);
  cf_boundary_hit(W, H, 1.0f, 0.0f, &edge, &t);       // 3 o'clock
  assert(edge == CF_EDGE_RIGHT && fabsf(t - 114.0f) < 1e-3f);
  cf_boundary_hit(W, H, 0.0f, -1.0f, &edge, &t);      // 6 o'clock
  assert(edge == CF_EDGE_BOTTOM && fabsf(t - 100.0f) < 1e-3f);
  cf_boundary_hit(W, H, -1.0f, 0.0f, &edge, &t);      // 9 o'clock
  assert(edge == CF_EDGE_LEFT && fabsf(t - 114.0f) < 1e-3f);
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

int main(void) {
  test_snap();
  test_nyquist_twelve_oclock();
  test_nyquist_minute_one();
  test_minute_shoulder_is_minimal_offscreen();
  test_boundary_hit();
  test_ticks_geometry();
  test_corner_wrap();
  test_shadow();
  test_tick_colour();
  printf("test_clock_face_calc: OK\n");
  return 0;
}

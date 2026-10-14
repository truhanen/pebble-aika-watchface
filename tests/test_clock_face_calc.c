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

static void tick_of(int b, CfPoint out[CF_TICK_PIXELS]) {
  double a = rad_of_minute(b);
  int edge;
  float t;
  cf_boundary_hit(W, H, (float)sin(a), (float)cos(a), &edge, &t);
  cf_tick_pixels(W, H, edge, t, out);
}

static int g_owner[H][W];

static void test_ticks_geometry(void) {
  assert(CF_TICK_COUNT == 60 && CF_TICK_PIXELS == 24);
  int corner_ticks = 0;
  for (int b = 0; b < CF_TICK_COUNT; b++) {
    CfPoint px[CF_TICK_PIXELS];
    tick_of(b, px);
    int on_ring0[CF_TICK_LEN], n0 = 0;
    for (int i = 0; i < CF_TICK_PIXELS; i++) {
      assert(px[i].x >= 0 && px[i].x < W && px[i].y >= 0 && px[i].y < H);
      assert(min_edge_dist(px[i]) < CF_STRIP_W);        // inside the strip
      assert(g_owner[px[i].y][px[i].x] == 0);           // 24 distinct, none shared
      g_owner[px[i].y][px[i].x] = b + 1;
      if (min_edge_dist(px[i]) == 0) { on_ring0[n0++] = ring0_index(px[i]); }
    }
    // Exactly 6 outer pixels: a run of consecutive ring cells.
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
    // Centre of the 6 cells [first, first + 6) vs the exact boundary point.
    double centre = first + CF_TICK_LEN / 2.0;
    double diff = fmod(centre - perimeter_of_boundary(b) + 1.5 * RING0, RING0) - RING0 / 2.0;
    assert(fabs(diff) <= 0.5 + 1e-3);
    // Does it wrap a corner: some pixel in a 4x4 corner square, plus pixels on both
    // adjoining edges outside it.
    int in_corner = 0, horiz = 0, vert = 0;
    for (int i = 0; i < CF_TICK_PIXELS; i++) {
      int xe = px[i].x < CF_STRIP_W || px[i].x >= W - CF_STRIP_W;
      int ye = px[i].y < CF_STRIP_W || px[i].y >= H - CF_STRIP_W;
      in_corner |= xe && ye;
      horiz |= ye && !xe;
      vert |= xe && !ye;
    }
    if (in_corner && horiz && vert) { corner_ticks++; }
  }
  // The right/left-edge ticks 7, 23, 37 and 53 start or end at a screen corner.
  assert(corner_ticks == 4);
  // No two ticks touch: every 8-neighbour of a tick pixel is free or the same tick.
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      if (!g_owner[y][x]) { continue; }
      for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
          int nx = x + dx, ny = y + dy;
          if (nx < 0 || ny < 0 || nx >= W || ny >= H) { continue; }
          assert(g_owner[ny][nx] == 0 || g_owner[ny][nx] == g_owner[y][x]);
        }
      }
    }
  }
}

static int has_pixel(const CfPoint *p, int x, int y) {
  for (int i = 0; i < CF_TICK_PIXELS; i++) { if (p[i].x == x && p[i].y == y) { return 1; } }
  return 0;
}

// A tick centred right on the top-right corner: cell k = 199 -> ring cells 196..201.
static void test_corner_wrap(void) {
  CfPoint px[CF_TICK_PIXELS];
  cf_tick_pixels(W, H, CF_EDGE_TOP, 199.2f, px);
  for (int x = 196; x <= 199; x++) { assert(has_pixel(px, x, 0)); }   // top, outer layer
  assert(has_pixel(px, 199, 1) && has_pixel(px, 199, 2));             // continues down the right
  assert(!has_pixel(px, 195, 0) && !has_pixel(px, 199, 3));
  // Inner layer 1 turns at its own corner pixel (198,1): ring cells 195..200 are
  // top x=196,197, the corner, then right y=2..4.
  assert(has_pixel(px, 196, 1) && has_pixel(px, 197, 1) && has_pixel(px, 198, 1));
  assert(has_pixel(px, 198, 2) && has_pixel(px, 198, 4));
  assert(!has_pixel(px, 195, 1) && !has_pixel(px, 198, 5));
  // Same on the other three corners (rotated cases): exact pixel count each.
  int bases[4][2] = { { CF_EDGE_RIGHT, H }, { CF_EDGE_BOTTOM, W }, { CF_EDGE_LEFT, H },
                      { CF_EDGE_TOP, 0 } };
  for (int c = 0; c < 4; c++) {
    cf_tick_pixels(W, H, bases[c][0], (float)bases[c][1], px);
    for (int i = 0; i < CF_TICK_PIXELS; i++) {
      for (int j = i + 1; j < CF_TICK_PIXELS; j++) {
        assert(px[i].x != px[j].x || px[i].y != px[j].y);
      }
      assert(min_edge_dist(px[i]) < CF_STRIP_W);
    }
  }
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
  int blacks = 0;
  for (int b = 0; b < CF_TICK_COUNT; b++) {
    CfPoint px[CF_TICK_PIXELS];
    tick_of(b, px);
    int n = 0;
    for (int i = 0; i < CF_TICK_PIXELS; i++) { n += cf_tick_is_black(b, px[i].x, px[i].y); }
    if (b % 5 == 0) { assert(n == CF_TICK_PIXELS); blacks++; } else { assert(n < CF_TICK_PIXELS); }
  }
  assert(blacks == 12);
  // 3/16 Bayer: exactly 3 black pixels in every aligned 4x4 block, at the matrix cells.
  for (int by = 0; by < 4; by++) {
    for (int bx = 0; bx < 4; bx++) {
      int n = 0;
      for (int y = 0; y < 4; y++) { for (int x = 0; x < 4; x++) { n += cf_tick_is_black(1, 4 * bx + x, 4 * by + y); } }
      assert(n == 3);
    }
  }
  assert(cf_tick_is_black(1, 0, 0) && cf_tick_is_black(1, 2, 0) && cf_tick_is_black(1, 2, 2));
  assert(!cf_tick_is_black(1, 1, 0) && !cf_tick_is_black(1, 0, 1) && !cf_tick_is_black(1, 3, 3));
  assert(cf_tick_is_black(1, 4, 8) && cf_tick_is_black(1, 6, 4));   // absolute coordinates tile
  assert(cf_tick_is_black(5, 1, 1) && cf_tick_is_black(0, 1, 1) && !cf_tick_is_black(59, 1, 1));
}

int main(void) {
  test_snap();
  test_nyquist_twelve_oclock();
  test_nyquist_minute_one();
  test_minute_shoulder_is_minimal_offscreen();
  test_boundary_hit();
  test_ticks_geometry();
  test_corner_wrap();
  test_tick_colour();
  printf("test_clock_face_calc: OK\n");
  return 0;
}

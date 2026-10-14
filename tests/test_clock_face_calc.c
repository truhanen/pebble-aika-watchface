// Host tests for src/c/clock_face_calc.c (hand geometry + edge strip).
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

static int minute_at_pixel(int x, int y) {
  return cf_minute_of_direction(2 * x + 1 - 2 * CX, 2 * y + 1 - 2 * CY);
}

static void test_minute_of_direction(void) {
  // Straight up, just right of the 12 o'clock line: minute 0; just left: 59.
  assert(cf_minute_of_direction(1, -100) == 0);
  assert(cf_minute_of_direction(-1, -100) == 59);
  assert(cf_minute_of_direction(0, -100) == 0);
  // 3 / 6 / 9 o'clock boundaries belong to the minute that starts there.
  assert(cf_minute_of_direction(100, 0) == 15);
  assert(cf_minute_of_direction(100, -1) == 14);
  assert(cf_minute_of_direction(0, 100) == 30);
  assert(cf_minute_of_direction(-100, 0) == 45);
  // Against floating point atan2 for every strip pixel, away from boundaries.
  int checked = 0;
  for (int y = 0; y < H; y++) {
    int x0[2], x1[2];
    int n = cf_strip_row_spans(y, W, H, x0, x1);
    for (int s = 0; s < n; s++) {
      for (int x = x0[s]; x < x1[s]; x++) {
        double dx = x + 0.5 - CX, dy = y + 0.5 - CY;
        double deg = atan2(dx, -dy) * 180.0 / M_PI;
        if (deg < 0) { deg += 360.0; }
        double frac = fmod(deg, 6.0);
        if (frac < 0.01 || frac > 5.99) { continue; }
        assert(minute_at_pixel(x, y) == (int)(deg / 6.0));
        checked++;
      }
    }
  }
  assert(checked > 3000);
}

static void test_strip_spans(void) {
  int x0[2], x1[2];
  assert(cf_strip_row_spans(0, W, H, x0, x1) == 1 && x0[0] == 0 && x1[0] == W);
  assert(cf_strip_row_spans(CF_STRIP_W - 1, W, H, x0, x1) == 1);
  assert(cf_strip_row_spans(CF_STRIP_W, W, H, x0, x1) == 2);
  assert(x0[0] == 0 && x1[0] == CF_STRIP_W && x0[1] == W - CF_STRIP_W && x1[1] == W);
  assert(cf_strip_row_spans(H - CF_STRIP_W, W, H, x0, x1) == 1);
  assert(cf_strip_row_spans(H, W, H, x0, x1) == 0);
  int total = 0;
  for (int y = 0; y < H; y++) {
    int n = cf_strip_row_spans(y, W, H, x0, x1);
    for (int s = 0; s < n; s++) { total += x1[s] - x0[s]; }
  }
  assert(total == cf_strip_pixel_count(W, H));
}

static int black_count(int shade) {
  int n = 0;
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) { n += cf_shade_is_black(shade, x, y); }
  }
  return n;
}

static void test_shades(void) {
  assert(cf_strip_shade(0) == 0 && cf_strip_shade(4) == 4 && cf_strip_shade(5) == 0);
  assert(cf_strip_shade(59) == 4);
  assert(black_count(0) == 0);
  assert(black_count(1) == 16);
  assert(black_count(2) == 32);
  assert(black_count(3) == 48);
  assert(black_count(4) == 64);
  // Shades 1 and 3 are exact complements, so an inverted 25% block reads as 75%.
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      assert(cf_shade_is_black(1, x, y) != cf_shade_is_black(3, x, y));
    }
  }
}

int main(void) {
  test_snap();
  test_nyquist_twelve_oclock();
  test_nyquist_minute_one();
  test_minute_shoulder_is_minimal_offscreen();
  test_minute_of_direction();
  test_strip_spans();
  test_shades();
  printf("test_clock_face_calc: OK\n");
  return 0;
}

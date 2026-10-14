#include "clock_face_calc.h"

int cf_snap(float v) {
  return (int)(v >= 0 ? v + 0.5f : v - 0.5f);
}

void cf_hand_pose(float cx, float cy, float sin_a, float cos_a, int shoulder,
                  int width, int apex_ext, int tail, CfHandPose *out) {
  float inner_x = cx - tail * sin_a;
  float inner_y = cy + tail * cos_a;
  float outer_x = cx + shoulder * sin_a;
  float outer_y = cy - shoulder * cos_a;
  float side_x = width / 2.0f * cos_a;
  float side_y = width / 2.0f * sin_a;

  out->inner_left  = (CfPoint){ cf_snap(inner_x - side_x), cf_snap(inner_y - side_y) };
  out->inner_right = (CfPoint){ cf_snap(inner_x + side_x), cf_snap(inner_y + side_y) };
  out->outer_left  = (CfPoint){ cf_snap(outer_x - side_x), cf_snap(outer_y - side_y) };
  out->outer_right = (CfPoint){ cf_snap(outer_x + side_x), cf_snap(outer_y + side_y) };
  out->apex        = (CfPoint){ cf_snap(outer_x + apex_ext * sin_a),
                                cf_snap(outer_y - apex_ext * cos_a) };
}

// Unit vectors (sin, cos) * 32767 of the rays at clockwise angles j*6deg + 3deg,
// j = 0..59, i.e. the edges between tick blocks (y-up frame). Ray j separates
// tick j (below) from tick j+1 (above). Generated, not hand-typed:
//   round(32767 * sin(radians(6*j + 3))), round(32767 * cos(radians(6*j + 3)))
static const int16_t s_ray_sin[60] = {
     1715,   5126,   8481,  11743,  14876,  17846,  20621,  23170,  25465,  27481,
    29196,  30591,  31650,  32364,  32722,  32722,  32364,  31650,  30591,  29196,
    27481,  25465,  23170,  20621,  17846,  14876,  11743,   8481,   5126,   1715,
    -1715,  -5126,  -8481, -11743, -14876, -17846, -20621, -23170, -25465, -27481,
   -29196, -30591, -31650, -32364, -32722, -32722, -32364, -31650, -30591, -29196,
   -27481, -25465, -23170, -20621, -17846, -14876, -11743,  -8481,  -5126,  -1715,
};
static const int16_t s_ray_cos[60] = {
    32722,  32364,  31650,  30591,  29196,  27481,  25465,  23170,  20621,  17846,
    14876,  11743,   8481,   5126,   1715,  -1715,  -5126,  -8481, -11743, -14876,
   -17846, -20621, -23170, -25465, -27481, -29196, -30591, -31650, -32364, -32722,
   -32722, -32364, -31650, -30591, -29196, -27481, -25465, -23170, -20621, -17846,
   -14876, -11743,  -8481,  -5126,  -1715,   1715,   5126,   8481,  11743,  14876,
    17846,  20621,  23170,  25465,  27481,  29196,  30591,  31650,  32364,  32722,
};

// 0 for clockwise angles in [0, 180), 1 for [180, 360). (sx, sy) is y-up.
static int half_of(int32_t sx, int32_t sy) {
  return (sx > 0 || (sx == 0 && sy > 0)) ? 0 : 1;
}

// Is the clockwise angle of a strictly smaller than that of b?
static bool cw_less(int32_t ax, int32_t ay, int32_t bx, int32_t by) {
  int ha = half_of(ax, ay), hb = half_of(bx, by);
  if (ha != hb) { return ha < hb; }
  // Same half-plane: sin(A - B) = ax*by - ay*bx < 0  <=>  A < B.
  return (int64_t)ax * by - (int64_t)ay * bx < 0;
}

int cf_tick_of_direction(int32_t dx2, int32_t dy2) {
  int32_t sx = dx2, sy = -dy2;   // to the y-up frame
  // Binary search for the last ray j with angle(j) <= angle(v); -1 if none (< 3deg).
  int lo = -1, hi = 59;
  while (lo < hi) {
    int mid = (lo + hi + 1) / 2;
    if (cw_less(sx, sy, s_ray_sin[mid], s_ray_cos[mid])) { hi = mid - 1; } else { lo = mid; }
  }
  // Past ray j the tick is j + 1; before ray 0 and past ray 59 it is tick 0.
  return (lo + 1) % 60;
}

int cf_strip_pixel_count(int w, int h) {
  return 2 * CF_STRIP_W * w + 2 * CF_STRIP_W * (h - 2 * CF_STRIP_W);
}

int cf_strip_row_spans(int y, int w, int h, int x0[2], int x1[2]) {
  if (y < 0 || y >= h) { return 0; }
  if (y < CF_STRIP_W || y >= h - CF_STRIP_W) {
    x0[0] = 0; x1[0] = w;
    return 1;
  }
  x0[0] = 0;              x1[0] = CF_STRIP_W;
  x0[1] = w - CF_STRIP_W; x1[1] = w;
  return 2;
}

// 25% black: two pixels per 4x2 tile.
static bool dither_25(int x, int y) {
  return (x % 4 == 0 && y % 2 == 0) || ((x + 2) % 4 == 0 && (y + 1) % 2 == 0);
}

bool cf_shade_is_black(int shade, int x, int y) {
  switch (shade) {
    case 0:  return false;
    case 1:  return dither_25(x, y);
    case 2:
    case 3:  return (x + y) % 2 == 0;   // 50% checkerboard
    default: return true;
  }
}

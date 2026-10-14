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

void cf_boundary_hit(int w, int h, float sin_a, float cos_a, int *edge, float *t) {
  float cx = w / 2.0f, cy = h / 2.0f;
  float dx = sin_a, dy = -cos_a;   // screen frame, y grows downwards
  const float big = 1e30f;
  float tx = big, ty = big;
  if (dx > 1e-6f) { tx = (w - cx) / dx; } else if (dx < -1e-6f) { tx = cx / -dx; }
  if (dy > 1e-6f) { ty = (h - cy) / dy; } else if (dy < -1e-6f) { ty = cy / -dy; }
  if (ty <= tx) {
    float x = cx + dx * ty;
    if (dy < 0) { *edge = CF_EDGE_TOP; *t = x; } else { *edge = CF_EDGE_BOTTOM; *t = w - x; }
  } else {
    float y = cy + dy * tx;
    if (dx > 0) { *edge = CF_EDGE_RIGHT; *t = y; } else { *edge = CF_EDGE_LEFT; *t = h - y; }
  }
}

// Pixel of layer d's ring (rectangle inset by d, walked clockwise from its top-left
// corner, each corner pixel once) at ring index i, any integer (wraps).
static CfPoint ring_pixel(int w, int h, int d, int i) {
  int a = w - 1 - 2 * d, b = h - 1 - 2 * d, len = 2 * a + 2 * b;
  i %= len;
  if (i < 0) { i += len; }
  if (i < a)         { return (CfPoint){ (int16_t)(d + i), (int16_t)d }; }
  i -= a;
  if (i < b)         { return (CfPoint){ (int16_t)(w - 1 - d), (int16_t)(d + i) }; }
  i -= b;
  if (i < a)         { return (CfPoint){ (int16_t)(w - 1 - d - i), (int16_t)(h - 1 - d) }; }
  i -= a;
  return (CfPoint){ (int16_t)d, (int16_t)(h - 1 - d - i) };
}

void cf_tick_pixels(int w, int h, int edge, float t, CfPoint out[CF_TICK_PIXELS]) {
  int k = (int)(t + 0.5f);   // t >= 0: round half up
  int base[4] = { 0, w - 1, w + h - 2, 2 * w + h - 3 };
  int o = base[edge] + k;
  int n = 0;
  for (int d = 0; d < CF_STRIP_W; d++) {
    int start = o - (2 * edge + 1) * d - CF_TICK_LEN / 2;
    for (int j = 0; j < CF_TICK_LEN; j++) { out[n++] = ring_pixel(w, h, d, start + j); }
  }
}

bool cf_tick_is_black(int b, int x, int y) {
  static const uint8_t bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 },
                                       { 3, 11, 1, 9 },  { 15, 7, 13, 5 } };
  return b % 5 == 0 || bayer[y & 3][x & 3] < 3;
}

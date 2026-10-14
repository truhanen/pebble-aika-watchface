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

// Point at doubled perimeter position P (any integer, wraps) on the rectangle of
// outer-ring pixel centres, in doubled coordinates (centre pixel (x,y) is (2x+1,2y+1)),
// walked clockwise from the top-left centre (1,1).
static void perimeter_point(int w, int h, int P, int *X, int *Y) {
  int A = 2 * (w - 1), B = 2 * (h - 1), len = 2 * A + 2 * B;
  P %= len;
  if (P < 0) { P += len; }
  if (P < A)  { *X = 1 + P; *Y = 1; return; }
  P -= A;
  if (P < B)  { *X = 2 * w - 1; *Y = 1 + P; return; }
  P -= B;
  if (P < A)  { *X = 2 * w - 1 - P; *Y = 2 * h - 1; return; }
  P -= A;
  *X = 1; *Y = 2 * h - 1 - P;
}

int cf_tick_pixels(int w, int h, int edge, float t, int depth, CfPoint out[CF_TICK_MAX_PIXELS]) {
  int k = (int)(t + 0.001f);   // floor, tolerant of float error when t is a whole number
  int base[4] = { 0, w - 1, w + h - 2, 2 * w + h - 3 };
  int o = base[edge] + k;
  int x1, y1, x2, y2;
  // Cell i spans doubled perimeter [2i-1, 2i+1], so its edges are at 2(o-2)-1, 2(o+2)+1.
  perimeter_point(w, h, 2 * (o - 2) - 1, &x1, &y1);
  perimeter_point(w, h, 2 * (o + 2) + 1, &x2, &y2);
  x1 -= w; y1 -= h; x2 -= w; y2 -= h;   // directions from the centre (doubled)
  int n = 0;
  for (int d = 0; d < depth; d++) {
    // Ring cells of layer d that can be in the tick lie within a window of the
    // outer-ring index (rays slant by at most ~1.2 px per layer, plus corner mitre).
    int start = o - (2 * edge + 1) * d - 12;
    for (int j = 0; j < 25; j++) {
      CfPoint p = ring_pixel(w, h, d, start + j);
      int vx = 2 * p.x + 1 - w, vy = 2 * p.y + 1 - h;
      if (x1 * vy - y1 * vx >= 0 && vx * y2 - vy * x2 >= 0 && n < CF_TICK_MAX_PIXELS) {
        out[n++] = p;
      }
    }
  }
  return n;
}

int cf_shadow_pixels(int w, int h, const CfPoint *ticks, int n, uint8_t *scratch,
                     CfPoint *out, int max) {
  int bytes = (w * h + 7) / 8;
  uint8_t *is_tick = scratch, *seen = scratch + bytes;
  for (int i = 0; i < 2 * bytes; i++) { scratch[i] = 0; }
  for (int i = 0; i < n; i++) {
    int bit = ticks[i].y * w + ticks[i].x;
    is_tick[bit >> 3] |= (uint8_t)(1 << (bit & 7));
  }
  static const int8_t dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
  int count = 0;
  for (int i = 0; i < n; i++) {
    for (int k = 0; k < 4; k++) {
      int x = ticks[i].x + dx[k], y = ticks[i].y + dy[k];
      if (x < 0 || y < 0 || x >= w || y >= h) { continue; }
      int bit = y * w + x;
      uint8_t m = (uint8_t)(1 << (bit & 7));
      if ((is_tick[bit >> 3] & m) || (seen[bit >> 3] & m)) { continue; }
      seen[bit >> 3] |= m;
      if (count < max) { out[count++] = (CfPoint){ (int16_t)x, (int16_t)y }; }
    }
  }
  return count;
}

bool cf_tick_is_black(int b) {
  return b % 5 == 0;
}

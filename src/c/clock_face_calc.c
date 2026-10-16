#include "clock_face_calc.h"

int cf_snap(float v) {
  return (int)(v >= 0 ? v + 0.5f : v - 0.5f);
}

// Moving clock centre: y of the pivot, x stays at w/2. top = w/2 is the pivot whose
// distance to the top edge equals that to the side edges, bottom = h - w/2 likewise.
// The minute hand points into the top edge for minutes 52.5..7.5 and the bottom edge for
// 22.5..37.5; between, the pivot glides along a smoothstep ease (down 7.5..22.5, up
// 37.5..52.5): s = 3t^2 - 2t^3, so it starts and stops gently at the hold positions.
int cf_pivot_y(int w, int h, int minute) {
  int top = w / 2, span = h - w;   // bottom - top = h - w
  int m = ((minute % 60) + 60) % 60;
  int u;                               // glide progress in 1/30 steps, 0 = top, 30 = bottom
  if (m <= 37) {
    u = 2 * m - 15;                    // down: t = (m - 7.5) / 15
  } else {
    u = 30 - (2 * m - 75);             // up: t = 1 - (m - 37.5) / 15
  }
  if (u < 0) { u = 0; }
  if (u > 30) { u = 30; }
  // smoothstep in integers: t = u/30, s * 27000 = u^2 * (90 - 2u)
  return top + (span * u * u * (90 - 2 * u) + 13500) / 27000;
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

// --- rounded-rectangle ring ------------------------------------------------
// The visible area is the rectangle [0,w] x [0,h] with corners rounded to radius r
// (pixel-edge coordinates). "C" is the curve of pixel centres of the outermost row:
// the same shape inset by half a pixel (corner arc radius r - 0.5, centre (a,a) etc.,
// a = max(r, 0.5)). It is walked clockwise from the start of the top straight by
// arc length s: top straight, TR arc, right straight, BR arc, bottom, BL arc, left,
// TL arc. For r = 0 it is the old rectangle of centres and s is the old ring index.

#define PI_F 3.14159265f

static float tsin(float x) {   // Taylor series, |error| < 1e-6 on [0, pi/2]; no libm
  float x2 = x * x, term = x, sum = x;
  for (int i = 1; i <= 6; i++) { term *= -x2 / (float)((2 * i) * (2 * i + 1)); sum += term; }
  return sum;
}
static float tcos(float x) {
  float x2 = x * x, term = 1, sum = 1;
  for (int i = 1; i <= 6; i++) { term *= -x2 / (float)((2 * i - 1) * (2 * i)); sum += term; }
  return sum;
}
static float tsqrt(float v) {
  if (v <= 0) { return 0; }
  float g = v > 1 ? v : 1;
  for (int i = 0; i < 24; i++) { g = 0.5f * (g + v / g); }
  return g;
}

typedef struct { float a, rc, lt, lr, q, len; } CRing;

static CRing c_ring(int w, int h, int r) {
  CRing c;
  c.a = r > 0 ? (float)r : 0.5f;
  c.rc = c.a - 0.5f;
  c.lt = w - 2 * c.a;
  c.lr = h - 2 * c.a;
  c.q = 0.5f * PI_F * c.rc;
  c.len = 2 * c.lt + 2 * c.lr + 4 * c.q;
  return c;
}

// Corner arcs: index 0 = TR, 1 = BR, 2 = BL, 3 = TL. Centre, and the unit vector
// (sx,sy) from the centre to where the clockwise walk enters the arc; the exit
// direction is (-sy, sx).
static void corner_geom(int w, int h, const CRing *c, int k, float *ax, float *ay, int *sx, int *sy) {
  static const int8_t SX[4] = { 0, 1, 0, -1 }, SY[4] = { -1, 0, 1, 0 };
  *ax = (k == 0 || k == 1) ? w - c->a : c->a;
  *ay = (k == 1 || k == 2) ? h - c->a : c->a;
  *sx = SX[k]; *sy = SY[k];
}

// Point of C at arc length s (any value, wraps).
static void c_point(int w, int h, const CRing *c, float s, float *X, float *Y) {
  while (s < 0) { s += c->len; }
  while (s >= c->len) { s -= c->len; }
  float seg[8] = { c->lt, c->q, c->lr, c->q, c->lt, c->q, c->lr, c->q };
  int i = 0;
  while (i < 7 && s >= seg[i]) { s -= seg[i]; i++; }
  switch (i) {
    case 0: *X = c->a + s;         *Y = 0.5f; return;
    case 2: *X = w - 0.5f;         *Y = c->a + s; return;
    case 4: *X = w - c->a - s;     *Y = h - 0.5f; return;
    case 6: *X = 0.5f;             *Y = h - c->a - s; return;
    default: break;
  }
  int k = i / 2;   // arc 1,3,5,7 -> corner 0,1,2,3
  float ax, ay, phi = c->rc > 0 ? s / c->rc : 0;
  int sx, sy;
  corner_geom(w, h, c, k, &ax, &ay, &sx, &sy);
  float co = tcos(phi), si = tsin(phi);
  *X = ax + c->rc * (co * sx - si * sy);
  *Y = ay + c->rc * (co * sy + si * sx);
}

void cf_boundary_hit(int w, int h, int r, float sin_a, float cos_a, float *hx, float *hy) {
  cf_boundary_hit_from(w, h, r, w / 2.0f, h / 2.0f, sin_a, cos_a, hx, hy);
}

void cf_boundary_hit_from(int w, int h, int r, float cx, float cy, float sin_a, float cos_a, float *hx, float *hy) {
  float dx = sin_a, dy = -cos_a;   // screen frame, y grows downwards
  const float big = 1e30f;
  float tx = big, ty = big;
  if (dx > 1e-6f) { tx = (w - cx) / dx; } else if (dx < -1e-6f) { tx = cx / -dx; }
  if (dy > 1e-6f) { ty = (h - cy) / dy; } else if (dy < -1e-6f) { ty = cy / -dy; }
  float tt = ty <= tx ? ty : tx;
  float x = cx + dx * tt, y = cy + dy * tt;
  if (r > 0) {
    bool left = x < r, right = x > w - r, top = y < r, bot = y > h - r;
    if ((left || right) && (top || bot)) {   // rect hit inside a corner square: exit through the arc
      float ax = left ? (float)r : (float)(w - r), ay = top ? (float)r : (float)(h - r);
      float px = cx - ax, py = cy - ay;
      float b = dx * px + dy * py, cc = px * px + py * py - (float)r * r;
      tt = -b + tsqrt(b * b - cc);
      x = cx + dx * tt; y = cy + dy * tt;
    }
  }
  *hx = x; *hy = y;
}

bool cf_in_band(int w, int h, int r, int x, int y, int depth) {
  if (x < 0 || y < 0 || x >= w || y >= h) { return false; }
  int dx2 = 0, dy2 = 0;
  if (x < r) { dx2 = 2 * x + 1 - 2 * r; } else if (x >= w - r) { dx2 = 2 * x + 1 - 2 * (w - r); }
  if (y < r) { dy2 = 2 * y + 1 - 2 * r; } else if (y >= h - r) { dy2 = 2 * y + 1 - 2 * (h - r); }
  if (dx2 != 0 && dy2 != 0) {   // corner square: distance from the arc centre
    int d = dx2 * dx2 + dy2 * dy2, lo = 2 * r - 2 * depth;
    return d <= 4 * r * r && !(lo > 0 && d <= lo * lo);
  }
  int m = x;
  if (w - 1 - x < m) { m = w - 1 - x; }
  if (y < m) { m = y; }
  if (h - 1 - y < m) { m = h - 1 - y; }
  return m < depth;
}

// s of the tick centre for the boundary point (hx,hy): arc length along C.
static float centre_s(int w, int h, int r, const CRing *c, float hx, float hy) {
  bool left = hx < r, right = hx > w - r, top = hy < r, bot = hy > h - r;
  if (r > 0 && (left || right) && (top || bot)) {
    int k = right ? (top ? 0 : 1) : (bot ? 2 : 3);
    float ax, ay;
    int sx, sy;
    corner_geom(w, h, c, k, &ax, &ay, &sx, &sy);
    float ux = (hx - ax) / r, uy = (hy - ay) / r;
    float sinp = ux * -sy + uy * sx;
    if (sinp < 0) { sinp = 0; }
    if (sinp > 1) { sinp = 1; }
    float lo = 0, hi = 0.5f * PI_F;   // bisect phi on sin
    for (int i = 0; i < 24; i++) {
      float mid = 0.5f * (lo + hi);
      if (tsin(mid) < sinp) { lo = mid; } else { hi = mid; }
    }
    float arc_start[4] = { c->lt, c->lt + c->q + c->lr, 2 * c->lt + 2 * c->q + c->lr,
                           2 * c->lt + 3 * c->q + 2 * c->lr };
    return arc_start[k] + 0.5f * (lo + hi) * c->rc;
  }
  // Straight part: nearest edge, quantised to the pixel cell containing the point.
  float dt = hy, dr = w - hx, db = h - hy, dl = hx;
  int edge = 0;
  float best = dt;
  if (dr < best) { best = dr; edge = 1; }
  if (db < best) { best = db; edge = 2; }
  if (dl < best) { edge = 3; }
  float t = edge == 0 ? hx : edge == 1 ? hy : edge == 2 ? w - hx : h - hy;
  int k = (int)(t + 0.001f);
  float start[4] = { 0, c->lt + c->q, c->lt + c->q + c->lr + c->q, 2 * c->lt + 2 * c->q + c->lr + c->q };
  return start[edge] + (k + 0.5f - c->a);
}

int cf_tick_pixels(int w, int h, int r, float hx, float hy, int depth, int inset, CfPoint out[CF_TICK_MAX_PIXELS]) {
  return cf_tick_pixels_from(w, h, r, w / 2.0f, h / 2.0f, hx, hy, depth, inset, out);
}

int cf_tick_pixels_from(int w, int h, int r, float pcx, float pcy, float hx, float hy, int depth, int inset, CfPoint out[CF_TICK_MAX_PIXELS]) {
  // The curve the tick is laid on is the outer-row curve of the visible area shrunk by
  // `inset` on every side (corner radius r - inset), moved back by (inset, inset).
  int w2 = w - 2 * inset, h2 = h - 2 * inset, r2 = r > inset ? r - inset : 0;
  float lx = hx, ly = hy;   // boundary point of that shrunk shape, on the same ray from the centre
  if (inset > 0) {
    float dx = hx - pcx, dy = hy - pcy, len = tsqrt(dx * dx + dy * dy);
    cf_boundary_hit_from(w2, h2, r2, pcx - inset, pcy - inset, dx / len, -dy / len, &lx, &ly);   // (sin, cos) of the ray
  }
  CRing c = c_ring(w2, h2, r2);
  float sc = centre_s(w2, h2, r2, &c, lx, ly);
  float off = (float)inset;
  float fx, fy, ex1, ey1, ex2, ey2;
  c_point(w2, h2, &c, sc, &fx, &fy);
  c_point(w2, h2, &c, sc - CF_TICK_LEN / 2.0f, &ex1, &ey1);
  c_point(w2, h2, &c, sc + CF_TICK_LEN / 2.0f, &ex2, &ey2);
  fx += off; fy += off; ex1 += off; ey1 += off; ex2 += off; ey2 += off;
  // Directions from the clock centre to E1/E2 in 1/16 px (exact on the straights).
  int x1 = (int)((ex1 - pcx) * 16 + (ex1 >= pcx ? 0.5f : -0.5f));
  int y1 = (int)((ey1 - pcy) * 16 + (ey1 >= pcy ? 0.5f : -0.5f));
  int x2 = (int)((ex2 - pcx) * 16 + (ex2 >= pcx ? 0.5f : -0.5f));
  int y2 = (int)((ey2 - pcy) * 16 + (ey2 >= pcy ? 0.5f : -0.5f));
  int pcx16 = (int)(pcx * 16), pcy16 = (int)(pcy * 16);
  int n = 0;
  int bx = (int)fx, by = (int)fy;
  for (int y = by - 14; y <= by + 14; y++) {
    for (int x = bx - 14; x <= bx + 14; x++) {
      if (!cf_in_band(w, h, r, x, y, inset + depth)) { continue; }
      if (inset > 0 && cf_in_band(w, h, r, x, y, inset)) { continue; }
      int vx = (2 * x + 1) * 8 - pcx16, vy = (2 * y + 1) * 8 - pcy16;
      if (x1 * vy - y1 * vx >= 0 && vx * y2 - vy * x2 >= 0 && n < CF_TICK_MAX_PIXELS) {
        out[n++] = (CfPoint){ (int16_t)x, (int16_t)y };
      }
    }
  }
  return n;
}

const int8_t CF_SHADOW_OFFSETS[CF_SHADOW_OFFSET_COUNT][2] = {
  { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
  { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 },
  { 2, 0 }, { -2, 0 }, { 0, 2 }, { 0, -2 },
};

int cf_shadow_pixels(int w, int h, const CfPoint *ticks, int n, uint8_t *scratch,
                     CfPoint *out, int max) {
  int bytes = (w * h + 7) / 8;
  uint8_t *is_tick = scratch, *seen = scratch + bytes;
  for (int i = 0; i < 2 * bytes; i++) { scratch[i] = 0; }
  for (int i = 0; i < n; i++) {
    int bit = ticks[i].y * w + ticks[i].x;
    is_tick[bit >> 3] |= (uint8_t)(1 << (bit & 7));
  }
  int count = 0;
  for (int i = 0; i < n; i++) {
    for (int k = 0; k < CF_SHADOW_OFFSET_COUNT; k++) {
      int x = ticks[i].x + CF_SHADOW_OFFSETS[k][0], y = ticks[i].y + CF_SHADOW_OFFSETS[k][1];
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

// --- edge-band shadow (no full-screen bitmaps) --------------------------------------------
// Ticks and their shadow lie within CF_BAND_DEPTH px of a screen edge, so the working
// bitmaps only cover that band: the top BD rows, the bottom BD rows, then the left and
// right BD columns of the rows between.
static int band_bit(int w, int h, int x, int y) {
  const int bd = CF_BAND_DEPTH;
  if (x < 0 || y < 0 || x >= w || y >= h) { return -1; }
  if (y < bd) { return y * w + x; }
  if (y >= h - bd) { return bd * w + (y - (h - bd)) * w + x; }
  int col = x < bd ? x : (x >= w - bd ? x - (w - bd) + bd : -1);
  if (col < 0) { return -1; }
  return 2 * bd * w + (y - bd) * 2 * bd + col;
}

int cf_band_bytes(int w, int h) {
  return (2 * CF_BAND_DEPTH * w + (h - 2 * CF_BAND_DEPTH) * 2 * CF_BAND_DEPTH + 7) / 8;
}

int cf_shadow_band(int w, int h, const uint8_t *ticks, int n, uint8_t *scratch, int scratch_bytes,
                   uint8_t *out, int max, int *misses) {
  int bytes = cf_band_bytes(w, h);
  *misses = 0;
  if (scratch_bytes < 2 * bytes) { return 0; }
  uint8_t *is_tick = scratch, *seen = scratch + bytes;
  for (int i = 0; i < 2 * bytes; i++) { scratch[i] = 0; }
  for (int i = 0; i < n; i++) {
    int bit = band_bit(w, h, ticks[2 * i], ticks[2 * i + 1]);
    if (bit < 0) { (*misses)++; continue; }
    is_tick[bit >> 3] |= (uint8_t)(1 << (bit & 7));
  }
  int count = 0;
  for (int i = 0; i < n; i++) {
    for (int k = 0; k < CF_SHADOW_OFFSET_COUNT; k++) {
      int x = ticks[2 * i] + CF_SHADOW_OFFSETS[k][0], y = ticks[2 * i + 1] + CF_SHADOW_OFFSETS[k][1];
      if (x < 0 || y < 0 || x >= w || y >= h) { continue; }
      int bit = band_bit(w, h, x, y);
      if (bit < 0) { (*misses)++; continue; }   // outside the band: cannot dedupe, drop
      uint8_t m = (uint8_t)(1 << (bit & 7));
      if ((is_tick[bit >> 3] & m) || (seen[bit >> 3] & m)) { continue; }
      seen[bit >> 3] |= m;
      if (count < max) { out[2 * count] = (uint8_t)x; out[2 * count + 1] = (uint8_t)y; count++; }
    }
  }
  return count;
}

void cf_sort_xy(uint8_t *xy, int n) {
  for (int i = 1; i < n; i++) {
    uint8_t x = xy[2 * i], y = xy[2 * i + 1];
    int j = i - 1;
    while (j >= 0 && (xy[2 * j + 1] > y || (xy[2 * j + 1] == y && xy[2 * j] > x))) {
      xy[2 * j + 2] = xy[2 * j];
      xy[2 * j + 3] = xy[2 * j + 1];
      j--;
    }
    xy[2 * j + 2] = x;
    xy[2 * j + 3] = y;
  }
}

bool cf_tick_is_major(int b) {
  return b % 5 == 0;
}

#pragma once
// Pure geometry for the clock face (hands + edge tick strip). Pebble-free
// (stdint/stdbool only) so tests/test_clock_face_calc.c can host-compile it.
#include <stdbool.h>
#include <stdint.h>

// Hand dimensions, emery pixels. Taken from the official Nyquist watchface
// (truhanen/pebble-nyquist-watchface, scripts/create_hand_coordinates.py):
// polygon width, shoulder distance, apex extension past the shoulder, tail.
#define CF_HAND_TAIL          18
#define CF_HOUR_WIDTH         25
#define CF_HOUR_SHOULDER      54
#define CF_HOUR_APEX_EXT      10
#define CF_MINUTE_WIDTH       15
#define CF_MINUTE_APEX_EXT     6
#define CF_HAND_EDGE_W         9   // outline stroke (odd: the SDK rounds even widths down)
#define CF_HAND_HALO_W         2   // bg halo around the minute hand, each side
#define CF_PIVOT_R             3

// Minute shoulder: Nyquist's is 86. Ours is the shortest length whose stroked
// shoulder corners lie at least CF_OFFSCREEN_MARGIN px outside the screen at every
// minute, so the hand always runs off the edge (tests/test_clock_face_calc.c
// asserts both properties).
#define CF_MINUTE_SHOULDER   153
#define CF_OFFSCREEN_MARGIN    5

// Edge ticks: one per minute boundary b = 0..59, CF_TICK_LEN px long along the
// edge and CF_STRIP_W px deep. Only tick pixels are ever drawn.
#define CF_STRIP_W      4
#define CF_TICK_LEN     6
#define CF_TICK_COUNT  60
#define CF_TICK_PIXELS (CF_TICK_LEN * CF_STRIP_W)

typedef struct { int16_t x, y; } CfPoint;

typedef struct {
  CfPoint inner_left, inner_right, outer_left, outer_right, apex;
} CfHandPose;

// Round half away from zero (Nyquist's snap_to_nearest_pixel).
int cf_snap(float v);

// Five pentagon corners of a hand pointing at the clockwise angle given by
// sin_a/cos_a (0 = 12 o'clock), each ideal coordinate rounded once to the
// nearest pixel. cx/cy is the pivot in screen coordinates.
void cf_hand_pose(float cx, float cy, float sin_a, float cos_a, int shoulder,
                  int width, int apex_ext, int tail, CfHandPose *out);

// Edges, clockwise from the top; the edge-local coordinate t runs clockwise too.
enum { CF_EDGE_TOP, CF_EDGE_RIGHT, CF_EDGE_BOTTOM, CF_EDGE_LEFT };

// Where the ray from the screen centre (w/2, h/2) at the clockwise angle given by
// sin_a/cos_a (0 = 12 o'clock) meets the outer border, the rectangle [0,w] x [0,h]
// in continuous pixel-edge coordinates. t is the distance along that edge from its
// clockwise start corner: top t = x, right t = y, bottom t = w - x, left t = h - y.
void cf_boundary_hit(int w, int h, float sin_a, float cos_a, int *edge, float *t);

// The CF_TICK_PIXELS pixels of the tick centred on boundary point (edge, t).
//
// Outer ring (the screen's outermost pixels) is walked clockwise from (0,0), each
// corner pixel once: ring index o. The edge cell k = floor(t + 0.5) (round half up)
// has ring index o = base(edge) + k (top 0, right w-1, bottom w+h-2, left 2w+h-3;
// k may run past the edge's ends, which wraps round the corner). The tick covers
// the 6 cells [o - 3, o + 3), i.e. the cell span [k - 3, k + 3) of length 6 whose
// centre k is within 0.5 px of t. Depth layer d = 0..CF_STRIP_W-1 is the same walk
// on the rectangle inset by d, covering the same cells shifted by the layer's
// inward offset (d, 3d, 5d, 7d on top/right/bottom/left): a straight bar on an
// edge, bent mitre-style round a corner, always 6 x CF_STRIP_W pixels.
void cf_tick_pixels(int w, int h, int edge, float t, CfPoint out[CF_TICK_PIXELS]);

// True when pixel (x, y) of tick b is black: all of every 5th tick (0 = 12 o'clock
// included), otherwise the 3/16 level of the 4x4 Bayer matrix (18.75%, black where
// M[y%4][x%4] < 3), in absolute coordinates so the pattern tiles. The rest is white.
bool cf_tick_is_black(int b, int x, int y);

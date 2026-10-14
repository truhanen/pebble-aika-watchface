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

// Edge ticks: one per minute boundary b = 0..59, CF_TICK_LEN px long on the
// outermost pixel row, CF_STRIP_W px deep for 5-minute ticks, CF_MINOR_TICK_DEPTH px
// deep for others, with sides along rays from the screen centre. Only tick pixels are ever drawn.
#define CF_STRIP_W           4   // depth of 5-minute (black) ticks
#define CF_MINOR_TICK_DEPTH  1   // depth of minor (light gray) ticks
#define CF_TICK_LEN          5
#define CF_TICK_COUNT       60
#define CF_TICK_MAX_PIXELS  24   // upper bound on one tick's pixel count (tests check it)

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

// The pixels of the tick at boundary point (edge, t); returns their count (at most
// CF_TICK_MAX_PIXELS). depth is the inward strip width (CF_STRIP_W for 5-minute ticks,
// CF_MINOR_TICK_DEPTH for others).
//
// Outer ring (the screen's outermost pixels) is walked clockwise from (0,0), each
// corner pixel once: ring index o = base(edge) + k (top 0, right w-1, bottom w+h-2,
// left 2w+h-3), where k = floor(t + 0.001) is the edge cell containing the boundary point
// (k may run past an edge's ends: the ring wraps round the corner). The outer-row
// tick is the 5 ring cells [o-2, o+2], centred on the cell containing the point.
//
// Ray rule: the pixel centres of the outer ring form the rectangle [0.5,w-0.5] x
// [0.5,h-0.5]. E1 and E2 are the points on that rectangle half a cell before cell
// o-2 and after cell o+2 (a 5 px perimeter span). A strip pixel (within depth
// of the edge) belongs to the tick iff the direction from the centre (w/2,h/2) to
// its centre lies, clockwise, between the directions to E1 and E2 (ends inclusive).
// Done in exact integers (doubled coordinates, half-plane via two cross products).
// So the outer row has exactly 5 pixels, inner rows follow the rays (slanting near
// corners, where they may hold more or fewer than 5, and bending round corners).
int cf_tick_pixels(int w, int h, int edge, float t, int depth, CfPoint out[CF_TICK_MAX_PIXELS]);

// Upper bound on the shadow pixels of one tick (tests check it).
#define CF_SHADOW_MAX_PER_TICK 16

// Shadow of the ticks: every on-screen pixel that is a 4-neighbour (+-1 in x or y)
// of some pixel of the n given tick pixels (all ticks together) and is not itself a
// tick pixel, each listed once. Same style as the widget value shadow. Returns the
// count, at most `max` (extra pixels are dropped). `scratch` is 2 * ((w*h + 7) / 8)
// bytes of working memory; its contents are overwritten.
int cf_shadow_pixels(int w, int h, const CfPoint *ticks, int n, uint8_t *scratch,
                     CfPoint *out, int max);

// True when tick b is a major (5-minute) tick: all ticks at b % 5 == 0 (0 = 12 o'clock included).
// Major ticks use the foreground colour; minor ticks use light gray.
bool cf_tick_is_major(int b);

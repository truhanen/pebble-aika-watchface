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

// Edge strip: CF_STRIP_W px wide along all four screen edges.
#define CF_STRIP_W 4

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

// Tick index 0..59 of the direction (dx2, dy2) from the clock centre, in doubled
// screen units (dx2 = 2*x + 1 - 2*cx for the centre of pixel x; y grows downwards).
// Tick b is centred on the minute boundary at b*6deg: it covers clockwise angles
// [b*6deg - 3deg, b*6deg + 3deg) from 12 o'clock, so tick 0 straddles 12 o'clock.
int cf_tick_of_direction(int32_t dx2, int32_t dy2);

// Number of strip pixels for a w x h screen, and the strip's spans on row y in
// row-major order: up to two [x0, x1) spans. Returns the span count (0..2).
int cf_strip_pixel_count(int w, int h);
int cf_strip_row_spans(int y, int w, int h, int x0[2], int x1[2]);

// Strip shade of tick b: (b - 1) mod 5, so every 5th tick (the minute marks, 0 =
// 12 o'clock included) is black, then white, 25%, 50%, 50%.
// Shades: 0 = white, 1 = 25% black dither, 2 and 3 = 50% (checkerboard), 4 = black.
static inline int cf_strip_shade(int tick) { return (tick + 4) % 5; }

// True when pixel (x, y) of `shade` is black. The dither patterns use absolute
// coordinates (as in mathewreiss/dithering) so neighbouring blocks tile seamlessly.
bool cf_shade_is_black(int shade, int x, int y);

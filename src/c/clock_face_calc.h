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
#define CF_HAND_HALO_W         2   // bg halo around each hand, each side
#define CF_PIVOT_R             3

// Minute shoulder: Nyquist's is 86. Ours is the shortest length whose stroked
// shoulder corners lie at least CF_OFFSCREEN_MARGIN px outside the screen at every
// minute, so the hand always runs off the edge (tests/test_clock_face_calc.c
// asserts both properties).
#define CF_MINUTE_SHOULDER   153
#define CF_OFFSCREEN_MARGIN    5

// Edge ticks: one per minute boundary b = 0..59, CF_TICK_LEN px long on the outermost
// pixel row, CF_STRIP_W px deep for 5-minute ticks, CF_MINOR_TICK_DEPTH px deep for
// others (which also sit CF_MINOR_TICK_INSET px in from the edge), with sides along
// rays from the screen centre. Only tick pixels are ever drawn.
// The ring follows the visible area, a rectangle with rounded corners (see CF_CORNER_R).
#define CF_STRIP_W           5   // depth of 5-minute (black) ticks
#define CF_MINOR_TICK_DEPTH  1   // depth of minor (light gray) ticks
#define CF_MINOR_TICK_INSET  1   // gap between the screen edge and a minor tick (its shadow covers it)
#define CF_TICK_LEN          5
#define CF_TICK_COUNT       60
#define CF_TICK_MAX_PIXELS  28   // upper bound on one tick's pixel count (tests check it)

// Corner radius (px) of the visible area. The Pebble Time 2's physical frame has rounded
// corners that hide the screen's corner pixels, so the tick ring follows a rounded
// rectangle instead of the screen rectangle. TUNING VALUE: an estimate for the PT2 frame,
// to be confirmed on hardware. 0 gives the plain screen rectangle; must be 0 or >= 1,
// and the geometry is tested for 0..20.
#define CF_CORNER_R         10

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

// Where the ray from the screen centre (w/2, h/2) at the clockwise angle given by
// sin_a/cos_a (0 = 12 o'clock) meets the outer boundary of the visible area: the
// rectangle [0,w] x [0,h] with corners rounded to radius r (r = 0: the plain rectangle),
// in continuous pixel-edge coordinates.
void cf_boundary_hit(int w, int h, int r, float sin_a, float cos_a, float *hx, float *hy);

// Same as cf_boundary_hit with the ray starting at (cx, cy) instead of the screen centre.
void cf_boundary_hit_from(int w, int h, int r, float cx, float cy, float sin_a, float cos_a, float *hx, float *hy);

// y of the moving clock centre (hub of both hands) at the given minute (see clock_face_calc.c).
int cf_pivot_y(int w, int h, int minute);

// True when pixel (x,y) is in the tick band of the given depth: its centre lies inside
// the rounded rectangle and its inset distance from the boundary is in [0, depth).
// On the straights that is the outermost `depth` rows/columns; in a corner it is the
// pixels whose centre is within `depth` of the arc, measured inward: with d the distance
// from the arc centre, (r - depth) < d <= r (doubled integer coordinates, exact).
// Pixels in the hidden corners (outside the rounded rectangle) are never in the band.
bool cf_in_band(int w, int h, int r, int x, int y, int depth);

// The pixels of the tick at boundary point (hx,hy) (from cf_boundary_hit); returns their
// count (at most CF_TICK_MAX_PIXELS). depth is the inward band width (CF_STRIP_W for
// 5-minute ticks, CF_MINOR_TICK_DEPTH for others) and inset how far the band starts inside
// the boundary (0 for 5-minute ticks, CF_MINOR_TICK_INSET for others): the tick's pixels
// have inset distance in [inset, inset + depth), leaving the outer `inset` px free. With
// an inset the tick lies on the same construction applied to the area shrunk by `inset`
// on every side (corner radius r - inset), on the same ray from the centre, so on the
// straights its inset row is 5 px, centred on the ray's crossing.
//
// C is the curve of pixel centres of the outermost row (the rounded rectangle inset by
// half a pixel). The tick centre is the boundary point's position on C; on a straight
// edge it is quantised to the centre of the pixel cell containing the point, on a corner
// arc it is exact. E1 and E2 are the points on C 2.5 px (arc length along C) before and
// after the centre, so the outer row spans 5 px of arc.
//
// Ray rule: a band pixel belongs to the tick iff the direction from the centre (w/2,h/2)
// to its centre lies, clockwise, between the directions to E1 and E2 (ends inclusive).
// Done in exact integers (1/16 px units, half-plane via two cross products; E1/E2 are
// exact on straights). With r = 0 the outer row is exactly 5 pixels; inner rows follow
// the rays (slanting near corners, where they hold more or fewer than 5). Near rounded
// corners the outer row follows the arc and its pixel count varies slightly around 5.
int cf_tick_pixels(int w, int h, int r, float hx, float hy, int depth, int inset, CfPoint out[CF_TICK_MAX_PIXELS]);

// cf_tick_pixels with the tick rays coming from (pcx, pcy) instead of the screen centre
// (hx,hy must come from cf_boundary_hit_from with the same pivot).
int cf_tick_pixels_from(int w, int h, int r, float pcx, float pcy, float hx, float hy, int depth, int inset, CfPoint out[CF_TICK_MAX_PIXELS]);

// Shadow reach in px: the shadow of ticks and icon-only widgets covers the pixels within
// this Manhattan distance of the shape (widget value text uses its own per-font offset
// sets, see widget_text.c): all offsets (dx,dy) with
// 1 <= |dx|+|dy| <= CF_SHADOW_R, a diamond (12 offsets for 2). A diamond keeps the shadow
// 2 px wide along straight edges and avoids the notched corners a plain 4-direction
// offset leaves on diagonals.
#define CF_SHADOW_R             2
#define CF_SHADOW_OFFSET_COUNT 12
// The offsets, {dx, dy} each.
extern const int8_t CF_SHADOW_OFFSETS[CF_SHADOW_OFFSET_COUNT][2];

// Upper bound on the shadow pixels of one tick (tests check it).
#define CF_SHADOW_MAX_PER_TICK 40

// Shadow of the ticks: every on-screen pixel within CF_SHADOW_R (Manhattan distance,
// i.e. offset by one of CF_SHADOW_OFFSETS) of some pixel of the n given tick pixels (all
// ticks together) that is not itself a tick pixel, each listed once. Same shape as the
// widget shadow. Returns the count, at most `max` (extra pixels are dropped). `scratch`
// is 2 * ((w*h + 7) / 8) bytes of working memory; its contents are overwritten.
int cf_shadow_pixels(int w, int h, const CfPoint *ticks, int n, uint8_t *scratch,
                     CfPoint *out, int max);

// Depth of the edge band that holds every tick and shadow pixel (pixel index 0..depth-1
// from the nearest screen edge) = CF_STRIP_W + CF_SHADOW_R; the host test measures a deepest
// pixel at index 6 over every pivot 100..128 and asserts the bound (corners included). A host
// miss is counted, never silently wrong.
#define CF_BAND_DEPTH 7

// Bytes of one band bitmap for a w x h screen.
int cf_band_bytes(int w, int h);

// cf_shadow_pixels over {x, y} byte pairs, with band-only bitmaps. `scratch` needs
// 2 * cf_band_bytes(w, h) bytes (scratch_bytes); output is {x, y} pairs, at most `max`
// (extras dropped). A pixel outside the band is counted in *misses (a shadow one is
// dropped, a tick one ignored); it never happens with CF_BAND_DEPTH and tests assert 0.
// Returns the count.
int cf_shadow_band(int w, int h, const uint8_t *ticks, int n, uint8_t *scratch, int scratch_bytes,
                   uint8_t *out, int max, int *misses);

// Sorts n {x, y} byte pairs by y, then x, in place (insertion sort, no heap).
void cf_sort_xy(uint8_t *xy, int n);

// True when tick b is a major (5-minute) tick: all ticks at b % 5 == 0 (0 = 12 o'clock included).
// Major ticks use the foreground colour; minor ticks use light gray.
bool cf_tick_is_major(int b);

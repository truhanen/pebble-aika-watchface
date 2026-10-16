#pragma once
// Decides whether a widget's background "shadow" can change any pixel: it only paints
// bg, so it matters only where something other than plain bg lies under it -- a clock
// hand (its bg halo, outline and fill) or the pivot, or another widget's ink. Pebble-free
// (stdint/stdbool only) so tests/test_shadow_skip_calc.c can host-compile it.
// Everything is conservative: a false "needed" only costs time, a false "skip" would be a
// visible bug.
#include <stdbool.h>
#include <stdint.h>
#include "clock_face_calc.h"

// Half-open pixel rectangle [x0, x1) x [y0, y1).
typedef struct { int16_t x0, y0, x1, y1; } SsRect;

// The hand stroke (outline + halo) is drawn with a line of width CF_HAND_EDGE_W + 2 *
// CF_HAND_HALO_W; the pixels it can reach lie within 1.5x its half width of the polygon
// edge (square line ends), rounded up.
#define SS_HAND_REACH ((3 * (CF_HAND_EDGE_W + 2 * CF_HAND_HALO_W) + 3) / 4)
// Extra safety distance on top of everything else.
#define SS_MARGIN 1

#define SS_MAX_ITEMS 40

// What one placed widget occupies: its icon / unit box and its whole slot.
typedef struct {
  SsRect icon, slot;
  bool has_icon, used;
} SsItem;

typedef struct {
  CfHandPose hand[2];
  bool has_hands;
  int16_t cx, cy;   // pivot
  SsItem item[SS_MAX_ITEMS];
  int n_items;
} SsScene;

SsRect ss_rect(int x, int y, int w, int h);

// True if the rect (closed, grown by SS_MARGIN) comes within SS_HAND_REACH of the convex
// hand polygon, or within CF_PIVOT_R of the pivot.
bool ss_rect_near_hand(const CfHandPose *hand, SsRect r);

// True if the shadow of ink `ink` (shadow reach `reach` px, any offset within it) may
// change a pixel: the grown ink touches a hand or the pivot, another item's slot
// (grown by SS_MARGIN), or, when check_own_icon, the icon box of item `self`.
bool ss_needs_shadow(const SsScene *s, SsRect ink, int reach, int self, bool check_own_icon);

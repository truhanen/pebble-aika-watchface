// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
// Widget-list payload v2: (id, flags) slots, rotating groups, font and hide bits, caps.
//   gcc -std=c11 -Wall -I src/c tests/test_widget_list.c src/c/widget_list.c
#include "widget_list.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define D WL_FONT_POS_DEFAULT   // flags byte "default font, identifier shown"
#define H WL_FLAG_HIDE

static uint8_t collected[64];
static int collectedN;
static void collect_cb(uint8_t id, void *ctx) { (void)ctx; collected[collectedN++] = id; }

// Sanitize a literal list in place; returns the new length.
#define SAN(arr, maxBytes, maxSlots) WidgetList_sanitize(arr, (int)sizeof(arr), maxBytes, maxSlots)

int main(void) {
  // --- isDrawableId ---
  assert(WidgetList_isDrawableId(0));       // EMPTY holds a slot
  assert(WidgetList_isDrawableId(1));       // BT disconnect (internal, but valid)
  assert(WidgetList_isDrawableId(2));
  assert(!WidgetList_isDrawableId(4));      // the retired DATE widget
  assert(WidgetList_isDrawableId(25));      // SLEEP_DEEP_BAR
  assert(WidgetList_isDrawableId(26));      // first new date widget
  assert(WidgetList_isDrawableId(35));      // digital time
  assert(WidgetList_isDrawableId(38));      // BLUETOOTH_ICON = MAX_WIDGET_TYPE
  assert(!WidgetList_isDrawableId(39));
  assert(WidgetList_isDrawableId(128) && WidgetList_isDrawableId(143) && !WidgetList_isDrawableId(144));
  assert(WidgetList_isDrawableId(200) && WidgetList_isDrawableId(215));
  assert(WidgetList_isDrawableId(216) && WidgetList_isDrawableId(222) && !WidgetList_isDrawableId(223));
  assert(!WidgetList_isDrawableId(255));    // the group marker is not an id
  // the old 0x20 hide-in-id encoding is gone: 7|0x20 = 39 is no widget
  assert(!WidgetList_isDrawableId(7 | 0x20));

  // --- flags ---
  assert(WidgetList_flagFont(0) == 0 && WidgetList_flagFont(16) == 16);
  assert(WidgetList_flagFont(31) == WL_FONT_POS_DEFAULT);
  assert(WidgetList_flagFont(17) == WL_FONT_POS_DEFAULT && WidgetList_flagFont(30) == WL_FONT_POS_DEFAULT);
  assert(WidgetList_flagFont(5 | H) == 5 && WidgetList_flagHidden(5 | H) && !WidgetList_flagHidden(5));
  assert(WidgetList_cleanFlags(5 | H | 0xC0) == (5 | H));       // reserved bits cleared
  assert(WidgetList_cleanFlags(20 | H) == (D | H));              // bad font -> default, hide kept

  // --- intervalSeconds ---
  assert(WidgetList_intervalSeconds(0) == 5);
  assert(WidgetList_intervalSeconds(2) == 30);
  assert(WidgetList_intervalSeconds(3) == 60);
  assert(WidgetList_intervalSeconds(5) == 300);
  assert(WidgetList_intervalSeconds(9) == 60);   // unknown -> 1min

  // --- sanitize: a canonical plain list is unchanged ---
  { uint8_t b[] = {12, D, 15, 3, 17, D | H}; int n = SAN(b, 64, 16);
    assert(n == 6 && b[0] == 12 && b[1] == D && b[2] == 15 && b[3] == 3 && b[4] == 17 && b[5] == (D | H)); }

  // --- sanitize: EMPTY kept, invalid ids dropped with their flags (pairs stay aligned) ---
  { uint8_t b[] = {12, D, 0, D, 99, 5, 17, D}; int n = SAN(b, 64, 16);
    assert(n == 6 && b[0] == 12 && b[2] == 0 && b[4] == 17 && b[5] == D); }

  // --- sanitize: the retired DATE id and a stray marker-valued id are dropped ---
  { uint8_t b[] = {4, D, 7, D}; int n = SAN(b, 64, 16);
    assert(n == 2 && b[0] == 7); }

  // --- sanitize: font out of range falls back to the default, reserved bits cleared ---
  { uint8_t b[] = {7, 20, 10, 0xC0 | 4 | H}; int n = SAN(b, 64, 16);
    assert(n == 4 && b[1] == D && b[3] == (4 | H)); }

  // --- sanitize: a valid group is preserved with each member's flags ---
  { uint8_t b[] = {2, D, 0xFF, 2, 1, 15, 4, 16, D | H}; int n = SAN(b, 64, 16);
    assert(n == 9 && b[2] == 0xFF && b[3] == 2 && b[4] == 1);
    assert(b[5] == 15 && b[6] == 4 && b[7] == 16 && b[8] == (D | H)); }

  // --- sanitize: drop invalid members, fix the count ---
  { uint8_t b[] = {0xFF, 3, 0, 15, D, 99, D, 16, D}; int n = SAN(b, 64, 16);
    assert(n == 7 && b[0] == 0xFF && b[1] == 2 && b[2] == 0 && b[3] == 15 && b[5] == 16); }

  // --- sanitize: a single valid member degrades to a plain slot ---
  { uint8_t b[] = {0xFF, 2, 1, 15, 7, 99, D}; int n = SAN(b, 64, 16);
    assert(n == 2 && b[0] == 15 && b[1] == 7); }
  // ...and a one-member group as sent is a plain slot too
  { uint8_t b[] = {0xFF, 1, 4, 15, 9}; int n = SAN(b, 64, 16);
    assert(n == 2 && b[0] == 15 && b[1] == 9); }

  // --- sanitize: an empty group is dropped ---
  { uint8_t b[] = {12, D, 0xFF, 0, 3, 7, D}; int n = SAN(b, 64, 16);
    assert(n == 4 && b[0] == 12 && b[2] == 7); }

  // --- sanitize: bad interval code clamped to 3 ---
  { uint8_t b[] = {0xFF, 2, 9, 15, D, 16, D}; int n = SAN(b, 64, 16);
    assert(n == 7 && b[2] == 3); }

  // --- sanitize: truncated input ---
  { uint8_t b[] = {12, D, 0xFF, 3, 1, 15, D}; int n = SAN(b, 64, 16);   // group declares 3, has 1 pair
    assert(n == 4 && b[0] == 12 && b[2] == 15); }
  { uint8_t b[] = {12, D, 0xFF, 3, 1}; int n = SAN(b, 64, 16);          // group with no members
    assert(n == 2 && b[0] == 12); }
  { uint8_t b[] = {12, D, 0xFF, 3}; int n = SAN(b, 64, 16);             // group header cut off
    assert(n == 2); }
  { uint8_t b[] = {12, D, 15}; int n = SAN(b, 64, 16);                  // odd trailing byte
    assert(n == 2 && b[0] == 12); }
  { uint8_t b[] = {0xFF}; assert(SAN(b, 64, 16) == 0); }
  assert(WidgetList_sanitize((uint8_t[]){1}, 0, 64, 16) == 0);          // empty list
  assert(WidgetList_sanitize((uint8_t[]){1}, -5, 64, 16) == 0);         // negative length

  // --- sanitize: a group claiming more than 6 members keeps 6 and does not run on ---
  { uint8_t b[] = {0xFF, 8, 1, 2,D, 3,D, 5,D, 6,D, 7,D, 8,D, 9,D, 10,D, 12, D};
    int n = SAN(b, 64, 16);
    assert(n == 3 + 12 + 2 && b[1] == 6 && b[14] == D);                   // 6 members kept, 9 and 10 cut
    assert(b[15] == 12); }                                               // the pair after the group is its own slot

  // --- storage caps: bytes ---
  { uint8_t b[] = {2, D, 3, D, 5, D, 6, D}; int n = WidgetList_sanitize(b, 8, 4, 16);
    assert(n == 4 && b[2] == 3); }                                       // 4-byte budget -> 2 slots
  { uint8_t b[] = {2, D, 0xFF, 3, 1, 5, D, 6, D, 7, D}; int n = WidgetList_sanitize(b, 11, 6, 16);
    assert(n == 2); }                                                    // the group (9 bytes) does not fit: not cut
  // 64 bytes: a longer input is read only up to the cap
  { uint8_t b[80]; for (int i = 0; i < 80; i += 2) { b[i] = 7; b[i + 1] = D; }
    assert(WidgetList_sanitize(b, 80, 64, 100) == 64); }
  // ...and 16 slots at most for the edge lists (32 would fit the bytes)
  { uint8_t b[64]; for (int i = 0; i < 64; i += 2) { b[i] = 7; b[i + 1] = D; }
    assert(WidgetList_sanitize(b, 64, 64, MAX_WIDGET_SLOTS) == 2 * MAX_WIDGET_SLOTS); }

  // --- top / bottom hold a single slot; a group counts as one ---
  { uint8_t b[] = {30, D, 7, D}; int n = SAN(b, 16, 1);
    assert(n == 2 && b[0] == 30); }
  { uint8_t b[] = {0xFF, 3, 3, 30, D, 32, D, 33, D, 7, D}; int n = SAN(b, 16, 1);
    assert(n == 9 && b[0] == 0xFF && b[1] == 3 && b[9 - 1] == D); }      // whole group, nothing after it
  { uint8_t b[] = {99, D, 35, D, 7, D}; int n = SAN(b, 16, 1);
    assert(n == 2 && b[0] == 35); }                                      // the first VALID slot counts

  // --- parse: plain slots, fonts and hide ---
  { WidgetSlot s[MAX_WIDGET_SLOTS]; uint8_t b[] = {12, D, 0, 3, 7, 5 | H};
    int n = WidgetList_parse(b, 6, s, MAX_WIDGET_SLOTS);
    assert(n == 3 && s[0].count == 1 && s[0].members[0] == 12 && s[0].flags[0] == D);
    assert(s[1].members[0] == 0 && WidgetList_flagFont(s[1].flags[0]) == 3);
    assert(s[2].members[0] == 7 && WidgetList_flagFont(s[2].flags[0]) == 5 && WidgetList_flagHidden(s[2].flags[0])); }

  // --- parse: group -> members, per-member flags, interval ---
  { WidgetSlot s[MAX_WIDGET_SLOTS]; uint8_t b[] = {2, D, 0xFF, 3, 1, 15, 4, 16, D | H, 17, 16};
    int n = WidgetList_parse(b, 11, s, MAX_WIDGET_SLOTS);
    assert(n == 2);
    assert(s[0].count == 1 && s[0].members[0] == 2);
    assert(s[1].count == 3 && s[1].interval_code == 1);
    assert(s[1].members[0] == 15 && s[1].flags[0] == 4);
    assert(s[1].members[1] == 16 && s[1].flags[1] == (D | H));
    assert(s[1].members[2] == 17 && s[1].flags[2] == 16);
    assert(WidgetSlot_activeIndex(&s[1], 0) == 0 && WidgetSlot_activeIndex(&s[1], 10) == 1);
    assert(WidgetSlot_activeIndex(&s[0], 99) == 0); }

  // --- parse: maxSlots, and agreement with sanitize on a messy list ---
  { WidgetSlot s[2]; uint8_t b[] = {1, D, 2, D, 3, D};
    assert(WidgetList_parse(b, 6, s, 2) == 2); }
  { uint8_t raw[] = {12, 0xE0 | 4, 99, 0, 0xFF, 3, 9, 15, D, 4, D, 16, 0x30};
    WidgetSlot s[MAX_WIDGET_SLOTS];
    int n = WidgetList_parse(raw, (int)sizeof(raw), s, MAX_WIDGET_SLOTS);
    uint8_t c[sizeof(raw)]; memcpy(c, raw, sizeof(raw));
    int m = WidgetList_sanitize(c, (int)sizeof(c), 64, MAX_WIDGET_SLOTS);
    WidgetSlot s2[MAX_WIDGET_SLOTS];
    assert(WidgetList_parse(c, m, s2, MAX_WIDGET_SLOTS) == n);
    assert(n == 2 && s[0].flags[0] == (4 | H) && s[1].count == 2 && s[1].flags[1] == (16 | H));
    (void)s2; }

  // --- forEachId: members + plain ids, hidden or not ---
  { collectedN = 0; uint8_t b[] = {2, D, 0xFF, 2, 1, 15, D | H, 16, 3, 3, 4};
    WidgetList_forEachId(b, (int)sizeof(b), collect_cb, NULL);
    assert(collectedN == 4 && collected[0] == 2 && collected[1] == 15
           && collected[2] == 16 && collected[3] == 3); }

  // --- minSubMinuteIntervalSec ---
  { uint8_t none[] = {12, D, 0xFF, 2, 3, 15, D, 16, D};   // group @ 1min -> no sub-minute
    assert(WidgetList_minSubMinuteIntervalSec(none, (int)sizeof(none)) == 0); }
  { uint8_t a[] = {0xFF, 2, 1, 15, D, 16, D};             // 10s
    assert(WidgetList_minSubMinuteIntervalSec(a, (int)sizeof(a)) == 10); }
  { uint8_t a[] = {0xFF, 2, 2, 15, D, 16, D, 0xFF, 2, 1, 17, D, 200, D}; // 30s + 10s -> min 10
    assert(WidgetList_minSubMinuteIntervalSec(a, (int)sizeof(a)) == 10); }
  { uint8_t a[] = {0xFF, 1, 0, 15, D};                    // a one-member "group" does not rotate
    assert(WidgetList_minSubMinuteIntervalSec(a, (int)sizeof(a)) == 0); }

  // --- activeIndex: rotation by seconds ---
  { WidgetSlot s; s.count = 3; s.interval_code = 1; // 10s
    assert(WidgetSlot_activeIndex(&s, 0)  == 0);
    assert(WidgetSlot_activeIndex(&s, 10) == 1);
    assert(WidgetSlot_activeIndex(&s, 25) == 2);    // 20..29
    assert(WidgetSlot_activeIndex(&s, 30) == 0); }  // wraps
  { WidgetSlot s; s.count = 1;
    assert(WidgetSlot_activeIndex(&s, 12345) == 0); }

  printf("All widget_list tests passed\n");
  return 0;
}

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Widget-list payload v2 (docs/widget_contract.md), shared by the watch (settings /
   messaging / widget layer) and host-compiled for tests/test_widget_list.c. Pebble-free
   (stdint/stdbool only).

   A "list" is a byte array; each slot is either ONE plain widget, 2 bytes
   `id, flags`, or a rotating group: WIDGET_ROTATING_MARKER, count, interval_code,
   then count x (id, flags). flags: bits 0-4 font index (31 = the position's default),
   bit 5 hide the identifier (icon / unit), bits 6-7 reserved (cleared). */

// Numeric mirrors -- keep in sync with src/c/widgets.h (MAX_WIDGET_TYPE),
// src/c/crypto.h (CRYPTO_WID_BASE / MAX_CRYPTO), src/c/tuya.h, src/ts/widget_list_payload.ts,
// src/ts/config_tuya_list.ts.
#define WL_MAX_WIDGET_TYPE   38    // BLUETOOTH_ICON
#define WL_RETIRED_DATE_ID   4     // the old DATE widget, replaced by 26..34
#define WL_CRYPTO_WID_BASE   200
#define WL_MAX_CRYPTO        16
#define WL_CURRENCY_WID_BASE 216
#define WL_MAX_CURRENCY      7
#define WL_TUYA_WID_BASE     128
#define WL_MAX_TUYA          16

#define WIDGET_ROTATING_MARKER 0xFF
#define MAX_GROUP_MEMBERS      6
#define MAX_WIDGET_SLOTS       16

#define WL_FLAG_FONT_MASK   0x1F
#define WL_FLAG_HIDE        0x20
#define WL_FLAG_RESERVED    0xC0
#define WL_FONT_MAX         16     // highest font index (widget_fonts.h)
#define WL_FONT_POS_DEFAULT 31     // "use the position's default font"

// The font bits of a flags byte as the layer should use them: 0..WL_FONT_MAX, or
// WL_FONT_POS_DEFAULT for 31 and for anything out of range.
uint8_t WidgetList_flagFont(uint8_t flags);
bool    WidgetList_flagHidden(uint8_t flags);
// A flags byte in canonical form: out-of-range font -> 31, reserved bits cleared.
uint8_t WidgetList_cleanFlags(uint8_t flags);

typedef struct {
  uint8_t members[MAX_GROUP_MEMBERS];   // widget ids
  uint8_t flags[MAX_GROUP_MEMBERS];     // canonical flags of each member
  uint8_t count;          // 1 = plain slot; 2..MAX_GROUP_MEMBERS = rotating
  uint8_t interval_code;  // 0..5, meaningful only when count > 1
} WidgetSlot;

// Drawable id: a normal type 0..WL_MAX_WIDGET_TYPE (not the retired DATE) or a crypto /
// currency / tuya wid. EMPTY(0) draws nothing but is a real slot (it reserves the ink
// height of its font), so lists keep it.
bool WidgetList_isDrawableId(uint8_t id);

// interval_code 0..5 -> seconds {5,10,30,60,120,300}; any other code -> 60.
int WidgetList_intervalSeconds(uint8_t code);

// Clean a list IN PLACE to canonical form: drop non-drawable ids and members, clean
// flags, clamp interval_code, degrade a group with <2 valid members to a plain slot (or
// drop it), drop a truncated trailing slot, keep at most maxSlots slots (a group counts
// as one). Reads at most min(len, maxBytes) bytes. Returns the new byte length
// (<= maxBytes).
int WidgetList_sanitize(uint8_t *bytes, int len, int maxBytes, int maxSlots);

// Parse a list into WidgetSlot[] (same rules as sanitize, so a sanitized list parses
// to itself). Returns slot count (<= maxSlots).
int WidgetList_parse(const uint8_t *bytes, int len, WidgetSlot *out, int maxSlots);

// Invoke cb(id, ctx) for every underlying drawable widget id (group members + plain
// ids).
void WidgetList_forEachId(const uint8_t *bytes, int len,
                          void (*cb)(uint8_t id, void *ctx), void *ctx);

// Shortest sub-minute (5/10/30 s) rotation interval present, or 0 if none.
int WidgetList_minSubMinuteIntervalSec(const uint8_t *bytes, int len);

// Index of the member a slot shows at the given seconds-of-day. count<=1 -> 0.
int WidgetSlot_activeIndex(const WidgetSlot *slot, int secondsOfDay);

// Resolve where the auto-battery / disconnect-icon fallback goes in a column.
//   count        configured slots in the chosen column
//   visibleCount how many fully fit (0..count)
//   appendFits   true iff all `count` slots fit AND one more (the fallback) also
//                fits AND there is array room for it (caller computes)
//   position     raw 1-based setting; clamped to [1, count+1] internally
// On return: *outAppend==true  -> add a NEW slot at *outIndex (== count);
//            *outAppend==false -> overwrite existing slot *outIndex.
// Safety net: a non-fitting append, or a replace target below the fold, lands on
// the last VISIBLE slot so the warning is always drawn.
void WidgetList_fallbackPlace(int count, int visibleCount, bool appendFits,
                              int position, int *outIndex, bool *outAppend);

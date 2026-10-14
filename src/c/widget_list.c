// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#include "widget_list.h"

uint8_t WidgetList_flagFont(uint8_t flags) {
  uint8_t f = flags & WL_FLAG_FONT_MASK;
  return f <= WL_FONT_MAX ? f : WL_FONT_POS_DEFAULT;
}
bool WidgetList_flagHidden(uint8_t flags) { return (flags & WL_FLAG_HIDE) != 0; }
uint8_t WidgetList_cleanFlags(uint8_t flags) {
  return (uint8_t)(WidgetList_flagFont(flags) | (flags & WL_FLAG_HIDE));
}

bool WidgetList_isDrawableId(uint8_t id) {
  if (id == 0) { return true; }                        // EMPTY: draws nothing, but holds a slot
  if (id == WL_RETIRED_DATE_ID) { return false; }
  if (id <= WL_MAX_WIDGET_TYPE) { return true; }
  if (id >= WL_CRYPTO_WID_BASE && id < WL_CRYPTO_WID_BASE + WL_MAX_CRYPTO) { return true; }
  if (id >= WL_CURRENCY_WID_BASE && id < WL_CURRENCY_WID_BASE + WL_MAX_CURRENCY) { return true; }
  if (id >= WL_TUYA_WID_BASE && id < WL_TUYA_WID_BASE + WL_MAX_TUYA) { return true; }
  return false;
}

int WidgetList_intervalSeconds(uint8_t code) {
  switch (code) {
    case 0: return 5;
    case 1: return 10;
    case 2: return 30;
    case 3: return 60;
    case 4: return 120;
    case 5: return 300;
    default: return 60;
  }
}

// One slot as read from the bytes: only drawable members, with canonical flags.
typedef struct {
  uint8_t id[MAX_GROUP_MEMBERS], flags[MAX_GROUP_MEMBERS];
  uint8_t count;      // valid members kept (0 = the slot is dropped)
  uint8_t interval;
} RawSlot;

// Read the slot at *i and advance *i. False at the end of the list or when what is
// left is truncated (that rest is dropped).
static bool read_slot(const uint8_t *b, int len, int *i, RawSlot *s) {
  s->count = 0;
  s->interval = 3;
  if (*i >= len) { return false; }
  if (b[*i] == WIDGET_ROTATING_MARKER) {
    if (*i + 2 >= len) { *i = len; return false; }     // truncated header
    int count = b[*i + 1];
    s->interval = b[*i + 2] > 5 ? 3 : b[*i + 2];
    int p = *i + 3;
    int avail = (len - p) / 2;
    int n = count < avail ? count : avail;             // a short group keeps its whole pairs
    for (int m = 0; m < n; m++) {
      uint8_t id = b[p + 2 * m];
      if (s->count < MAX_GROUP_MEMBERS && WidgetList_isDrawableId(id)) {
        s->id[s->count] = id;
        s->flags[s->count] = WidgetList_cleanFlags(b[p + 2 * m + 1]);
        s->count++;
      }
    }
    *i = (count > avail) ? len : p + 2 * n;
    return true;
  }
  if (*i + 1 >= len) { *i = len; return false; }       // truncated plain slot
  if (WidgetList_isDrawableId(b[*i])) {
    s->id[0] = b[*i];
    s->flags[0] = WidgetList_cleanFlags(b[*i + 1]);
    s->count = 1;
  }
  *i += 2;
  return true;
}

int WidgetList_sanitize(uint8_t *bytes, int len, int maxBytes, int maxSlots) {
  uint8_t tmp[64];
  if (maxBytes > (int)sizeof(tmp)) { maxBytes = (int)sizeof(tmp); }
  if (len > maxBytes) { len = maxBytes; }
  if (len < 0) { len = 0; }
  int w = 0, i = 0, slots = 0;
  RawSlot s;
  while (slots < maxSlots && read_slot(bytes, len, &i, &s)) {
    if (s.count == 0) { continue; }
    int need = s.count >= 2 ? 3 + 2 * s.count : 2;
    if (w + need > maxBytes) { break; }
    if (s.count >= 2) {
      tmp[w++] = WIDGET_ROTATING_MARKER;
      tmp[w++] = s.count;
      tmp[w++] = s.interval;
    }
    for (int m = 0; m < s.count; m++) {
      tmp[w++] = s.id[m];
      tmp[w++] = s.flags[m];
    }
    slots++;
  }
  for (int k = 0; k < w; k++) { bytes[k] = tmp[k]; }
  return w;
}

int WidgetList_parse(const uint8_t *bytes, int len, WidgetSlot *out, int maxSlots) {
  int n = 0, i = 0;
  RawSlot s;
  while (n < maxSlots && read_slot(bytes, len, &i, &s)) {
    if (s.count == 0) { continue; }
    for (int m = 0; m < s.count; m++) {
      out[n].members[m] = s.id[m];
      out[n].flags[m] = s.flags[m];
    }
    out[n].count = s.count;
    out[n].interval_code = s.count >= 2 ? s.interval : 3;
    n++;
  }
  return n;
}

void WidgetList_forEachId(const uint8_t *bytes, int len,
                          void (*cb)(uint8_t id, void *ctx), void *ctx) {
  int i = 0;
  RawSlot s;
  while (read_slot(bytes, len, &i, &s)) {
    for (int m = 0; m < s.count; m++) { cb(s.id[m], ctx); }
  }
}

int WidgetList_minSubMinuteIntervalSec(const uint8_t *bytes, int len) {
  int best = 0, i = 0;
  RawSlot s;
  while (read_slot(bytes, len, &i, &s)) {
    if (s.count < 2) { continue; }
    int sec = WidgetList_intervalSeconds(s.interval);
    if (sec < 60 && (best == 0 || sec < best)) { best = sec; }
  }
  return best;
}

int WidgetSlot_activeIndex(const WidgetSlot *slot, int secondsOfDay) {
  if (slot->count <= 1) { return 0; }
  int sec = WidgetList_intervalSeconds(slot->interval_code);
  if (sec <= 0) { sec = 60; }
  int idx = (secondsOfDay / sec) % slot->count;
  if (idx < 0) { idx += slot->count; }
  return idx;
}

void WidgetList_fallbackPlace(int count, int visibleCount, bool appendFits,
                              int position, int *outIndex, bool *outAppend) {
  if (position < 1) { position = 1; }
  if (position > count + 1) { position = count + 1; }

  if (count <= 0) {                 // empty column: fallback becomes the sole slot
    *outIndex = 0; *outAppend = true; return;
  }
  if (position == count + 1) {      // append intent
    if (appendFits) { *outIndex = count; *outAppend = true; return; }
    *outIndex = (visibleCount > 0) ? (visibleCount - 1) : 0;   // no room -> last visible
    *outAppend = false; return;
  }
  int t = position - 1;             // replace intent
  if (t < visibleCount) { *outIndex = t; *outAppend = false; return; }
  *outIndex = (visibleCount > 0) ? (visibleCount - 1) : 0;     // below fold -> last visible
  *outAppend = false;
}

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#include "widget_list.h"

uint8_t WidgetList_baseId(uint8_t b)   { return b & (uint8_t)~WIDGET_HIDE_FLAG; }
bool    WidgetList_isHidden(uint8_t b) { return (b & WIDGET_HIDE_FLAG) != 0; }

bool WidgetList_isDrawableId(uint8_t id) {
  id = WidgetList_baseId(id);
  if (id == 0) { return false; }                       // EMPTY draws nothing
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

int WidgetList_sanitize(uint8_t *bytes, int len, int maxBytes) {
  uint8_t tmp[64];
  if (maxBytes > (int)sizeof(tmp)) { maxBytes = (int)sizeof(tmp); }
  if (len > maxBytes) { len = maxBytes; }
  if (len < 0) { len = 0; }
  int w = 0, i = 0;
  while (i < len) {
    uint8_t b = bytes[i];
    if (b == WIDGET_ROTATING_MARKER) {
      if (i + 2 >= len) { break; }                     // truncated header -> drop rest
      uint8_t count = bytes[i + 1];
      uint8_t interval = bytes[i + 2];
      if (interval > 5) { interval = 3; }
      if (count > MAX_GROUP_MEMBERS) { count = MAX_GROUP_MEMBERS; }
      int memStart = i + 3;
      int avail = len - memStart;
      if ((int)count > avail) { count = (uint8_t)(avail < 0 ? 0 : avail); }
      uint8_t valid[MAX_GROUP_MEMBERS];
      int v = 0;
      for (int m = 0; m < count; m++) {
        uint8_t id = bytes[memStart + m];
        if (WidgetList_isDrawableId(id)) { valid[v++] = id; }
      }
      i = memStart + count;
      if (v >= 2) {
        if (w + 3 + v <= maxBytes) {
          tmp[w++] = WIDGET_ROTATING_MARKER;
          tmp[w++] = (uint8_t)v;
          tmp[w++] = interval;
          for (int m = 0; m < v; m++) { tmp[w++] = valid[m]; }
        }
      } else if (v == 1) {
        if (w + 1 <= maxBytes) { tmp[w++] = valid[0]; }
      }
      // v == 0: drop the group entirely
    } else {
      if (WidgetList_isDrawableId(b) && w + 1 <= maxBytes) { tmp[w++] = b; }
      i += 1;
    }
  }
  for (int k = 0; k < w; k++) { bytes[k] = tmp[k]; }
  return w;
}

int WidgetList_parse(const uint8_t *bytes, int len, WidgetSlot *out, int maxSlots) {
  int n = 0, i = 0;
  while (i < len && n < maxSlots) {
    uint8_t b = bytes[i];
    if (b == WIDGET_ROTATING_MARKER) {
      if (i + 2 >= len) { break; }
      uint8_t count = bytes[i + 1];
      uint8_t interval = bytes[i + 2];
      if (count > MAX_GROUP_MEMBERS) { count = MAX_GROUP_MEMBERS; }
      if (interval > 5) { interval = 3; }
      int memStart = i + 3;
      int avail = len - memStart;
      if ((int)count > avail) { count = (uint8_t)(avail < 0 ? 0 : avail); }
      int v = 0;
      for (int m = 0; m < count; m++) {
        uint8_t raw = bytes[memStart + m];
        if (WidgetList_isDrawableId(raw)) {
          out[n].members[v] = WidgetList_baseId(raw);
          out[n].hide[v] = WidgetList_isHidden(raw);
          v++;
        }
      }
      i = memStart + count;
      if (v >= 2) {
        out[n].count = (uint8_t)v; out[n].interval_code = interval; n++;
      } else if (v == 1) {
        out[n].count = 1; out[n].interval_code = 3; n++;   // members[0]/hide[0] already set
      }
    } else {
      if (WidgetList_isDrawableId(b)) {
        out[n].members[0] = WidgetList_baseId(b);
        out[n].hide[0] = WidgetList_isHidden(b);
        out[n].count = 1; out[n].interval_code = 3; n++;
      }
      i += 1;
    }
  }
  return n;
}

void WidgetList_forEachId(const uint8_t *bytes, int len,
                          void (*cb)(uint8_t id, void *ctx), void *ctx) {
  int i = 0;
  while (i < len) {
    uint8_t b = bytes[i];
    if (b == WIDGET_ROTATING_MARKER) {
      if (i + 2 >= len) { break; }
      uint8_t count = bytes[i + 1];
      if (count > MAX_GROUP_MEMBERS) { count = MAX_GROUP_MEMBERS; }
      int memStart = i + 3;
      int avail = len - memStart;
      if ((int)count > avail) { count = (uint8_t)(avail < 0 ? 0 : avail); }
      for (int m = 0; m < count; m++) {
        uint8_t id = bytes[memStart + m];
        if (WidgetList_isDrawableId(id)) { cb(WidgetList_baseId(id), ctx); }
      }
      i = memStart + count;
    } else {
      if (WidgetList_isDrawableId(b)) { cb(WidgetList_baseId(b), ctx); }
      i += 1;
    }
  }
}

int WidgetList_minSubMinuteIntervalSec(const uint8_t *bytes, int len) {
  int best = 0, i = 0;
  while (i < len) {
    uint8_t b = bytes[i];
    if (b == WIDGET_ROTATING_MARKER) {
      if (i + 2 >= len) { break; }
      uint8_t count = bytes[i + 1];
      uint8_t interval = bytes[i + 2];
      if (count > MAX_GROUP_MEMBERS) { count = MAX_GROUP_MEMBERS; }
      int sec = WidgetList_intervalSeconds(interval);
      if (sec < 60 && (best == 0 || sec < best)) { best = sec; }
      int memStart = i + 3;
      int avail = len - memStart;
      if ((int)count > avail) { count = (uint8_t)(avail < 0 ? 0 : avail); }
      i = memStart + count;
    } else {
      i += 1;
    }
  }
  return best;
}

uint8_t WidgetSlot_activeMember(const WidgetSlot *slot, int secondsOfDay) {
  if (slot->count <= 1) { return slot->members[0]; }
  int sec = WidgetList_intervalSeconds(slot->interval_code);
  if (sec <= 0) { sec = 60; }
  int step = secondsOfDay / sec;
  int idx = step % slot->count;
  if (idx < 0) { idx += slot->count; }
  return slot->members[idx];
}

bool WidgetSlot_activeHide(const WidgetSlot *slot, int secondsOfDay) {
  if (slot->count <= 1) { return slot->hide[0]; }
  int sec = WidgetList_intervalSeconds(slot->interval_code);
  if (sec <= 0) { sec = 60; }
  int idx = (secondsOfDay / sec) % slot->count;
  if (idx < 0) { idx += slot->count; }
  return slot->hide[idx];
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

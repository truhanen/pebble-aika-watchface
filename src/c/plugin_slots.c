// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include "plugin_slots.h"
#include <string.h>
#include <stdlib.h>

#define PLUGIN_DELIM '\x1f'

// Copy [start, end) into dst (size dstSize), NUL-terminated, cutting at a UTF-8
// character boundary when it does not fit.
static void copy_field(char *dst, uint8_t dstSize, const char *start, const char *end) {
  size_t n = (size_t)(end - start);
  if (n > (size_t)(dstSize - 1)) {
    n = (size_t)(dstSize - 1);
    while (n > 0 && ((uint8_t)start[n] & 0xC0) == 0x80) { n--; }   // start[n] is a continuation byte
  }
  memcpy(dst, start, n);
  dst[n] = '\0';
}

// The next field starting at *p: sets [*start, *end) and advances *p past the delimiter.
// Returns false when no field remains (only the first field of a record may be absent
// at end of string; callers check). `last` fields may end at the NUL.
static bool next_field(const char **p, const char **start, const char **end, bool last) {
  if (**p == '\0' && !last) { return false; }
  const char *e = strchr(*p, PLUGIN_DELIM);
  if (!e) {
    if (!last) { return false; }
    e = *p + strlen(*p);
    *start = *p; *end = e; *p = e;
    return true;
  }
  *start = *p; *end = e; *p = e + 1;
  return true;
}

uint8_t PluginSlots_parse(const char *data, PluginSlot *slots) {
  uint8_t applied = 0;
  if (!data) { return 0; }
  const char *p = data;
  while (*p) {
    const char *s, *e;
    char idBuf[6];
    if (!next_field(&p, &s, &e, false)) { break; }
    copy_field(idBuf, sizeof(idBuf), s, e);
    int idx = atoi(idBuf);
    char value[PLUGIN_VALUE_LEN], unit[PLUGIN_UNIT_LEN];
    if (!next_field(&p, &s, &e, false)) { break; }
    copy_field(value, PLUGIN_VALUE_LEN, s, e);
    if (!next_field(&p, &s, &e, true)) { break; }   // unit: last field, may end the string
    copy_field(unit, PLUGIN_UNIT_LEN, s, e);
    if (idx >= 0 && idx < PLUGIN_SLOT_COUNT) {
      PluginSlot *slot = &slots[idx];
      memcpy(slot->value, value, PLUGIN_VALUE_LEN);
      memcpy(slot->unit, unit, PLUGIN_UNIT_LEN);
      slot->kind = (strcmp(value, PLUGIN_ICON_MARK) == 0) ? PLUGIN_KIND_ICON : PLUGIN_KIND_NUMBER;
      slot->has = 1;
      applied++;
    }
  }
  return applied;
}

PluginSlot *PluginSlots_get(PluginSlot *slots, uint8_t index) {
  if (index >= PLUGIN_SLOT_COUNT || !slots[index].has) { return NULL; }
  return &slots[index];
}

bool PluginIcon_valid(uint8_t w, uint8_t h, uint16_t paletteLen, uint16_t pixelLen) {
  if (w < 1 || w > PLUGIN_ICON_MAX_DIM || h < 1 || h > PLUGIN_ICON_MAX_DIM) { return false; }
  return paletteLen == PLUGIN_ICON_PALETTE_LEN && pixelLen == (uint16_t)h * ((w + 1) / 2);
}

void PluginIcon_copyRows(uint8_t *dst, uint16_t dstStride, const uint8_t *src, uint8_t w, uint8_t h) {
  uint16_t srcStride = (uint16_t)((w + 1) / 2);
  for (uint8_t y = 0; y < h; y++) {
    memcpy(dst + (size_t)y * dstStride, src + (size_t)y * srcStride, srcStride);
    if (dstStride > srcStride) { memset(dst + (size_t)y * dstStride + srcStride, 0, dstStride - srcStride); }
  }
}

uint8_t PluginIcon_colour(uint8_t idx, uint8_t stroke, uint8_t fill) {
  return idx == 0 ? stroke : (idx == 1 ? fill : 0x00);
}

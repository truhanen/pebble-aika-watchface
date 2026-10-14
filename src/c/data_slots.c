// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include "data_slots.h"
#include <string.h>
#include <stdlib.h>

#define DATA_SLOT_DELIM '\x1f'

// Copy up to dstSize-1 chars of [start, end) into dst, NUL-terminated.
static void copy_field(char *dst, uint8_t dstSize, const char *start, const char *end) {
  size_t n = (size_t)(end - start);
  if (n > (size_t)(dstSize - 1)) { n = (size_t)(dstSize - 1); }
  memcpy(dst, start, n);
  dst[n] = '\0';
}

uint8_t DataSlots_parse(const char *data, DataSlot *slots, uint8_t maxSlots) {
  uint8_t count = 0;
  if (!data) { return 0; }
  const char *p = data;
  while (*p && count < maxSlots) {
    const char *e = strchr(p, DATA_SLOT_DELIM);
    if (!e) { break; }
    char widBuf[6];
    copy_field(widBuf, sizeof(widBuf), p, e);
    p = e + 1;
    e = strchr(p, DATA_SLOT_DELIM);
    if (!e) { break; }
    DataSlot *s = &slots[count];
    s->wid = (uint8_t)atoi(widBuf);
    copy_field(s->label, DATA_SLOT_LABEL_LEN, p, e);
    p = e + 1;
    e = strchr(p, DATA_SLOT_DELIM);
    const char *valEnd = e ? e : (p + strlen(p));
    copy_field(s->value, DATA_SLOT_VALUE_LEN, p, valEnd);
    s->valid = (strcmp(s->value, "--") != 0);
    count++;
    if (!e) { break; }
    p = e + 1;
  }
  return count;
}

DataSlot *DataSlots_find(DataSlot *slots, uint8_t count, uint8_t wid) {
  for (uint8_t i = 0; i < count; i++) {
    if (slots[i].wid == wid) { return &slots[i]; }
  }
  return NULL;
}

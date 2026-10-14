// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include <pebble.h>
#include "tuya.h"

CryptoSlot Tuya_slots[MAX_TUYA];
uint8_t    Tuya_count = 0;

bool Tuya_isWid(uint8_t wid) {
  return wid >= TUYA_WID_BASE && wid < TUYA_WID_BASE + MAX_TUYA;
}

void Tuya_parse(const char *data) { Tuya_count = DataSlots_parse(data, Tuya_slots, MAX_TUYA); }
CryptoSlot *Tuya_find(uint8_t wid) { return DataSlots_find(Tuya_slots, Tuya_count, wid); }

void Tuya_init() {
  Tuya_count = 0;
  if (persist_exists(TUYA_PERSIST_KEY_DATA)) {
    char buf[256];
    int n = persist_read_string(TUYA_PERSIST_KEY_DATA, buf, sizeof(buf));
    if (n > 0) { Tuya_count = DataSlots_parse(buf, Tuya_slots, MAX_TUYA); }
  }
}

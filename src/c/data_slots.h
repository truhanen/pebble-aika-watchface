// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <stdint.h>
#include <stdbool.h>

// Shared wid/label/value slot table for the phone-data widgets (crypto, currency,
// tuya). Pebble-free so it host-compiles for tests/test_data_slots.c.
#define DATA_SLOT_LABEL_LEN 8    // up to 7 chars + NUL (fits "EUR/USD")
#define DATA_SLOT_VALUE_LEN 16   // formatted value + NUL

typedef struct {
  uint8_t wid;
  char    label[DATA_SLOT_LABEL_LEN];
  char    value[DATA_SLOT_VALUE_LEN];
  bool    valid;                 // false when the value is "--"
} DataSlot;

// Parse "wid<US>label<US>value<US>..." (US = 0x1f) into slots[]; returns count (<= maxSlots).
uint8_t DataSlots_parse(const char *data, DataSlot *slots, uint8_t maxSlots);

// Linear lookup by wid, or NULL.
DataSlot *DataSlots_find(DataSlot *slots, uint8_t count, uint8_t wid);

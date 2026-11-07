// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <stdint.h>
#include <stdbool.h>

// Value/unit slot table for the plugin-sourced widgets (ids 144..159). Pebble-free so
// it host-compiles for tests/test_plugin_slots.c; plugin.c adds persistence.
// Wire format (src/ts/plugins_parse.ts): "slot<US>value<US>unit<US>slot..." (US =
// 0x1f), three fields per record.
#define PLUGIN_SLOT_COUNT 16
#define PLUGIN_VALUE_LEN  12   // up to 11 bytes + NUL
#define PLUGIN_UNIT_LEN   12   // up to 11 bytes + NUL (UTF-8, e.g. "\xc2\xb0" "C")

typedef struct {
  char    value[PLUGIN_VALUE_LEN];
  char    unit[PLUGIN_UNIT_LEN];
  uint8_t has;                 // 1 once the phone has sent this slot (even as "--")
} PluginSlot;

// Apply the records in `data` to slots[PLUGIN_SLOT_COUNT] IN PLACE (slots the message
// does not mention keep their data; a slot index >= PLUGIN_SLOT_COUNT is skipped; a
// truncated trailing record is dropped). Returns the number of records applied.
uint8_t PluginSlots_parse(const char *data, PluginSlot *slots);

// The slot with data, or NULL (index out of range or never received).
PluginSlot *PluginSlots_get(PluginSlot *slots, uint8_t index);

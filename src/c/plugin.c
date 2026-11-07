// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include <pebble.h>
#include <string.h>
#include "plugin.h"

#define HALF (PLUGIN_SLOT_COUNT / 2)

_Static_assert(sizeof(PluginSlot) * HALF <= PERSIST_DATA_MAX_LENGTH,
               "plugin slot half does not fit one persist value");

PluginSlot Plugin_slots[PLUGIN_SLOT_COUNT];

bool Plugin_isWid(uint8_t wid) {
  return wid >= PLUGIN_WID_BASE && wid < PLUGIN_WID_BASE + PLUGIN_SLOT_COUNT;
}

static void save(void) {
  persist_write_data(PLUGIN_PERSIST_KEY_LO, &Plugin_slots[0], sizeof(PluginSlot) * HALF);
  persist_write_data(PLUGIN_PERSIST_KEY_HI, &Plugin_slots[HALF], sizeof(PluginSlot) * HALF);
}

void Plugin_parse(const char *data) {
  if (PluginSlots_parse(data, Plugin_slots) > 0) { save(); }
}

PluginSlot *Plugin_find(uint8_t wid) {
  return Plugin_isWid(wid) ? PluginSlots_get(Plugin_slots, (uint8_t)(wid - PLUGIN_WID_BASE)) : NULL;
}

void Plugin_init() {
  memset(Plugin_slots, 0, sizeof(Plugin_slots));
  if (persist_exists(PLUGIN_PERSIST_KEY_LO)) {
    persist_read_data(PLUGIN_PERSIST_KEY_LO, &Plugin_slots[0], sizeof(PluginSlot) * HALF);
  }
  if (persist_exists(PLUGIN_PERSIST_KEY_HI)) {
    persist_read_data(PLUGIN_PERSIST_KEY_HI, &Plugin_slots[HALF], sizeof(PluginSlot) * HALF);
  }
  for (int i = 0; i < PLUGIN_SLOT_COUNT; i++) {   // a corrupt blob must not leave unterminated text
    Plugin_slots[i].value[PLUGIN_VALUE_LEN - 1] = '\0';
    Plugin_slots[i].unit[PLUGIN_UNIT_LEN - 1] = '\0';
  }
}

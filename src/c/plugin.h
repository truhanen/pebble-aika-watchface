// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <pebble.h>
#include "plugin_slots.h"

// The slot table is persisted in two blobs (slots 0..7 and 8..15; one persist value is
// capped at 256 bytes). 310-318 are crypto/currency/tuya/etc., 319 is the poll state,
// so 320/321 are free.
#define PLUGIN_PERSIST_KEY_LO 320
#define PLUGIN_PERSIST_KEY_HI 321

#define PLUGIN_WID_BASE 144       // widget ids [144, 144+PLUGIN_SLOT_COUNT) = 144-159

extern PluginSlot Plugin_slots[PLUGIN_SLOT_COUNT];

// True iff `wid` is a plugin widget id (144..159).
bool Plugin_isWid(uint8_t wid);

void        Plugin_init();                    // load the persisted slots
void        Plugin_parse(const char *data);   // apply a PluginData string + persist
PluginSlot *Plugin_find(uint8_t wid);         // slot with data for a wid, or NULL

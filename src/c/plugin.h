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

// Icon slots (kind PLUGIN_KIND_ICON): the bitmap arrives in its own message and is NOT
// persisted (16 x ~290 bytes would not fit the 4 KB persist store), so after a restart the
// icon is missing until the phone resends it: Plugin_find() still returns the slot (so the
// widget keeps its place) while Plugin_iconMissing() makes main.c ask for a cold refresh.
bool      Plugin_isIconWid(uint8_t wid);       // plugin widget whose slot is an icon slot
GBitmap  *Plugin_icon(uint8_t wid);            // its bitmap, or NULL when not (yet) received
bool      Plugin_iconMissing(uint8_t wid);     // icon slot without a bitmap
// Apply a PluginIcon* message: slot, size, palette (3 bytes), 4bpp pixels. Invalid -> ignored.
void      Plugin_setIcon(uint8_t slot, uint8_t w, uint8_t h, const uint8_t *palette, uint16_t paletteLen,
                         const uint8_t *pixels, uint16_t pixelLen);
// Draw the slot's bitmap at `origin` with black -> stroke, white -> fill (clear stays clear).
void      Plugin_drawIcon(GContext *g, uint8_t wid, GPoint origin, GColor stroke, GColor fill);
void      Plugin_deinit(void);

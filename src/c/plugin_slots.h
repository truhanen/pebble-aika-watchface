// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <stdint.h>
#include <stdbool.h>

// Value/unit slot table for the plugin-sourced widgets (ids 144..159). Pebble-free so
// it host-compiles for tests/test_plugin_slots.c; plugin.c adds persistence.
// Wire format (src/ts/plugins_parse.ts): "slot<US>value<US>unit<US>slot..." (US =
// 0x1f), three fields per record. A value of exactly "@" (PLUGIN_ICON_MARK) makes the slot
// an icon slot (kind PLUGIN_KIND_ICON); its bitmap arrives in a separate message.
#define PLUGIN_SLOT_COUNT 16
#define PLUGIN_VALUE_LEN  12   // up to 11 bytes + NUL
#define PLUGIN_UNIT_LEN   12   // up to 11 bytes + NUL (UTF-8, e.g. "\xc2\xb0" "C")

#define PLUGIN_KIND_NUMBER 0
#define PLUGIN_KIND_ICON   1
#define PLUGIN_ICON_MARK   "@"

typedef struct {
  char    value[PLUGIN_VALUE_LEN];
  char    unit[PLUGIN_UNIT_LEN];
  uint8_t has;                 // 1 once the phone has sent this slot (even as "--")
  uint8_t kind;                // PLUGIN_KIND_*
} PluginSlot;

// Apply the records in `data` to slots[PLUGIN_SLOT_COUNT] IN PLACE (slots the message
// does not mention keep their data; a slot index >= PLUGIN_SLOT_COUNT is skipped; a
// truncated trailing record is dropped). Returns the number of records applied.
uint8_t PluginSlots_parse(const char *data, PluginSlot *slots);

// The slot with data, or NULL (index out of range or never received).
PluginSlot *PluginSlots_get(PluginSlot *slots, uint8_t index);

/***** icons: 4bpp palette-indexed bitmaps (the plugin `icon` shape) *****/
// Largest icon the phone sends: iconPixelSize is WL_ICON_H (23) square, rows of ceil(w/2) bytes.
#define PLUGIN_ICON_MAX_DIM    23
#define PLUGIN_ICON_MAX_PIXELS (PLUGIN_ICON_MAX_DIM * ((PLUGIN_ICON_MAX_DIM + 1) / 2))   // 276
#define PLUGIN_ICON_PALETTE_LEN 3     // GColor8: black, white, clear (palette index 0, 1, 2)

// True iff a w x h icon with this palette / pixel byte count is acceptable: 1..23 each way,
// palette exactly 3 bytes, pixels exactly h * ceil(w/2) bytes.
bool PluginIcon_valid(uint8_t w, uint8_t h, uint16_t paletteLen, uint16_t pixelLen);

// Copy `h` source rows of ceil(w/2) bytes into a destination with `dstStride` bytes per row
// (>= ceil(w/2)); the padding of each destination row is zeroed.
void PluginIcon_copyRows(uint8_t *dst, uint16_t dstStride, const uint8_t *src, uint8_t w, uint8_t h);

// The colour of palette index `idx` (0 black, 1 white, 2 clear, anything else clear) as a
// GColor8 byte given the widget's stroke / fill colour bytes: black -> `stroke`,
// white -> `fill`, clear -> 0x00 (transparent).
uint8_t PluginIcon_colour(uint8_t idx, uint8_t stroke, uint8_t fill);

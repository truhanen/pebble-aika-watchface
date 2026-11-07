// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include <pebble.h>
#include <string.h>
#include <stdlib.h>
#include "plugin.h"

#define HALF (PLUGIN_SLOT_COUNT / 2)

_Static_assert(sizeof(PluginSlot) * HALF <= PERSIST_DATA_MAX_LENGTH,
               "plugin slot half does not fit one persist value");

PluginSlot Plugin_slots[PLUGIN_SLOT_COUNT];
static GBitmap *s_icons[PLUGIN_SLOT_COUNT];   // one blank 4bpp-palette bitmap per received icon

static void free_icon(uint8_t i) {
  if (s_icons[i]) { gbitmap_destroy(s_icons[i]); s_icons[i] = NULL; }   // frees its palette too
}

bool Plugin_isWid(uint8_t wid) {
  return wid >= PLUGIN_WID_BASE && wid < PLUGIN_WID_BASE + PLUGIN_SLOT_COUNT;
}

static void save(void) {
  persist_write_data(PLUGIN_PERSIST_KEY_LO, &Plugin_slots[0], sizeof(PluginSlot) * HALF);
  persist_write_data(PLUGIN_PERSIST_KEY_HI, &Plugin_slots[HALF], sizeof(PluginSlot) * HALF);
}

void Plugin_parse(const char *data) {
  if (PluginSlots_parse(data, Plugin_slots) > 0) {
    for (uint8_t i = 0; i < PLUGIN_SLOT_COUNT; i++) {   // a slot that is no longer an icon drops its bitmap
      if (Plugin_slots[i].kind != PLUGIN_KIND_ICON) { free_icon(i); }
    }
    save();
  }
}

bool Plugin_isIconWid(uint8_t wid) {
  return Plugin_isWid(wid) && Plugin_slots[wid - PLUGIN_WID_BASE].has
      && Plugin_slots[wid - PLUGIN_WID_BASE].kind == PLUGIN_KIND_ICON;
}

GBitmap *Plugin_icon(uint8_t wid) {
  return Plugin_isIconWid(wid) ? s_icons[wid - PLUGIN_WID_BASE] : NULL;
}

bool Plugin_iconMissing(uint8_t wid) { return Plugin_isIconWid(wid) && Plugin_icon(wid) == NULL; }

void Plugin_setIcon(uint8_t slot, uint8_t w, uint8_t h, const uint8_t *palette, uint16_t paletteLen,
                    const uint8_t *pixels, uint16_t pixelLen) {
  if (slot >= PLUGIN_SLOT_COUNT || !Plugin_isIconWid((uint8_t)(PLUGIN_WID_BASE + slot)) || !palette || !pixels
      || !PluginIcon_valid(w, h, paletteLen, pixelLen)) { return; }
  GColor *pal = (GColor *)malloc(16 * sizeof(GColor));   // 4bpp palette: 16 entries
  if (!pal) { return; }
  memset(pal, 0, 16 * sizeof(GColor));
  GBitmap *bmp = gbitmap_create_blank_with_palette(GSize(w, h), GBitmapFormat4BitPalette, pal, true);
  if (!bmp) { free(pal); return; }
  PluginIcon_copyRows(gbitmap_get_data(bmp), gbitmap_get_bytes_per_row(bmp), pixels, w, h);
  for (uint8_t i = 0; i < PLUGIN_ICON_PALETTE_LEN; i++) { pal[i].argb = palette[i]; }   // tinted at draw time
  free_icon(slot);
  s_icons[slot] = bmp;
}

void Plugin_drawIcon(GContext *g, uint8_t wid, GPoint origin, GColor stroke, GColor fill) {
  GBitmap *bmp = Plugin_icon(wid);
  if (!bmp) { return; }
  GColor *pal = gbitmap_get_palette(bmp);
  for (uint8_t i = 0; i < PLUGIN_ICON_PALETTE_LEN; i++) { pal[i].argb = PluginIcon_colour(i, stroke.argb, fill.argb); }
  GRect b = gbitmap_get_bounds(bmp);
  graphics_context_set_compositing_mode(g, GCompOpSet);
  graphics_draw_bitmap_in_rect(g, bmp, GRect(origin.x, origin.y, b.size.w, b.size.h));
}

void Plugin_deinit(void) {
  for (uint8_t i = 0; i < PLUGIN_SLOT_COUNT; i++) { free_icon(i); }
}

PluginSlot *Plugin_find(uint8_t wid) {
  return Plugin_isWid(wid) ? PluginSlots_get(Plugin_slots, (uint8_t)(wid - PLUGIN_WID_BASE)) : NULL;
}

void Plugin_init() {
  memset(Plugin_slots, 0, sizeof(Plugin_slots));
  // A blob of another size (an older build's slot layout) is dropped, not misread.
  const size_t half = sizeof(PluginSlot) * HALF;
  if (persist_exists(PLUGIN_PERSIST_KEY_LO)
      && (persist_get_size(PLUGIN_PERSIST_KEY_LO) != (int)half
          || persist_read_data(PLUGIN_PERSIST_KEY_LO, &Plugin_slots[0], half) != (int)half)) {
    memset(&Plugin_slots[0], 0, half);
  }
  if (persist_exists(PLUGIN_PERSIST_KEY_HI)
      && (persist_get_size(PLUGIN_PERSIST_KEY_HI) != (int)half
          || persist_read_data(PLUGIN_PERSIST_KEY_HI, &Plugin_slots[HALF], half) != (int)half)) {
    memset(&Plugin_slots[HALF], 0, half);
  }
  for (int i = 0; i < PLUGIN_SLOT_COUNT; i++) {   // a corrupt blob must not leave unterminated text
    if (Plugin_slots[i].kind > PLUGIN_KIND_ICON) { Plugin_slots[i].kind = PLUGIN_KIND_NUMBER; }
    Plugin_slots[i].value[PLUGIN_VALUE_LEN - 1] = '\0';
    Plugin_slots[i].unit[PLUGIN_UNIT_LEN - 1] = '\0';
  }
}

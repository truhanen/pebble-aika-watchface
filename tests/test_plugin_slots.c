// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
// gcc -std=c11 -Wall -I src/c -o /tmp/t tests/test_plugin_slots.c src/c/plugin_slots.c
#include "plugin_slots.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define US "\x1f"

int main(void) {
  PluginSlot slots[PLUGIN_SLOT_COUNT];
  memset(slots, 0, sizeof(slots));

  // two records; the second has an empty unit
  assert(PluginSlots_parse("0" US "18.3" US "\xc2\xb0" "C" US "2" US "--" US "", slots) == 2);
  assert(PluginSlots_get(slots, 0) == &slots[0]);
  assert(strcmp(slots[0].value, "18.3") == 0);
  assert(strcmp(slots[0].unit, "\xc2\xb0" "C") == 0);
  assert(strcmp(slots[2].value, "--") == 0);
  assert(slots[2].unit[0] == '\0');
  assert(PluginSlots_get(slots, 1) == NULL);          // never received
  assert(PluginSlots_get(slots, 16) == NULL);         // out of range

  // unused tail bytes are zero, so persisted blobs are deterministic
  {
    PluginSlot dirty[PLUGIN_SLOT_COUNT];
    memset(dirty, 0xAA, sizeof(dirty));
    assert(PluginSlots_parse("3" US "18.3" US "C", dirty) == 1);
    for (int i = 5; i < PLUGIN_VALUE_LEN; i++) { assert(dirty[3].value[i] == 0); }
    for (int i = 2; i < PLUGIN_UNIT_LEN; i++) { assert(dirty[3].unit[i] == 0); }
  }

  // a later message updates only the slots it mentions
  assert(PluginSlots_parse("0" US "-4" US "\xc2\xb0" "C", slots) == 1);
  assert(strcmp(slots[0].value, "-4") == 0);
  assert(strcmp(slots[2].value, "--") == 0);
  assert(PluginSlots_get(slots, 2) != NULL);

  // a full 16-record message, as the phone sends it
  {
    char msg[512] = "";
    for (int i = 0; i < 16; i++) {
      char rec[40];
      snprintf(rec, sizeof(rec), "%s%d" US "%d" US "u%d", i ? US : "", i, i * 10, i);
      strcat(msg, rec);
    }
    assert(PluginSlots_parse(msg, slots) == 16);
    assert(strcmp(slots[15].value, "150") == 0);
    assert(strcmp(slots[15].unit, "u15") == 0);
    assert(strcmp(slots[7].value, "70") == 0);
  }

  // out-of-range slot index skipped; truncated trailing record dropped
  assert(PluginSlots_parse("16" US "1" US "x", slots) == 0);
  assert(PluginSlots_parse("3" US "9" US "m" US "4" US "5", slots) == 1);   // slot 4 lacks its unit field
  assert(strcmp(slots[3].value, "9") == 0);
  assert(strcmp(slots[4].value, "40") == 0);          // untouched by the truncated record
  assert(PluginSlots_parse("5", slots) == 0);
  assert(PluginSlots_parse("", slots) == 0);
  assert(PluginSlots_parse(NULL, slots) == 0);

  // over-long fields are cut to the buffers, never in the middle of a UTF-8 character
  PluginSlots_parse("6" US "123456789012345" US "\xc2\xb0\xc2\xb0\xc2\xb0\xc2\xb0\xc2\xb0\xc2\xb0", slots);
  assert(strlen(slots[6].value) == PLUGIN_VALUE_LEN - 1);
  assert(strcmp(slots[6].unit, "\xc2\xb0\xc2\xb0\xc2\xb0\xc2\xb0\xc2\xb0") == 0);   // 10 of 11 bytes: the 6th would split

  // the icon marker makes a slot an icon slot; a number makes it a number slot again
  PluginSlots_parse("7" US "@" US "", slots);
  assert(slots[7].has && slots[7].kind == PLUGIN_KIND_ICON);
  assert(slots[3].kind == PLUGIN_KIND_NUMBER);
  PluginSlots_parse("7" US "18" US "C", slots);
  assert(slots[7].kind == PLUGIN_KIND_NUMBER);
  PluginSlots_parse("7" US "@x" US "", slots);
  assert(slots[7].kind == PLUGIN_KIND_NUMBER);

  // icon validation: 1..23 square-ish, palette 3 bytes, pixels h * ceil(w/2)
  assert(PLUGIN_ICON_MAX_PIXELS == 276);
  assert(PluginIcon_valid(23, 23, 3, 276));
  assert(PluginIcon_valid(5, 4, 3, 12));
  assert(!PluginIcon_valid(23, 23, 3, 275));
  assert(!PluginIcon_valid(23, 23, 2, 276));
  assert(!PluginIcon_valid(24, 23, 3, 276));
  assert(!PluginIcon_valid(0, 5, 3, 0));
  assert(!PluginIcon_valid(5, 24, 3, 288));

  // rows are copied into a (possibly wider) destination stride, padding zeroed
  {
    uint8_t src[2 * 3] = { 1, 2, 3, 4, 5, 6 };            // w=5 -> 3 bytes per row, h=2
    uint8_t dst[2 * 4];
    memset(dst, 0xee, sizeof(dst));
    PluginIcon_copyRows(dst, 4, src, 5, 2);
    uint8_t want[8] = { 1, 2, 3, 0, 4, 5, 6, 0 };
    assert(memcmp(dst, want, 8) == 0);
    uint8_t tight[6];
    PluginIcon_copyRows(tight, 3, src, 5, 2);
    assert(memcmp(tight, src, 6) == 0);
  }

  // palette: black -> stroke, white -> fill, clear stays clear
  assert(PluginIcon_colour(0, 0xc0, 0xff) == 0xc0);
  assert(PluginIcon_colour(1, 0xc0, 0xf0) == 0xf0);
  assert(PluginIcon_colour(2, 0xc0, 0xff) == 0x00);
  assert(PluginIcon_colour(9, 0xc0, 0xff) == 0x00);

  printf("PASS\n");
  return 0;
}

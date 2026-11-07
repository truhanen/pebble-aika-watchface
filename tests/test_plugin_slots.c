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

  printf("PASS\n");
  return 0;
}

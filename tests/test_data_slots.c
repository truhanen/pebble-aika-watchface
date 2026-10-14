// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#include "data_slots.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  DataSlot slots[4];
  uint8_t n = DataSlots_parse("128\x1f" "Sauna\x1f" "23.5\x1f" "129\x1f" "H\x1f" "--", slots, 4);
  assert(n == 2);
  assert(slots[0].wid == 128);
  assert(strcmp(slots[0].label, "Sauna") == 0);
  assert(strcmp(slots[0].value, "23.5") == 0);
  assert(slots[0].valid);
  assert(slots[1].wid == 129);
  assert(strcmp(slots[1].value, "--") == 0);
  assert(slots[1].valid == false);            // "--" -> invalid
  assert(DataSlots_find(slots, n, 129) == &slots[1]);
  assert(DataSlots_find(slots, n, 200) == NULL);
  assert(DataSlots_parse(NULL, slots, 4) == 0);
  assert(DataSlots_parse("1\x1f" "A\x1f" "2\x1f" "2\x1f" "B\x1f" "3", slots, 1) == 1); // maxSlots cap
  printf("PASS\n");
  return 0;
}

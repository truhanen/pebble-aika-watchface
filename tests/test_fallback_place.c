// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#include "widget_list.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  int idx; bool app;

  // Replace: position within a fully-visible list -> overwrite that slot.
  WidgetList_fallbackPlace(3, 3, /*appendFits=*/true, /*position=*/2, &idx, &app);
  assert(idx == 1 && app == false);          // 2nd widget (index 1), replaced

  // Replace the first widget.
  WidgetList_fallbackPlace(3, 3, true, 1, &idx, &app);
  assert(idx == 0 && app == false);

  // Append: position == count+1 and there is room -> add a new slot at index count.
  WidgetList_fallbackPlace(3, 3, true, 4, &idx, &app);
  assert(idx == 3 && app == true);

  // Append requested but no room (appendFits=false) -> replace last visible.
  WidgetList_fallbackPlace(3, 3, false, 4, &idx, &app);
  assert(idx == 2 && app == false);

  // Position past count+1 clamps to append.
  WidgetList_fallbackPlace(3, 3, true, 9, &idx, &app);
  assert(idx == 3 && app == true);

  // Position below 1 clamps to 1 (replace first).
  WidgetList_fallbackPlace(3, 3, true, 0, &idx, &app);
  assert(idx == 0 && app == false);

  // Empty column: always append the fallback as the sole slot.
  WidgetList_fallbackPlace(0, 0, true, 1, &idx, &app);
  assert(idx == 0 && app == true);
  WidgetList_fallbackPlace(0, 0, true, 5, &idx, &app);   // position clamps to 1
  assert(idx == 0 && app == true);

  // Replace target beyond the visible count -> fall back to last VISIBLE slot
  // (so a low-battery / BT-lost warning is never hidden below the fold).
  WidgetList_fallbackPlace(5, 2, false, 4, &idx, &app);  // want slot 3, only 2 visible
  assert(idx == 1 && app == false);                      // last visible (index 1)

  // Append requested, list overflows (visibleCount < count) -> last visible.
  WidgetList_fallbackPlace(5, 2, false, 6, &idx, &app);
  assert(idx == 1 && app == false);

  printf("All fallback_place tests passed\n");
  return 0;
}

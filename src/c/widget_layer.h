#pragma once
// Widgets drawn on the clock face layer, in two passes around the hands: the icons
// and units below them, the values (with their shadow) above them together with the
// icon-only widgets (Widgets_iconOnTop; icon with a shadow). One frame is
// resolved and laid out once (WidgetLayer_prepare), then drawn by both passes.
#include <pebble.h>

void WidgetLayer_init(void);
void WidgetLayer_deinit(void);
// Re-parse the three widget lists; call after settings.*WidgetList changed.
void WidgetLayer_reload(void);
// Resolve the active member of every slot (rotation, auto-battery / BT fallback) and
// place everything for a face of the given bounds.
void WidgetLayer_prepare(GRect bounds);
void WidgetLayer_drawIcons(GContext *ctx);
void WidgetLayer_drawValues(GContext *ctx);

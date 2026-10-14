// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Whether the UV-index widget (id 13) is currently placed in any of the four widget
   lists (left / right edge, top / bottom centre). Read straight from Clay's persisted `clay-settings` (the same store the
   widgetList migration reads), so it always reflects the user's current layout
   without a separate flag to keep in sync. Used by the FMI weather provider to
   decide whether to make the extra radiation-observation request for UV -- FMI's
   forecast carries no UV, so we only pay for the UV obs query when the widget that
   shows it is actually installed. */

import { widgetListIds } from './widget_list_payload';

export const UV_WIDGET_ID = 13;   // matches widget_options.ts / widgets.h

interface MinimalStorage {
  getItem(key: string): string | null;
}

export function isUvWidgetConfigured(storage: MinimalStorage): boolean {
  let stored: Record<string, any>;
  try {
    stored = JSON.parse(storage.getItem('clay-settings') || '{}') || {};
  } catch (e) {
    return false;
  }
  const ids = widgetListIds(stored.WidgetList, 'side')
    .concat(widgetListIds(stored.WidgetListRight, 'side'))
    .concat(widgetListIds(stored.WidgetListTop, 'center'))
    .concat(widgetListIds(stored.WidgetListBottom, 'center'));
  return ids.indexOf(UV_WIDGET_ID) !== -1;
}

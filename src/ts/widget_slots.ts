// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// The 6 legacy per-slot setting keys, in slot order. Mirrors the order used by
// the old 6-dropdown config and the watch C side.
const WIDGET_SLOT_KEYS = ['SettingWidget0ID', 'SettingWidget1ID', 'SettingWidget2ID',
  'SettingWidget2_0ID', 'SettingWidget2_1ID', 'SettingWidget2_2ID'];

// One-time migration from the 6 legacy per-slot settings (saved by the old
// 6-dropdown config) to the single `WidgetList` array.
// Given the flattened clay-settings object, returns the ordered id array with
// trailing Empty(0) slots trimmed, or null if NONE of the legacy slot keys are
// present (so the caller leaves WidgetList untouched and the component falls back
// to its defaultValue).
export function slotsToList(settings: Record<string, any>): number[] | null {
  let hasAny = false;
  const ids: number[] = [];
  for (let i = 0; i < WIDGET_SLOT_KEYS.length; i++) {
    const raw = settings[WIDGET_SLOT_KEYS[i]];
    if (raw !== undefined && raw !== null && raw !== '') { hasAny = true; }
    ids.push(parseInt(raw, 10) || 0);
  }
  if (!hasAny) { return null; }
  while (ids.length > 0 && ids[ids.length - 1] === 0) { ids.pop(); }
  return ids;
}

// Split a single ordered widget list onto the new left/right columns based on the
// legacy `SettingSidebarOnLeft` value ('1' = the old single sidebar was on the
// left). The list goes entirely to the side it used to render on; the other side
// starts empty. Used for the one-time config-side dual-list migration.
export function splitListByPosition(
  list: number[], sidebarOnLeftRaw: any): { left: number[]; right: number[] } {
  const onLeft = (sidebarOnLeftRaw === undefined || sidebarOnLeftRaw === null)
    ? true : (parseInt(sidebarOnLeftRaw, 10) !== 0);
  const arr = Array.isArray(list) ? list.slice() : [];
  return onLeft ? { left: arr, right: [] } : { left: [], right: arr };
}

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// Converts the `widgetList` Clay value (a flat marker-encoded array) into the byte
// payload sent under SettingWidgetList / SettingRightWidgetList. A slot is either a
// plain widget id or a rotating group: 255, count, interval_code, members...
// Drops non-drawable ids/members, clamps count(<=6)/interval(0..5), degrades a
// <2-member group, packs WHOLE groups only, and stops before exceeding 16 bytes.
// KEEP IN SYNC with src/c/widget_list.c.
const MAX_WIDGET_LIST_BYTES = 32;   // matches MAX_WIDGET_LIST in src/c/sidebar_widgets.h
const MAX_WIDGET_TYPE = 25;         // SLEEP_DEEP_BAR
const CRYPTO_WID_BASE = 200;
const MAX_CRYPTO = 16;
const CURRENCY_WID_BASE = 216;
const MAX_CURRENCY = 7;
const TUYA_WID_BASE = 128;
const MAX_TUYA = 16;
const ROTATING_MARKER = 255;
const MAX_GROUP_MEMBERS = 6;
const WIDGET_HIDE_FLAG = 0x20;   // KEEP IN SYNC with src/c/widget_list.h

function isDrawableId(id: number): boolean {
  const base = id & ~WIDGET_HIDE_FLAG;
  if (base === 0) { return false; }
  if (base >= 1 && base <= MAX_WIDGET_TYPE) { return true; }
  if (base >= CRYPTO_WID_BASE && base < CRYPTO_WID_BASE + MAX_CRYPTO) { return true; }
  if (base >= CURRENCY_WID_BASE && base < CURRENCY_WID_BASE + MAX_CURRENCY) { return true; }
  if (base >= TUYA_WID_BASE && base < TUYA_WID_BASE + MAX_TUYA) { return true; }
  return false;
}

export function widgetListToPayload(list: any): number[] {
  const arr: any[] = Array.isArray(list) ? list : [];
  const out: number[] = [];
  let i = 0;
  while (i < arr.length && out.length < MAX_WIDGET_LIST_BYTES) {
    const head = parseInt(arr[i], 10);
    if (head === ROTATING_MARKER) {
      const count = parseInt(arr[i + 1], 10);
      const intervalRaw = parseInt(arr[i + 2], 10);
      if (isNaN(count) || isNaN(intervalRaw)) { break; }   // malformed tail
      const memStart = i + 3;
      const members: number[] = [];
      for (let m = 0; m < count && m < MAX_GROUP_MEMBERS; m++) {
        const id = parseInt(arr[memStart + m], 10);
        if (!isNaN(id) && isDrawableId(id)) { members.push(id); }
      }
      i = memStart + count;
      const interval = (intervalRaw >= 0 && intervalRaw <= 5) ? intervalRaw : 3;
      if (members.length >= 2) {
        if (out.length + 3 + members.length > MAX_WIDGET_LIST_BYTES) { break; } // whole group only
        out.push(ROTATING_MARKER, members.length, interval);
        for (let m = 0; m < members.length; m++) { out.push(members[m]); }
      } else if (members.length === 1) {
        out.push(members[0]);
      }
    } else {
      if (!isNaN(head) && isDrawableId(head)) { out.push(head); }
      i += 1;
    }
  }
  return out;
}

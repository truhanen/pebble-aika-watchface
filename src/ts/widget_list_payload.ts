// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// Converts the `widgetList` Clay value into the v2 byte payload sent under
// SettingWidgetList / SettingRightWidgetList / SettingCenterWidgetList. See docs/widget_contract.md (the binding contract):
//   plain widget   : id, flags
//   rotating group : 0xFF, count (2..6), interval_code (0..5), (id, flags) x count
//   flags          : bits 0-4 font index (0..16, 31 = position default),
//                    bit 5 no identifier ('none'), bit 6 the unit ('unit'; neither bit =
//                    the icon), bit 7 reserved (0).
// The Clay value is an array of slot objects:
//   { id, ident: 'icon'|'unit'|'none', font }           plain slot
//   { rotating: true, interval, members: [{id, ident, font}, ...] }
// Values stored by older versions carry `hide: boolean` instead of `ident`
// (hide -> 'none', else 'icon').
// For values stored by older versions a flat marker-encoded int array is still
// accepted (255, count, interval, members...; the hide flag as 0x20 OR-ed into the
// id; no font -> position default).
// Invalid ids/members are dropped, a font outside 0..16 falls back to the default,
// count/interval are clamped, a group with one member degrades to a plain slot, WHOLE
// groups only are packed, and the list stops before exceeding its byte cap (64, all
// three lists).
// KEEP IN SYNC with src/c/widget_list.c.
export const MAX_LIST_BYTES = 64;
const MAX_WIDGET_TYPE = 38;         // BLUETOOTH_ICON
const CRYPTO_WID_BASE = 200;
const MAX_CRYPTO = 16;
const CURRENCY_WID_BASE = 216;
const MAX_CURRENCY = 7;
const TUYA_WID_BASE = 128;
const MAX_TUYA = 16;
const ROTATING_MARKER = 255;
const MAX_GROUP_MEMBERS = 6;
const DEFAULT_INTERVAL = 3;         // 1 min
const FONT_DEFAULT = 31;
const MAX_FONT = 16;
const FLAG_NONE = 0x20;
const FLAG_UNIT = 0x40;
const LEGACY_HIDE_FLAG = 0x20;      // old encoding: hide flag OR-ed into the id
const LEGACY_MAX_TYPE = 25;

export type WidgetIdent = 'icon' | 'unit' | 'none';
interface Entry { id: number; ident: WidgetIdent; font: number; }
interface Slot { members: Entry[]; interval: number; group: boolean; }

// Offerable ids plus the legacy crypto ids 15/16/17. 1 (BT, internal) and 4 (retired
// date) are never sent.
function isValidId(id: number): boolean {
  if (id === 0) { return true; }
  if (id === 2 || id === 3) { return true; }
  if (id >= 5 && id <= MAX_WIDGET_TYPE) { return true; }
  if (id >= CRYPTO_WID_BASE && id < CRYPTO_WID_BASE + MAX_CRYPTO) { return true; }
  if (id >= CURRENCY_WID_BASE && id < CURRENCY_WID_BASE + MAX_CURRENCY) { return true; }
  if (id >= TUYA_WID_BASE && id < TUYA_WID_BASE + MAX_TUYA) { return true; }
  return false;
}

function fontOf(raw: any): number {
  const f = parseInt(raw, 10);
  return (!isNaN(f) && ((f >= 0 && f <= MAX_FONT) || f === FONT_DEFAULT)) ? f : FONT_DEFAULT;
}

function intervalOf(raw: any): number {
  const n = parseInt(raw, 10);
  return (!isNaN(n) && n >= 0 && n <= 5) ? n : DEFAULT_INTERVAL;
}

// The identifier mode of a stored entry: `ident` when valid, else the older `hide`.
function identOf(o: any): WidgetIdent {
  if (o && (o.ident === 'icon' || o.ident === 'unit' || o.ident === 'none')) { return o.ident; }
  return (o && o.hide) ? 'none' : 'icon';
}

function entryFromObject(o: any): Entry | null {
  if (!o || typeof o !== 'object') { return null; }
  const id = parseInt(o.id, 10);
  if (isNaN(id) || !isValidId(id)) { return null; }
  return { id: id, ident: identOf(o), font: fontOf(o.font) };
}

// Legacy flat value: an id possibly carrying the 0x20 hide flag (ids 33..57 can only
// be a hidden id 1..25 there, since ids 26+ did not exist yet).
function entryFromLegacy(v: number): Entry | null {
  let id = v;
  let ident: WidgetIdent = 'icon';
  if (v > LEGACY_HIDE_FLAG && v <= (LEGACY_HIDE_FLAG | LEGACY_MAX_TYPE)) {
    id = v - LEGACY_HIDE_FLAG;
    ident = 'none';
  }
  if (!isValidId(id)) { return null; }
  return { id: id, ident: ident, font: FONT_DEFAULT };
}

function memberEntry(m: any): Entry | null {
  if (m && typeof m === 'object') { return entryFromObject(m); }
  const v = parseInt(m, 10);
  return isNaN(v) ? null : entryFromLegacy(v);
}

function parseSlots(list: any): Slot[] {
  const arr: any[] = Array.isArray(list) ? list : [];
  const slots: Slot[] = [];
  let i = 0;
  while (i < arr.length) {
    const el = arr[i];
    if (el && typeof el === 'object') {
      if (el.rotating) {
        const mems: Entry[] = [];
        const src: any[] = Array.isArray(el.members) ? el.members : [];
        for (let m = 0; m < src.length && m < MAX_GROUP_MEMBERS; m++) {
          const e = memberEntry(src[m]);
          if (e && e.id !== 0) { mems.push(e); }
        }
        slots.push({ members: mems, interval: intervalOf(el.interval), group: true });
      } else {
        const e = entryFromObject(el);
        if (e) { slots.push({ members: [e], interval: DEFAULT_INTERVAL, group: false }); }
      }
      i += 1;
      continue;
    }
    const head = parseInt(el, 10);
    if (head === ROTATING_MARKER) {
      const count = parseInt(arr[i + 1], 10);
      const intervalRaw = parseInt(arr[i + 2], 10);
      if (isNaN(count) || isNaN(intervalRaw)) { break; }   // malformed tail
      const memStart = i + 3;
      const mems: Entry[] = [];
      for (let m = 0; m < count && m < MAX_GROUP_MEMBERS; m++) {
        const e = memberEntry(arr[memStart + m]);
        if (e && e.id !== 0) { mems.push(e); }
      }
      i = memStart + count;
      slots.push({ members: mems, interval: intervalOf(intervalRaw), group: true });
    } else {
      const e = isNaN(head) ? null : entryFromLegacy(head);
      if (e) { slots.push({ members: [e], interval: DEFAULT_INTERVAL, group: false }); }
      i += 1;
    }
  }
  return slots;
}

function flagsOf(e: Entry): number {
  return e.font | (e.ident === 'none' ? FLAG_NONE : (e.ident === 'unit' ? FLAG_UNIT : 0));
}

export function widgetListToPayload(list: any): number[] {
  const cap = MAX_LIST_BYTES;
  const slots = parseSlots(list);
  const out: number[] = [];
  for (let s = 0; s < slots.length; s++) {
    const slot = slots[s];
    const n = slot.members.length;
    if (n === 0) { continue; }
    if (slot.group && n >= 2) {
      if (out.length + 3 + 2 * n > cap) { break; }   // whole group only
      out.push(ROTATING_MARKER, n, slot.interval);
      for (let m = 0; m < n; m++) { out.push(slot.members[m].id, flagsOf(slot.members[m])); }
    } else {
      if (out.length + 2 > cap) { break; }
      out.push(slot.members[0].id, flagsOf(slot.members[0]));
    }
  }
  return out;
}

// Plain widget ids (flags stripped, group markers removed) of a list, for the
// phone-side "which data sources are placed" decisions.
export function widgetListIds(list: any): number[] {
  const p = widgetListToPayload(list);
  const ids: number[] = [];
  let i = 0;
  while (i < p.length) {
    if (p[i] === ROTATING_MARKER) {
      const n = p[i + 1];
      for (let m = 0; m < n; m++) { ids.push(p[i + 3 + 2 * m]); }
      i += 3 + 2 * n;
    } else {
      ids.push(p[i]);
      i += 2;
    }
  }
  return ids;
}

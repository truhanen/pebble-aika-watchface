// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Pure helpers for the plugin-sourced widgets (ids 144..159): selection parsing,
   instance picking, value formatting and the PluginData wire string. No Pebble
   globals, so unit-testable with `node --test` (tests/plugins_parse.test.js).
   Wire format (KEEP IN SYNC with src/c/plugin_slots.c): PluginData is a string of
   `slot<US>value<US>unit` records, US = 0x1f, three fields per record, slot 0..15
   (widget id = 144 + slot). An icon slot's value is ICON_MARK ("@"); its bitmap follows in
   one AppMessage per slot (buildIconMessage: PluginIconSlot / Width / Height / Palette /
   Pixels, byte arrays), so it never competes with PluginData. */

export const MAX_PLUGINS = 16;       // matches PLUGIN_SLOT_COUNT in src/c/plugin_slots.h
export const PLUGIN_WID_BASE = 144;  // matches WL_PLUGIN_WID_BASE in src/c/widget_list.h
export const NO_DATA = '--';
export const ICON_MARK = '@';        // PluginData value of a slot that shows a bitmap
export const ICON_PIXEL_SIZE = 23;   // = WL_ICON_H: the size requested via iconPixelSize
const MAX_ICON_DIM = 23;             // matches PLUGIN_ICON_MAX_DIM in src/c/plugin_slots.h
const MAX_VALUE_BYTES = 11;          // watch buffers are 12 incl. NUL
const MAX_UNIT_BYTES = 11;
const DELIM = '\x1f';

export interface PluginSelection {
  plugin?: string;        // plugin uuid; absent = any plugin offering the source
  category: string;
  item: string;
  instanceId?: string;    // instance UUID (or its 1-based number); absent = first
  property: string;
  shape?: 'icon';         // absent = numericValue; 'icon' = the 3-colour bitmap shape
}

// A decoded icon: 4bpp palette-indexed pixels (rows of ceil(w/2) bytes) + 3 palette bytes
// (GColor8: black, white, clear).
export interface PluginIcon { width: number; height: number; palette: number[]; pixels: number[]; }

export interface PluginResult { slot: number; value: string; unit: string; icon?: PluginIcon; }

function str(v: any): string | undefined {
  return (typeof v === 'string' && v !== '') ? v : undefined;
}

// One stored selection -> a clean PluginSelection, or null when unusable.
export function normalizeSelection(raw: any): PluginSelection | null {
  if (!raw || typeof raw !== 'object') { return null; }
  const category = str(raw.category), item = str(raw.item), property = str(raw.property);
  if (!category || !item || !property) { return null; }
  const sel: PluginSelection = { category: category, item: item, property: property };
  const plugin = str(raw.plugin);
  if (plugin) { sel.plugin = plugin; }
  const inst = (typeof raw.instanceId === 'number') ? String(raw.instanceId) : str(raw.instanceId);
  if (inst) { sel.instanceId = inst; }
  if (raw.shape === 'icon') { sel.shape = 'icon'; }
  return sel;
}

// The stored JSON array (localStorage `plugin_selections`) -> slot-indexed selections.
// Index n is widget id 144+n, so unusable entries stay as null placeholders instead of
// shifting the later ones; capped at MAX_PLUGINS. Garbage in -> [].
export function parseSelections(json: string | null): (PluginSelection | null)[] {
  let arr: any;
  try { arr = JSON.parse(json || '[]'); } catch (e) { return []; }
  if (!Array.isArray(arr)) { return []; }
  const out: (PluginSelection | null)[] = [];
  for (let i = 0; i < arr.length && i < MAX_PLUGINS; i++) { out.push(normalizeSelection(arr[i])); }
  return out;
}

// The config page's pluginList value (array of selections / nulls for empty slots, or the
// JSON of one) -> the JSON string stored as localStorage `plugin_selections`: same slot
// order, unusable entries as null, trailing nulls trimmed, capped at MAX_PLUGINS. Always
// round-trips through parseSelections. Slot n stays widget id 144+n even when an earlier
// slot is emptied.
export function serializeSelections(value: any): string {
  let arr: any = value;
  if (typeof arr === 'string') { try { arr = JSON.parse(arr); } catch (e) { arr = []; } }
  if (!Array.isArray(arr)) { arr = []; }
  const out: (PluginSelection | null)[] = [];
  for (let i = 0; i < arr.length && i < MAX_PLUGINS; i++) { out.push(normalizeSelection(arr[i])); }
  while (out.length > 0 && out[out.length - 1] === null) { out.pop(); }
  return JSON.stringify(out);
}

// Selection groups sharing one subscription: same plugin + category + item.
export interface SelectionGroup {
  plugin?: string;
  category: string;
  item: string;
  properties: string[];
  slots: number[];
  icons: boolean;         // some slot of the group shows an icon: subscribe with iconPixelSize
}

export function groupSelections(sels: (PluginSelection | null)[]): SelectionGroup[] {
  const groups: SelectionGroup[] = [];
  const byKey: Record<string, SelectionGroup> = {};
  for (let i = 0; i < sels.length; i++) {
    const s = sels[i];
    if (!s) { continue; }
    const key = (s.plugin || '') + '|' + s.category + '|' + s.item;
    let g = byKey[key];
    if (!g) {
      g = { plugin: s.plugin, category: s.category, item: s.item, properties: [], slots: [], icons: false };
      byKey[key] = g;
      groups.push(g);
    }
    if (g.properties.indexOf(s.property) === -1) { g.properties.push(s.property); }
    g.slots.push(i);
    if (s.shape === 'icon') { g.icons = true; }
  }
  return groups;
}

// The instance a selection reads: the one whose instanceId matches, else (for a bare
// number) the 1-based n-th, else -- only when no instanceId was chosen -- the first.
export function pickInstance(envelope: any, instanceId?: string): PebblePluginInstance | null {
  const list: PebblePluginInstance[] =
    (envelope && Array.isArray(envelope.instances)) ? envelope.instances : [];
  if (list.length === 0) { return null; }
  if (!instanceId) { return list[0]; }
  for (let i = 0; i < list.length; i++) {
    if (list[i] && list[i].instanceId === instanceId) { return list[i]; }
  }
  if (/^\d+$/.test(instanceId)) {
    const n = parseInt(instanceId, 10);
    if (n >= 1 && n <= list.length) { return list[n - 1]; }
  }
  return null;
}

// Short value text: ~3 significant digits, no float noise, sign kept, no trailing
// zeros ("18.3", "-4", "1013", "0.06"). Non-finite / non-number -> "--".
export function formatNumber(n: any): string {
  if (typeof n !== 'number' || !isFinite(n)) { return NO_DATA; }
  const a = Math.abs(n);
  const dp = a >= 100 ? 0 : a >= 10 ? 1 : a >= 1 ? 2 : 3;
  let s = n.toFixed(dp);
  if (s.indexOf('.') !== -1) { s = s.replace(/0+$/, '').replace(/\.$/, ''); }
  if (s === '-0') { s = '0'; }
  return s.length > MAX_VALUE_BYTES ? NO_DATA : s;
}

// Cut `s` to at most `max` UTF-8 bytes without splitting a character.
export function truncateUtf8(s: string, max: number): string {
  let bytes = 0, out = '';
  for (let i = 0; i < s.length; i++) {
    const c = s.charCodeAt(i);
    const len = c < 0x80 ? 1 : c < 0x800 ? 2 : (c >= 0xD800 && c <= 0xDFFF) ? 2 : 3;  // a surrogate pair = 4 over 2 units
    if (bytes + len > max) { break; }
    bytes += len;
    out += s.charAt(i);
  }
  return out;
}

export function noData(slot: number): PluginResult {
  return { slot: slot, value: NO_DATA, unit: '' };
}

const B64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

// Base64 (padding optional, whitespace ignored) -> bytes, or null on any other character.
// Pure so it works where atob is absent (pkjs).
export function base64ToBytes(s: any): number[] | null {
  if (typeof s !== 'string') { return null; }
  const clean = s.replace(/[\s=]+/g, '');
  if (clean.length % 4 === 1) { return null; }
  const out: number[] = [];
  let acc = 0, bits = 0;
  for (let i = 0; i < clean.length; i++) {
    const v = B64.indexOf(clean.charAt(i));
    if (v === -1) { return null; }
    acc = (acc << 6) | v;
    bits += 6;
    if (bits >= 8) { bits -= 8; out.push((acc >> bits) & 0xff); acc &= (1 << bits) - 1; }
  }
  return out;
}

// An envelope icon `{pixels, palette, width, height}` (base64 strings) -> PluginIcon, or
// null when missing, undecodable, oversized or too short for width x height.
export function decodeIcon(raw: any): PluginIcon | null {
  if (!raw || typeof raw !== 'object') { return null; }
  const w = raw.width, h = raw.height;
  if (typeof w !== 'number' || typeof h !== 'number' || w < 1 || h < 1 || w > MAX_ICON_DIM
      || h > MAX_ICON_DIM || Math.floor(w) !== w || Math.floor(h) !== h) { return null; }
  const pixels = base64ToBytes(raw.pixels), palette = base64ToBytes(raw.palette);
  if (!pixels || !palette || palette.length !== 3) { return null; }
  const need = h * Math.ceil(w / 2);
  if (pixels.length < need) { return null; }
  return { width: w, height: h, palette: palette, pixels: pixels.slice(0, need) };
}

// The icon AppMessage of one slot (own message: ~290 bytes of byte arrays).
export function buildIconMessage(slot: number, icon: PluginIcon): Record<string, any> {
  return { PluginIconSlot: slot, PluginIconWidth: icon.width, PluginIconHeight: icon.height,
    PluginIconPalette: icon.palette, PluginIconPixels: icon.pixels };
}

// A cheap identity of an icon for the unchanged-bitmap check.
export function iconKey(icon: PluginIcon): string {
  return icon.width + 'x' + icon.height + ':' + icon.palette.join(',') + ':' + icon.pixels.join(',');
}

// The icon messages to send now: one per result with an icon whose key differs from
// `sent` (slot -> key last delivered), all of them when `force`. Each carries its key so
// the caller records it only after a successful send.
export function iconMessages(results: PluginResult[], sent: Record<string, string>,
                             force: boolean): { slot: number; key: string; msg: Record<string, any> }[] {
  const out: { slot: number; key: string; msg: Record<string, any> }[] = [];
  results.forEach((r) => {
    if (!r.icon) { return; }
    const key = iconKey(r.icon);
    if (!force && sent[String(r.slot)] === key) { return; }
    out.push({ slot: r.slot, key: key, msg: buildIconMessage(r.slot, r.icon) });
  });
  return out;
}

// `sent` restricted to slots that still show an icon: a slot that switched to a number
// loses its bitmap on the watch, so selecting the same icon again must resend it.
export function pruneIconKeys(sent: Record<string, string>, results: PluginResult[]): Record<string, string> {
  const out: Record<string, string> = {};
  results.forEach((r) => { if (r.icon && sent[String(r.slot)] !== undefined) { out[String(r.slot)] = sent[String(r.slot)]; } });
  return out;
}

// The slot's result for a selection, read from the fetched envelope: formatted value
// + unit, or "--" / "" when the instance, property or numericValue is missing.
export function resultFor(slot: number, sel: PluginSelection, envelope: any): PluginResult {
  const inst = pickInstance(envelope, sel.instanceId);
  const prop = inst && inst.properties ? inst.properties[sel.property] : undefined;
  if (sel.shape === 'icon') {
    const icon = decodeIcon(prop ? (prop as any).icon : undefined);
    return icon ? { slot: slot, value: ICON_MARK, unit: '', icon: icon } : noData(slot);
  }
  const nv = prop ? prop.numericValue : undefined;
  if (!nv || typeof nv.value !== 'number') { return noData(slot); }
  const value = formatNumber(nv.value);
  if (value === NO_DATA) { return noData(slot); }
  const unit = typeof nv.unit === 'string'
    ? truncateUtf8(nv.unit.replace(/[\x00-\x1f]/g, ''), MAX_UNIT_BYTES) : '';
  return { slot: slot, value: value, unit: unit };
}

// True when a fetch produced nothing usable (every slot "--": the subscription failed or
// timed out). plugins.ts then forgets the fetch time so the next watch poll retries at once
// instead of waiting out the burst-coalescing throttle.
export function allNoData(results: PluginResult[]): boolean {
  return results.every((r) => r.value === NO_DATA);
}

// Records for every slot 0..MAX_PLUGINS-1, so a slot without a selection (or a removed
// one) reads "--" on the watch instead of looking like missing data to its cold-poll
// check. A result for a slot overrides the default.
export function packPluginData(results: PluginResult[]): string {
  const bySlot: Record<number, PluginResult> = {};
  results.forEach((r) => { bySlot[r.slot] = r; });
  const parts: string[] = [];
  for (let i = 0; i < MAX_PLUGINS; i++) {
    const r = bySlot[i] || noData(i);
    parts.push(String(i), r.value, r.unit);
  }
  return parts.join(DELIM);
}

// The AppMessage dict for a set of results.
export function buildPluginMessage(results: PluginResult[]): Record<string, string> {
  return { PluginData: packPluginData(results) };
}

// True iff any placed widget id (flags stripped) is a plugin widget.
export function anyPluginWidget(ids: number[]): boolean {
  return ids.some((id) => id >= PLUGIN_WID_BASE && id < PLUGIN_WID_BASE + MAX_PLUGINS);
}

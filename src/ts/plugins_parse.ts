// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Pure helpers for the plugin-sourced widgets (ids 144..159): selection parsing,
   instance picking, value formatting and the PluginData wire string. No Pebble
   globals, so unit-testable with `node --test` (tests/plugins_parse.test.js).
   Wire format (KEEP IN SYNC with src/c/plugin_slots.c): PluginData is a string of
   `slot<US>value<US>unit` records, US = 0x1f, three fields per record, slot 0..15
   (widget id = 144 + slot). */

export const MAX_PLUGINS = 16;       // matches PLUGIN_SLOT_COUNT in src/c/plugin_slots.h
export const PLUGIN_WID_BASE = 144;  // matches WL_PLUGIN_WID_BASE in src/c/widget_list.h
export const NO_DATA = '--';
const MAX_VALUE_BYTES = 11;          // watch buffers are 12 incl. NUL
const MAX_UNIT_BYTES = 11;
const DELIM = '\x1f';

export interface PluginSelection {
  plugin?: string;        // plugin uuid; absent = any plugin offering the source
  category: string;
  item: string;
  instanceId?: string;    // instance UUID (or its 1-based number); absent = first
  property: string;
}

export interface PluginResult { slot: number; value: string; unit: string; }

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
      g = { plugin: s.plugin, category: s.category, item: s.item, properties: [], slots: [] };
      byKey[key] = g;
      groups.push(g);
    }
    if (g.properties.indexOf(s.property) === -1) { g.properties.push(s.property); }
    g.slots.push(i);
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

// The slot's result for a selection, read from the fetched envelope: formatted value
// + unit, or "--" / "" when the instance, property or numericValue is missing.
export function resultFor(slot: number, sel: PluginSelection, envelope: any): PluginResult {
  const inst = pickInstance(envelope, sel.instanceId);
  const prop = inst && inst.properties ? inst.properties[sel.property] : undefined;
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

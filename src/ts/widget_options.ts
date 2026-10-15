// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Pure widget-picker option model. The static (built-in) widgets are fixed; the
   crypto coin options are appended dynamically from the cryptoList rows. Both
   config_widget_list.ts (at runtime, reading the DOM) and the unit test use
   buildOptions, so the mapping stays in one tested place. NOTE: this module is
   imported by config_widget_list.ts at BUILD time (tsc inlines it); the values
   are embedded into the serialized component, so keep it data-only + helper-free
   (no spread/destructuring) for the toSource() re-eval. */

export interface WidgetOption { id: number; label: string; }

// KEEP IN SYNC with docs/widget_contract.md, src/c/widgets.h (WidgetType) and the
// inlined STATIC_OPTIONS copy in config_widget_list.ts (tests/widget_options_sync).
// Id 1 (BT disconnect, internal) and 4 (retired date) are deliberately not listed.
export const STATIC_WIDGETS: WidgetOption[] = [
  { id: 0, label: 'EMPTY' },
  { id: 255, label: '🔁 Rotating' },
  { id: 35, label: 'TIME digital' },
  { id: 3, label: 'TIME alt time zone' },
  { id: 5, label: 'TIME seconds' },
  { id: 11, label: 'TIME swatch beats' },
  { id: 30, label: 'DATE wkday dd' },
  { id: 31, label: 'DATE wkday d.m.' },
  { id: 32, label: 'DATE wkday' },
  { id: 26, label: 'DATE d.m.' },
  { id: 27, label: 'DATE dd.mm.' },
  { id: 28, label: 'DATE dd.mm' },
  { id: 29, label: 'DATE d.m.yyyy' },
  { id: 33, label: 'DATE dd mon' },
  { id: 34, label: 'DATE yyyy-mm-dd' },
  { id: 6, label: 'DATE week number' },
  { id: 7, label: 'WEATHER temperature' },
  { id: 37, label: 'WEATHER condition icon' },
  { id: 8, label: 'WEATHER forecast high/low' },
  { id: 13, label: 'WEATHER UV index' },
  { id: 10, label: 'HEALTH steps' },
  { id: 21, label: 'HEALTH distance' },
  { id: 12, label: 'HEALTH heart rate' },
  { id: 9, label: 'SLEEP hours' },
  { id: 20, label: 'SLEEP deep hours' },
  { id: 24, label: 'SLEEP + deep hours' },
  { id: 25, label: 'SLEEP hours + deep bar' },
  { id: 2, label: 'BATTERY percentage' },
  { id: 39, label: 'BATTERY percentage with % unit' },
  { id: 36, label: 'BATTERY icon' },
  { id: 22, label: 'BATTERY days left' },
  { id: 14, label: 'ELECTR price' },
  { id: 18, label: 'ELECTR next cheap' },
  { id: 19, label: 'ELECTR cheapest hour' },
  { id: 38, label: 'MISC bluetooth icon' },
  { id: 23, label: 'MISC Tuya LEDs' },
];

// Value-font choices for a widget row. The array index IS the font index sent in the
// payload flags (bits 0-4); FONT_DEFAULT (31) means "the position's default font".
// KEEP IN SYNC with docs/widget_contract.md and FONT_KEYS in src/c/widget_fonts.c.
export const FONT_DEFAULT = 31;
export const FONT_OPTIONS: string[] = [
  '3XS Gothic',
  '2XS Gothic',
  'XS Gothic',
  'S Gothic',
  'S Roboto',
  'M Gothic',
  'M Leco',
  'L Bitham',
  'L Droid serif',
  'L Leco',
  'XL Leco',
  '2XL Bitham',
  '2XL Leco 36',
  '2XL Leco 38',
  '3XL Leco',
  '4XL Leco',
  '5XL Leco',
];

export const ROTATING_ID = 255;

export function buildOptions(rows: any): WidgetOption[] {
  const out: WidgetOption[] = STATIC_WIDGETS.slice();
  const arr: any[] = Array.isArray(rows) ? rows : [];
  for (let i = 0; i < arr.length; i++) {
    const r = arr[i];
    if (!r) { continue; }
    const wid = parseInt(r.wid, 10);
    if (isNaN(wid)) { continue; }
    const coin = (typeof r.coin === 'string') ? r.coin : '';
    const label = (typeof r.label === 'string' && r.label !== '')
      ? r.label : coin.toUpperCase();
    out.push({ id: wid, label: label });
  }
  return out;
}

// Options valid as a rotating-group MEMBER: every drawable widget except Empty (0)
// and Rotating (255) — no nesting, no empty members.
export function memberOptions(rows: any): WidgetOption[] {
  const all = buildOptions(rows);
  const out: WidgetOption[] = [];
  for (let i = 0; i < all.length; i++) {
    const o = all[i];
    if (o.id !== 0 && o.id !== ROTATING_ID) { out.push(o); }
  }
  return out;
}

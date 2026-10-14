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

export const STATIC_WIDGETS: WidgetOption[] = [
  { id: 0, label: 'Empty' },
  { id: 255, label: '🔁 Rotating' },
  { id: 3, label: 'Alternate Time Zone' },
  { id: 5, label: 'Seconds' },
  { id: 11, label: 'Swatch Internet Time' },
  { id: 4, label: "Today's Date" },
  { id: 6, label: 'Week Number' },
  { id: 7, label: 'Current Weather' },
  { id: 8, label: "Today's Forecast" },
  { id: 13, label: 'UV Index' },
  { id: 14, label: 'Electricity price' },
  { id: 18, label: 'Next cheap electricity' },
  { id: 19, label: 'Cheapest electricity hour' },
  { id: 9, label: 'Sleep' },
  { id: 20, label: 'Deep Sleep' },
  { id: 24, label: 'Sleep + Deep Sleep' },
  { id: 25, label: 'Sleep + Deep Bar' },
  { id: 10, label: 'Steps' },
  { id: 21, label: 'Distance' },
  { id: 12, label: 'Heart Rate' },
  { id: 2, label: 'Battery' },
  { id: 22, label: 'Battery (days left)' },
  { id: 23, label: 'Tuya LEDs' },
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

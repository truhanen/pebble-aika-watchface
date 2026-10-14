// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Pure helpers for the Tuya LED-row widget: validating the configured switch rows
   and packing their fetched states into the compact wire string sent under the
   TuyaLeds message key. One character per row, in row order:
     '1' = on, '0' = off, '?' = unknown (device or code missing, or not a boolean).
   No Pebble/browser globals -> unit-tested in tests/tuya_leds.test.js. */

export const MAX_TUYA_LEDS = 6;   // matches TUYA_LEDS_MAX in src/c/tuya_leds_parse.h

export interface TuyaLedRow { deviceId: string; code: string }

export function normalizeLedRows(raw: any): TuyaLedRow[] {
  const arr: any[] = Array.isArray(raw) ? raw : [];
  const out: TuyaLedRow[] = [];
  for (let i = 0; i < arr.length && out.length < MAX_TUYA_LEDS; i++) {
    const r = arr[i];
    if (!r || typeof r !== 'object') { continue; }
    const deviceId = (typeof r.deviceId === 'string') ? r.deviceId : '';
    const code = (typeof r.code === 'string') ? r.code : '';
    if (deviceId === '' || code === '') { continue; }
    out.push({ deviceId: deviceId, code: code });
  }
  return out;
}

export function ledDeviceIds(rows: TuyaLedRow[]): string[] {
  const out: string[] = [];
  for (let i = 0; i < rows.length; i++) {
    if (out.indexOf(rows[i].deviceId) === -1) { out.push(rows[i].deviceId); }
  }
  return out;
}

function boolFor(valuesById: any, row: TuyaLedRow): boolean | null {
  const dev = valuesById ? valuesById[row.deviceId] : undefined;
  const raw = dev ? dev[row.code] : undefined;
  return (typeof raw === 'boolean') ? raw : null;
}

export function packLedStates(rows: TuyaLedRow[], valuesById: any): string {
  let s = '';
  for (let i = 0; i < rows.length; i++) {
    const b = boolFor(valuesById, rows[i]);
    s += (b === null) ? '?' : (b ? '1' : '0');
  }
  return s;
}

export function countKnownStates(rows: TuyaLedRow[], valuesById: any): number {
  let n = 0;
  for (let i = 0; i < rows.length; i++) {
    if (boolFor(valuesById, rows[i]) !== null) { n++; }
  }
  return n;
}

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Pure helpers for the configurable Tuya sensor list: validating config rows,
   building the /status request path, and packing fetched datapoint values into the
   wire string sent under the TuyaData message key. No Pebble/browser globals ->
   unit-tested in tests/tuya_parse.test.js. Wire format matches crypto/currency:
   "wid<US>label<US>value<US>..." parsed by the shared CryptoSlot C code. */

import { formatPrice } from './crypto_format';

export interface TuyaRow {
  wid: number;
  deviceId: string;
  code: string;
  p: number;      // display precision (crypto_format.formatPrice)
  t: number;      // leading digits to trim
  label: string;  // sidebar label; '' -> auto = code
}

export const DELIM = '\x1f';

export function normalizeRows(raw: any): TuyaRow[] {
  const arr: any[] = Array.isArray(raw) ? raw : [];
  const out: TuyaRow[] = [];
  for (let i = 0; i < arr.length; i++) {
    const r = arr[i];
    if (!r || typeof r !== 'object') { continue; }
    const wid = parseInt(r.wid, 10);
    const deviceId = (typeof r.deviceId === 'string') ? r.deviceId : '';
    const code = (typeof r.code === 'string') ? r.code : '';
    if (isNaN(wid) || deviceId === '' || code === '') { continue; }
    let p = parseInt(r.p, 10);
    if (isNaN(p)) { p = 0; }
    if (p > 8) { p = 8; }
    if (p < -8) { p = -8; }
    let t = parseInt(r.t, 10);
    if (isNaN(t) || t < 0) { t = 0; }
    if (t > 15) { t = 15; }
    const label = (typeof r.label === 'string') ? r.label : '';
    out.push({ wid: wid, deviceId: deviceId, code: code, p: p, t: t, label: label });
  }
  return out;
}

export function distinctDevices(rows: TuyaRow[]): string[] {
  const out: string[] = [];
  for (let i = 0; i < rows.length; i++) {
    if (out.indexOf(rows[i].deviceId) === -1) { out.push(rows[i].deviceId); }
  }
  return out;
}

// The v2.0 "thing shadow" endpoint returns ALL datapoints (incl. manufacturer-custom
// DPs like a soil sensor's real moisture `humidity1`) with fresh values — unlike
// iot-03/status, which only exposes the standard instruction set (and can be stale).
export function buildPropertiesPath(deviceId: string): string {
  return '/v2.0/cloud/thing/' + deviceId + '/shadow/properties';
}

function rawFor(valuesById: any, deviceId: string, code: string): any {
  const dev = valuesById ? valuesById[deviceId] : undefined;
  return dev ? dev[code] : undefined;
}

// valuesById: { deviceId: { code: rawValue } }; scaleMap: { deviceId: { code: {scale,unit} } }
// (from tuya_spec.catalogScaleMap); prevValues: { wid: value } fallback from the last send.
export function packTuyaData(rows: TuyaRow[], valuesById: any, scaleMap: any, prevValues: any): string {
  const fields: string[] = [];
  const prev = prevValues || {};
  for (let i = 0; i < rows.length; i++) {
    const r = rows[i];
    const raw = rawFor(valuesById, r.deviceId, r.code);
    const su = (scaleMap && scaleMap[r.deviceId] && scaleMap[r.deviceId][r.code]) || { scale: 0, unit: '' };
    let value: string;
    if (typeof raw === 'number' && isFinite(raw)) {
      // Unit suffix is intentionally NOT appended: the narrow sidebar is tight and
      // the user knows what each configured sensor means (per product decision).
      const scaled = raw / Math.pow(10, su.scale || 0);
      value = formatPrice(scaled, r.p, r.t);
    } else if (typeof raw === 'boolean') {
      value = raw ? 'On' : 'Off';
    } else if (typeof raw === 'string' && raw !== '') {
      value = raw;
    } else if (prev[r.wid] !== undefined) {
      value = prev[r.wid];
    } else {
      value = '--';
    }
    const label = (r.label !== '') ? r.label : r.code;
    fields.push(String(r.wid), label, value);
  }
  return fields.join(DELIM);
}

export function countValidValues(rows: TuyaRow[], valuesById: any): number {
  let n = 0;
  for (let i = 0; i < rows.length; i++) {
    const raw = rawFor(valuesById, rows[i].deviceId, rows[i].code);
    if (raw !== undefined && raw !== null) { n++; }
  }
  return n;
}

export function parseLastSent(packed: string): Record<string, string> {
  const out: Record<string, string> = {};
  if (!packed) { return out; }
  const f = packed.split(DELIM);
  for (let i = 0; i + 2 <= f.length - 1; i += 3) { out[f[i]] = f[i + 2]; }
  return out;
}

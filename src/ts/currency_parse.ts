// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Pure helpers for the configurable currency-pair list: validating config rows,
   building the ExchangeRate-API request per base, and packing fetched rates into
   the wire string sent under the CurrencyData message key. No Pebble/browser
   globals, so unit-testable with `node --test`. The pair list comes from config
   at runtime (read in currency.ts); this module never hardcodes pairs. */

import { formatPrice } from './crypto_format';

export interface CurrencyRow {
  wid: number;      // stable widget id in [216, 223)
  base: string;     // 3-letter ISO, uppercase (request base)
  quote: string;    // 3-letter ISO, uppercase (looked up in rates)
  p: number;        // display precision (see crypto_format.formatPrice): >=0 decimals; <0 rounds
  t: number;        // leading digits to trim (see formatPrice); 0 = none
  label: string;    // sidebar label; '' -> auto = quote currency
}

export const DELIM = '\x1f';                 // unit separator between wire fields
const BASE_URL = 'https://open.er-api.com/v6/latest/';

function up(s: any): string {
  return (typeof s === 'string') ? s.trim().toUpperCase() : '';
}

export function normalizeRows(raw: any): CurrencyRow[] {
  const arr: any[] = Array.isArray(raw) ? raw : [];
  const out: CurrencyRow[] = [];
  for (let i = 0; i < arr.length; i++) {
    const r = arr[i];
    if (!r || typeof r !== 'object') { continue; }
    const wid = parseInt(r.wid, 10);
    const base = up(r.base);
    const quote = up(r.quote);
    if (isNaN(wid) || base === '' || quote === '') { continue; }
    // Same precision/trim semantics as the crypto list (crypto_parse.ts): p >= 0 is
    // decimal places, p < 0 rounds to the nearest 10^(-p); t trims leading digits.
    let p = parseInt(r.p, 10);
    if (isNaN(p)) { p = 4; }
    if (p > 8) { p = 8; }
    if (p < -8) { p = -8; }
    let t = parseInt(r.t, 10);
    if (isNaN(t) || t < 0) { t = 0; }
    if (t > 15) { t = 15; }
    const label = (typeof r.label === 'string') ? r.label : '';
    out.push({ wid: wid, base: base, quote: quote, p: p, t: t, label: label });
  }
  return out;
}

export function distinctBases(rows: CurrencyRow[]): string[] {
  const out: string[] = [];
  for (let i = 0; i < rows.length; i++) {
    if (out.indexOf(rows[i].base) === -1) { out.push(rows[i].base); }
  }
  return out;
}

export function buildRateUrl(base: string): string {
  return BASE_URL + base;
}

function labelFor(r: CurrencyRow): string {
  // Empty label -> the quote currency alone (e.g. "USD"), which fits the narrow
  // sidebar; the full "BASE/QUOTE" would clip. Users can set any custom label.
  return r.label !== '' ? r.label : r.quote;
}

// ratesByBase: { BASE: { QUOTE: number, ... } } accumulated from the per-base
// responses. prevValues: { wid: value } from the last send, used as a fallback
// for a pair whose base hasn't answered (or failed) this round.
export function packCurrencyData(rows: CurrencyRow[], ratesByBase: any, prevValues: any): string {
  const fields: string[] = [];
  const prev = prevValues || {};
  for (let i = 0; i < rows.length; i++) {
    const r = rows[i];
    const rates = ratesByBase ? ratesByBase[r.base] : undefined;
    const rate = rates ? rates[r.quote] : undefined;
    let value: string;
    if (typeof rate === 'number' && isFinite(rate)) {
      value = formatPrice(rate, r.p, r.t);
    } else if (prev[r.wid] !== undefined) {
      value = prev[r.wid];
    } else {
      value = '--';
    }
    fields.push(String(r.wid), labelFor(r), value);
  }
  return fields.join(DELIM);
}

export function countValidRates(rows: CurrencyRow[], ratesByBase: any): number {
  let n = 0;
  for (let i = 0; i < rows.length; i++) {
    const r = rows[i];
    const rates = ratesByBase ? ratesByBase[r.base] : undefined;
    const rate = rates ? rates[r.quote] : undefined;
    if (typeof rate === 'number' && isFinite(rate)) { n++; }
  }
  return n;
}

export function parseLastSent(packed: string): Record<string, string> {
  const out: Record<string, string> = {};
  if (!packed) { return out; }
  const f = packed.split(DELIM);
  for (let i = 0; i + 2 <= f.length - 1; i += 3) {
    out[f[i]] = f[i + 2];
  }
  return out;
}

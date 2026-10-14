// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Pure helpers for the configurable crypto coin list: validating config rows,
   building the CoinGecko request, and packing fetched prices into the wire
   string sent under the CryptoData message key. No Pebble/browser globals, so
   unit-testable with `node --test`. The coin list itself comes from config at
   runtime (read in crypto.ts); this module never hardcodes coins. */

import { formatPrice } from './crypto_format';

export interface CoinRow {
  wid: number;      // stable widget id (15/16/17 legacy, or 200+)
  coin: string;     // CoinGecko id, e.g. 'bitcoin'
  vs: string;       // 'usd' | 'eur'
  p: number;        // display precision (see formatPrice)
  t: number;        // leading digits to trim (see formatPrice); 0 = none
  label: string;    // sidebar label; '' -> uppercased coin id
}

export const DELIM = '\x1f';                 // unit separator between wire fields
const BASE_URL = 'https://api.coingecko.com/api/v3/simple/price';

export function normalizeRows(raw: any): CoinRow[] {
  const arr: any[] = Array.isArray(raw) ? raw : [];
  const out: CoinRow[] = [];
  for (let i = 0; i < arr.length; i++) {
    const r = arr[i];
    if (!r || typeof r !== 'object') { continue; }
    const wid = parseInt(r.wid, 10);
    const p = parseInt(r.p, 10);
    const coin = typeof r.coin === 'string' ? r.coin : '';
    if (isNaN(wid) || isNaN(p) || coin === '') { continue; }
    const vs = (r.vs === 'eur') ? 'eur' : 'usd';
    const label = (typeof r.label === 'string') ? r.label : '';
    let pc = p;
    if (pc > 8) { pc = 8; }
    if (pc < -8) { pc = -8; }
    let tc = parseInt(r.t, 10);
    if (isNaN(tc) || tc < 0) { tc = 0; }
    if (tc > 15) { tc = 15; }
    out.push({ wid: wid, coin: coin, vs: vs, p: pc, t: tc, label: label });
  }
  return out;
}

function uniq(values: string[]): string[] {
  const out: string[] = [];
  for (let i = 0; i < values.length; i++) {
    if (out.indexOf(values[i]) === -1) { out.push(values[i]); }
  }
  return out;
}

export function buildPriceUrl(rows: CoinRow[]): string {
  const ids = uniq(rows.map((r) => r.coin));
  const vs = uniq(rows.map((r) => r.vs));
  return BASE_URL + '?ids=' + ids.join(',') +
    '&vs_currencies=' + vs.join(',') + '&precision=8';
}

function labelFor(r: CoinRow): string {
  return r.label !== '' ? r.label : r.coin.toUpperCase();
}

export function packCryptoData(rows: CoinRow[], json: any): string {
  const fields: string[] = [];
  for (let i = 0; i < rows.length; i++) {
    const r = rows[i];
    const price = json && json[r.coin] ? json[r.coin][r.vs] : undefined;
    const value = (typeof price === 'number' && isFinite(price))
      ? formatPrice(price, r.p, r.t) : '--';
    fields.push(String(r.wid), labelFor(r), value);
  }
  return fields.join(DELIM);
}

/* How many configured coins resolved to a usable price in this response. A
   rate-limited / error response (e.g. CoinGecko's 429 JSON body) carries none of
   the requested coin keys, so this is 0 — the caller uses that to skip sending,
   leaving the watch's last-good prices intact instead of blanking them to "--". */
export function countValidPrices(rows: CoinRow[], json: any): number {
  let n = 0;
  for (let i = 0; i < rows.length; i++) {
    const r = rows[i];
    const price = json && json[r.coin] ? json[r.coin][r.vs] : undefined;
    if (typeof price === 'number' && isFinite(price)) { n++; }
  }
  return n;
}

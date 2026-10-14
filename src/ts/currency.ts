// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Fiat currency-pair rates via ExchangeRate-API (open access, no key), driven by
   the user's configurable pair list (Clay key `CurrencyList`, stored in
   clay-settings localStorage). Pairs are grouped by base -> one request per
   distinct base. Each response merges into an accumulator and re-packs the WHOLE
   CurrencyData string, using the last-sent values as a fallback for pairs whose
   base hasn't answered (weather.xhrRequest has no error callback, so a failed base
   simply never updates instead of blocking the others). Sent only when it changed.
   Rates are daily, so the fetch is throttled far longer than crypto. Pure helpers
   live in currency_parse.ts (unit-tested). */

import * as weather from './weather';          // reuse xhrRequest helper
import { CurrencyRow, normalizeRows, distinctBases, buildRateUrl,
  packCurrencyData, countValidRates, parseLastSent } from './currency_parse';

const LAST_SENT_KEY = 'currency_last_sent';    // last CurrencyData string we pushed
const LAST_FETCH_KEY = 'currency_last_fetch';  // unix-seconds of the last fetch attempt
// Rates update once per day, so throttle much longer than crypto (4 min). The
// watch poll is floored at 5 min; 60 min lets a scheduled poll through at most
// hourly and stays well within the open endpoint's rate limit.
const MIN_FETCH_INTERVAL_S = 60 * 60;

export function readCurrencyRows(): CurrencyRow[] {
  let stored: any;
  try {
    stored = JSON.parse(window.localStorage.getItem('clay-settings') || '{}') || {};
  } catch (e) {
    return [];
  }
  return normalizeRows(stored.CurrencyList);
}

export function updateCurrency(forceUpdate?: boolean): void {
  if (window.localStorage.getItem('disable_currency') === 'yes') {
    return;
  }
  const rows = readCurrencyRows();
  if (rows.length === 0) {
    return;
  }

  const last = parseInt(window.localStorage.getItem(LAST_FETCH_KEY) || '0', 10);
  const now = Math.floor(Date.now() / 1000);
  if (!forceUpdate && (now - last) < MIN_FETCH_INTERVAL_S) {
    console.log('currency: skipping fetch, last was ' + (now - last) + 's ago');
    return;
  }
  window.localStorage.setItem(LAST_FETCH_KEY, String(now));

  const bases = distinctBases(rows);
  const ratesByBase: Record<string, any> = {};
  const prev = parseLastSent(window.localStorage.getItem(LAST_SENT_KEY) || '');

  bases.forEach((base) => {
    weather.xhrRequest(buildRateUrl(base), 'GET', (responseText) => {
      let json;
      try {
        json = JSON.parse(responseText);
      } catch (e) {
        console.log('currency: parse error ' + e);
        return;
      }
      // An error / rate-limited body lacks result:'success'/rates -> keep last data.
      if (!json || json.result !== 'success' || !json.rates) {
        console.log('currency: no rates for ' + base + ', keeping last data');
        return;
      }
      ratesByBase[base] = json.rates;
      if (countValidRates(rows, ratesByBase) === 0) {
        return;
      }
      const packed = packCurrencyData(rows, ratesByBase, prev);
      const lastSent = window.localStorage.getItem(LAST_SENT_KEY);
      if (!forceUpdate && lastSent === packed) {
        console.log('currency: nothing changed, not sending');
        return;
      }
      Pebble.sendAppMessage({ CurrencyData: packed }, () => {
        window.localStorage.setItem(LAST_SENT_KEY, packed);
        console.log('currency: sent ' + packed);
      }, () => {
        console.log('currency: failed to send to Pebble');
      });
    });
  });
}

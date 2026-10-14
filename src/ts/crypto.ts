// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Crypto/currency prices via CoinGecko, driven by the user's configurable coin
   list (Clay key `CryptoList`, stored in clay-settings localStorage). One
   request per poll fetches every configured coin; the whole packed CryptoData
   string is sent only when it changes vs. the last send — no needless Bluetooth
   wakeups. Pure helpers (url/pack/normalize/format) live in crypto_parse.ts +
   crypto_format.ts (unit-tested). */

import * as weather from './weather';          // reuse xhrRequest helper
import { CoinRow, normalizeRows, buildPriceUrl, packCryptoData, countValidPrices } from './crypto_parse';

const LAST_SENT_KEY = 'crypto_last_sent';      // last CryptoData string we pushed
const LAST_FETCH_KEY = 'crypto_last_fetch';    // unix-seconds of the last fetch attempt
// Coalesce bursts (rapid watch polls / phone reconnects) without suppressing a
// scheduled poll: the watch's poll interval is floored at 5 min, so a 4-min
// throttle lets every scheduled poll through but blocks back-to-back fetches that
// would trip CoinGecko's free-tier rate limit (which blanks the widgets to "--").
const MIN_FETCH_INTERVAL_S = 4 * 60;

export function readCoinRows(): CoinRow[] {
  let stored: any;
  try {
    stored = JSON.parse(window.localStorage.getItem('clay-settings') || '{}') || {};
  } catch (e) {
    return [];
  }
  return normalizeRows(stored.CryptoList);
}

export function updateCrypto(forceUpdate?: boolean): void {
  if (window.localStorage.getItem('disable_crypto') === 'yes') {
    return;
  }
  const rows = readCoinRows();
  if (rows.length === 0) {
    return;
  }

  const last = parseInt(window.localStorage.getItem(LAST_FETCH_KEY) || '0', 10);
  const now = Math.floor(Date.now() / 1000);
  if (!forceUpdate && (now - last) < MIN_FETCH_INTERVAL_S) {
    console.log('crypto: skipping fetch, last was ' + (now - last) + 's ago');
    return;
  }
  // Stamp the attempt up front (not on success): a rate-limited response or a
  // burst of triggers must not immediately re-fetch and dig the rate limit deeper.
  window.localStorage.setItem(LAST_FETCH_KEY, String(now));

  const url = buildPriceUrl(rows);

  weather.xhrRequest(url, 'GET', (responseText) => {
    let json;
    try {
      json = JSON.parse(responseText);
    } catch (e) {
      console.log('crypto: parse error ' + e);
      return;
    }
    // A 429 / error body parses fine but contains no requested coin, so every
    // field would pack as "--". Sending that would overwrite the watch's
    // last-good prices with blanks -- keep the existing data instead.
    if (countValidPrices(rows, json) === 0) {
      console.log('crypto: no valid prices in response, keeping last data');
      return;
    }
    const packed = packCryptoData(rows, json);
    const last = window.localStorage.getItem(LAST_SENT_KEY);
    if (!forceUpdate && last === packed) {
      console.log('crypto: nothing changed, not sending');
      return;
    }
    Pebble.sendAppMessage({ CryptoData: packed }, () => {
      window.localStorage.setItem(LAST_SENT_KEY, packed);
      console.log('crypto: sent ' + packed);
    }, () => {
      console.log('crypto: failed to send to Pebble');
    });
  });
}

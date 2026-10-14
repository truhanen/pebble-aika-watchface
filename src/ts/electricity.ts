// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Pörssisähkö (spot electricity price). Fetches porssisahko.net v2 latest prices
   at most ~twice a day and pushes the whole 48 h quarter-hour schedule to the
   watch, which indexes "now" and computes today's average locally — so no
   Bluetooth traffic is needed between the 1-2 daily fetches. */

import * as weather from './weather';            // reuse xhrRequest helper
import { parseLatestPrices, elecFetchDue, elecExpectedTableEndEpoch } from './electricity_parse';

const LATEST_PRICES_ENDPOINT = 'https://api.porssisahko.net/v2/latest-prices.json';

export function updateElectricity(forceUpdate?: boolean): void {
  if (window.localStorage.getItem('disable_electricity') === 'yes') {
    return;
  }

  const last = parseInt(window.localStorage.getItem('electricity_last_fetch') || '0', 10);
  const tableEnd = parseInt(window.localStorage.getItem('electricity_table_end') || '0', 10);
  const now = Math.floor(Date.now() / 1000);
  // Local midnight + hour drive what coverage we expect (tomorrow's prices only
  // appear after the day-ahead publish, ~15:00 local).
  const localNow = new Date();
  const localMidnight = new Date(localNow.getFullYear(), localNow.getMonth(), localNow.getDate());
  const expectedEnd = elecExpectedTableEndEpoch(
    Math.floor(localMidnight.getTime() / 1000), localNow.getHours());
  if (!elecFetchDue(!!forceUpdate, now, last, tableEnd, expectedEnd)) {
    console.log('Electricity: skipping fetch, last was ' + (now - last)
      + 's ago, table reaches ' + tableEnd + ' (expected ' + expectedEnd + ')');
    return;
  }

  weather.xhrRequest(LATEST_PRICES_ENDPOINT, 'GET', (responseText) => {
    let parsed;
    try {
      parsed = parseLatestPrices(JSON.parse(responseText));
    } catch (e) {
      console.log('Electricity: parse error ' + e);
      return;
    }
    if (!parsed.count) {
      console.log('Electricity: no prices in response');
      return;
    }

    const dict = {
      ElecStartEpoch: parsed.startEpoch,
      ElecPrices: parsed.bytes,
    };

    Pebble.sendAppMessage(dict, () => {
      console.log('Electricity: sent ' + parsed.count + ' quarters to Pebble');
      window.localStorage.setItem('electricity_last_fetch', String(now));
      // Record how far the watch's table now reaches so a short table (e.g.
      // today-only, before the day-ahead publish) triggers a prompt re-fetch.
      window.localStorage.setItem('electricity_table_end',
        String(parsed.startEpoch + parsed.count * 900));
    }, () => {
      console.log('Electricity: failed to send to Pebble');
    });
  });
}

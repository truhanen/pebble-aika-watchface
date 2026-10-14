// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Pure parser for porssisahko.net v2 latest-prices.json.
   No Pebble/browser globals -- unit-testable with `node --test`. */

export const ELEC_MAX_QUARTERS = 192;

interface PriceEntry {
  startDate: string;
  price: number;
}

interface LatestPricesResponse {
  prices?: PriceEntry[];
}

export interface ParsedPrices {
  startEpoch: number; // UTC seconds
  count: number;
  bytes: number[]; // little-endian int16 per quarter, unit 0.01 snt/kWh
}

// The watch indexes the table arithmetically as startEpoch + i*900s, so we must
// emit entries ascending by startDate. The v2 endpoint is documented ascending,
// but we sort defensively so the result is correct regardless of API order.
export function parseLatestPrices(json: LatestPricesResponse | null | undefined): ParsedPrices {
  const prices = json?.prices ? json.prices.slice() : [];
  prices.sort((a, b) => Date.parse(a.startDate) - Date.parse(b.startDate));

  const count = Math.min(prices.length, ELEC_MAX_QUARTERS);
  const startEpoch = count ? Math.floor(Date.parse(prices[0].startDate) / 1000) : 0;

  const bytes: number[] = [];
  for (let i = 0; i < count; i++) {
    let centi = Math.round(prices[i].price * 100);
    if (centi > 32767) { centi = 32767; }
    if (centi < -32768) { centi = -32768; }
    const u = centi & 0xFFFF;
    bytes.push(u & 0xFF);
    bytes.push((u >> 8) & 0xFF);
  }

  return { startEpoch, count, bytes };
}

// --- Fetch scheduling --------------------------------------------------------
// Normal throttle once the watch holds the prices it can have (a "safety"
// refresh that also picks up any same-day price corrections).
export const ELEC_FULL_INTERVAL_S = 11 * 3600;
// Shorter "catch-up" throttle used while the held table falls short of what
// should already be published (e.g. only today's prices after the day-ahead
// publish). Without it the watch could be stuck for ELEC_FULL_INTERVAL_S
// showing "--" for the next-cheap/cheapest widgets even though tomorrow's
// prices are available.
export const ELEC_STALE_INTERVAL_S = 1 * 3600;
// Local hour after which tomorrow's day-ahead prices are reliably published.
// Nord Pool day-ahead clears ~12:42-13:00 CET (= ~13:42-14:00 EET) and Finnish
// sources cite ~13:45-14:15 local; 15:00 is a safe margin so we never poll hard
// before the data can exist.
export const ELEC_PUBLISH_HOUR_LOCAL = 15;

// Epoch (seconds) of the end of the latest day whose prices we expect to hold,
// given local midnight and the local hour. Before the publish hour only today's
// prices are expected (midnight + 1 day); at/after it, tomorrow's too (+2 days).
export function elecExpectedTableEndEpoch(localMidnightSec: number, localHour: number): number {
  const daysAhead = localHour >= ELEC_PUBLISH_HOUR_LOCAL ? 2 : 1;
  return localMidnightSec + daysAhead * 86400;
}

// Decide whether a price fetch is due. `tableEndEpoch` is the epoch (seconds) of
// the end of the last quarter currently held (0 if unknown); `expectedEndEpoch`
// is how far the table *should* reach (see elecExpectedTableEndEpoch). A forced
// update always fetches. Otherwise a table that already reaches expectedEnd uses
// the long interval, while one that falls short uses the short catch-up interval
// so newly-published prices are picked up promptly instead of after ~11 h.
export function elecFetchDue(forceUpdate: boolean, now: number, lastFetch: number,
                             tableEndEpoch: number, expectedEndEpoch: number): boolean {
  if (forceUpdate) { return true; }
  const interval = tableEndEpoch < expectedEndEpoch
    ? ELEC_STALE_INTERVAL_S : ELEC_FULL_INTERVAL_S;
  return (now - lastFetch) >= interval;
}

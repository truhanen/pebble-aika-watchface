"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
Object.defineProperty(exports, "__esModule", { value: true });
exports.ELEC_PUBLISH_HOUR_LOCAL = exports.ELEC_STALE_INTERVAL_S = exports.ELEC_FULL_INTERVAL_S = exports.ELEC_MAX_QUARTERS = void 0;
exports.parseLatestPrices = parseLatestPrices;
exports.elecExpectedTableEndEpoch = elecExpectedTableEndEpoch;
exports.elecFetchDue = elecFetchDue;
/* Pure parser for porssisahko.net v2 latest-prices.json.
   No Pebble/browser globals -- unit-testable with `node --test`. */
exports.ELEC_MAX_QUARTERS = 192;
// The watch indexes the table arithmetically as startEpoch + i*900s, so we must
// emit entries ascending by startDate. The v2 endpoint is documented ascending,
// but we sort defensively so the result is correct regardless of API order.
function parseLatestPrices(json) {
    var prices = (json === null || json === void 0 ? void 0 : json.prices) ? json.prices.slice() : [];
    prices.sort(function (a, b) { return Date.parse(a.startDate) - Date.parse(b.startDate); });
    var count = Math.min(prices.length, exports.ELEC_MAX_QUARTERS);
    var startEpoch = count ? Math.floor(Date.parse(prices[0].startDate) / 1000) : 0;
    var bytes = [];
    for (var i = 0; i < count; i++) {
        var centi = Math.round(prices[i].price * 100);
        if (centi > 32767) {
            centi = 32767;
        }
        if (centi < -32768) {
            centi = -32768;
        }
        var u = centi & 0xFFFF;
        bytes.push(u & 0xFF);
        bytes.push((u >> 8) & 0xFF);
    }
    return { startEpoch: startEpoch, count: count, bytes: bytes };
}
// --- Fetch scheduling --------------------------------------------------------
// Normal throttle once the watch holds the prices it can have (a "safety"
// refresh that also picks up any same-day price corrections).
exports.ELEC_FULL_INTERVAL_S = 11 * 3600;
// Shorter "catch-up" throttle used while the held table falls short of what
// should already be published (e.g. only today's prices after the day-ahead
// publish). Without it the watch could be stuck for ELEC_FULL_INTERVAL_S
// showing "--" for the next-cheap/cheapest widgets even though tomorrow's
// prices are available.
exports.ELEC_STALE_INTERVAL_S = 1 * 3600;
// Local hour after which tomorrow's day-ahead prices are reliably published.
// Nord Pool day-ahead clears ~12:42-13:00 CET (= ~13:42-14:00 EET) and Finnish
// sources cite ~13:45-14:15 local; 15:00 is a safe margin so we never poll hard
// before the data can exist.
exports.ELEC_PUBLISH_HOUR_LOCAL = 15;
// Epoch (seconds) of the end of the latest day whose prices we expect to hold,
// given local midnight and the local hour. Before the publish hour only today's
// prices are expected (midnight + 1 day); at/after it, tomorrow's too (+2 days).
function elecExpectedTableEndEpoch(localMidnightSec, localHour) {
    var daysAhead = localHour >= exports.ELEC_PUBLISH_HOUR_LOCAL ? 2 : 1;
    return localMidnightSec + daysAhead * 86400;
}
// Decide whether a price fetch is due. `tableEndEpoch` is the epoch (seconds) of
// the end of the last quarter currently held (0 if unknown); `expectedEndEpoch`
// is how far the table *should* reach (see elecExpectedTableEndEpoch). A forced
// update always fetches. Otherwise a table that already reaches expectedEnd uses
// the long interval, while one that falls short uses the short catch-up interval
// so newly-published prices are picked up promptly instead of after ~11 h.
function elecFetchDue(forceUpdate, now, lastFetch, tableEndEpoch, expectedEndEpoch) {
    if (forceUpdate) {
        return true;
    }
    var interval = tableEndEpoch < expectedEndEpoch
        ? exports.ELEC_STALE_INTERVAL_S : exports.ELEC_FULL_INTERVAL_S;
    return (now - lastFetch) >= interval;
}

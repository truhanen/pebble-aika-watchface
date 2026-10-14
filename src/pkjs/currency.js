"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.readCurrencyRows = readCurrencyRows;
exports.updateCurrency = updateCurrency;
/* Fiat currency-pair rates via ExchangeRate-API (open access, no key), driven by
   the user's configurable pair list (Clay key `CurrencyList`, stored in
   clay-settings localStorage). Pairs are grouped by base -> one request per
   distinct base. Each response merges into an accumulator and re-packs the WHOLE
   CurrencyData string, using the last-sent values as a fallback for pairs whose
   base hasn't answered (weather.xhrRequest has no error callback, so a failed base
   simply never updates instead of blocking the others). Sent only when it changed.
   Rates are daily, so the fetch is throttled far longer than crypto. Pure helpers
   live in currency_parse.ts (unit-tested). */
var weather = __importStar(require("./weather")); // reuse xhrRequest helper
var currency_parse_1 = require("./currency_parse");
var LAST_SENT_KEY = 'currency_last_sent'; // last CurrencyData string we pushed
var LAST_FETCH_KEY = 'currency_last_fetch'; // unix-seconds of the last fetch attempt
// Rates update once per day, so throttle much longer than crypto (4 min). The
// watch poll is floored at 5 min; 60 min lets a scheduled poll through at most
// hourly and stays well within the open endpoint's rate limit.
var MIN_FETCH_INTERVAL_S = 60 * 60;
function readCurrencyRows() {
    var stored;
    try {
        stored = JSON.parse(window.localStorage.getItem('clay-settings') || '{}') || {};
    }
    catch (e) {
        return [];
    }
    return (0, currency_parse_1.normalizeRows)(stored.CurrencyList);
}
function updateCurrency(forceUpdate) {
    if (window.localStorage.getItem('disable_currency') === 'yes') {
        return;
    }
    var rows = readCurrencyRows();
    if (rows.length === 0) {
        return;
    }
    var last = parseInt(window.localStorage.getItem(LAST_FETCH_KEY) || '0', 10);
    var now = Math.floor(Date.now() / 1000);
    if (!forceUpdate && (now - last) < MIN_FETCH_INTERVAL_S) {
        console.log('currency: skipping fetch, last was ' + (now - last) + 's ago');
        return;
    }
    window.localStorage.setItem(LAST_FETCH_KEY, String(now));
    var bases = (0, currency_parse_1.distinctBases)(rows);
    var ratesByBase = {};
    var prev = (0, currency_parse_1.parseLastSent)(window.localStorage.getItem(LAST_SENT_KEY) || '');
    bases.forEach(function (base) {
        weather.xhrRequest((0, currency_parse_1.buildRateUrl)(base), 'GET', function (responseText) {
            var json;
            try {
                json = JSON.parse(responseText);
            }
            catch (e) {
                console.log('currency: parse error ' + e);
                return;
            }
            // An error / rate-limited body lacks result:'success'/rates -> keep last data.
            if (!json || json.result !== 'success' || !json.rates) {
                console.log('currency: no rates for ' + base + ', keeping last data');
                return;
            }
            ratesByBase[base] = json.rates;
            if ((0, currency_parse_1.countValidRates)(rows, ratesByBase) === 0) {
                return;
            }
            var packed = (0, currency_parse_1.packCurrencyData)(rows, ratesByBase, prev);
            var lastSent = window.localStorage.getItem(LAST_SENT_KEY);
            if (!forceUpdate && lastSent === packed) {
                console.log('currency: nothing changed, not sending');
                return;
            }
            Pebble.sendAppMessage({ CurrencyData: packed }, function () {
                window.localStorage.setItem(LAST_SENT_KEY, packed);
                console.log('currency: sent ' + packed);
            }, function () {
                console.log('currency: failed to send to Pebble');
            });
        });
    });
}

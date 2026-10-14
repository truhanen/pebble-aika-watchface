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
exports.readCoinRows = readCoinRows;
exports.updateCrypto = updateCrypto;
/* Crypto/currency prices via CoinGecko, driven by the user's configurable coin
   list (Clay key `CryptoList`, stored in clay-settings localStorage). One
   request per poll fetches every configured coin; the whole packed CryptoData
   string is sent only when it changes vs. the last send — no needless Bluetooth
   wakeups. Pure helpers (url/pack/normalize/format) live in crypto_parse.ts +
   crypto_format.ts (unit-tested). */
var weather = __importStar(require("./weather")); // reuse xhrRequest helper
var crypto_parse_1 = require("./crypto_parse");
var LAST_SENT_KEY = 'crypto_last_sent'; // last CryptoData string we pushed
var LAST_FETCH_KEY = 'crypto_last_fetch'; // unix-seconds of the last fetch attempt
// Coalesce bursts (rapid watch polls / phone reconnects) without suppressing a
// scheduled poll: the watch's poll interval is floored at 5 min, so a 4-min
// throttle lets every scheduled poll through but blocks back-to-back fetches that
// would trip CoinGecko's free-tier rate limit (which blanks the widgets to "--").
var MIN_FETCH_INTERVAL_S = 4 * 60;
function readCoinRows() {
    var stored;
    try {
        stored = JSON.parse(window.localStorage.getItem('clay-settings') || '{}') || {};
    }
    catch (e) {
        return [];
    }
    return (0, crypto_parse_1.normalizeRows)(stored.CryptoList);
}
function updateCrypto(forceUpdate) {
    if (window.localStorage.getItem('disable_crypto') === 'yes') {
        return;
    }
    var rows = readCoinRows();
    if (rows.length === 0) {
        return;
    }
    var last = parseInt(window.localStorage.getItem(LAST_FETCH_KEY) || '0', 10);
    var now = Math.floor(Date.now() / 1000);
    if (!forceUpdate && (now - last) < MIN_FETCH_INTERVAL_S) {
        console.log('crypto: skipping fetch, last was ' + (now - last) + 's ago');
        return;
    }
    // Stamp the attempt up front (not on success): a rate-limited response or a
    // burst of triggers must not immediately re-fetch and dig the rate limit deeper.
    window.localStorage.setItem(LAST_FETCH_KEY, String(now));
    var url = (0, crypto_parse_1.buildPriceUrl)(rows);
    weather.xhrRequest(url, 'GET', function (responseText) {
        var json;
        try {
            json = JSON.parse(responseText);
        }
        catch (e) {
            console.log('crypto: parse error ' + e);
            return;
        }
        // A 429 / error body parses fine but contains no requested coin, so every
        // field would pack as "--". Sending that would overwrite the watch's
        // last-good prices with blanks -- keep the existing data instead.
        if ((0, crypto_parse_1.countValidPrices)(rows, json) === 0) {
            console.log('crypto: no valid prices in response, keeping last data');
            return;
        }
        var packed = (0, crypto_parse_1.packCryptoData)(rows, json);
        var last = window.localStorage.getItem(LAST_SENT_KEY);
        if (!forceUpdate && last === packed) {
            console.log('crypto: nothing changed, not sending');
            return;
        }
        Pebble.sendAppMessage({ CryptoData: packed }, function () {
            window.localStorage.setItem(LAST_SENT_KEY, packed);
            console.log('crypto: sent ' + packed);
        }, function () {
            console.log('crypto: failed to send to Pebble');
        });
    });
}

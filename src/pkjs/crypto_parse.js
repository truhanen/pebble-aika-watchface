"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
Object.defineProperty(exports, "__esModule", { value: true });
exports.DELIM = void 0;
exports.normalizeRows = normalizeRows;
exports.buildPriceUrl = buildPriceUrl;
exports.packCryptoData = packCryptoData;
exports.countValidPrices = countValidPrices;
/* Pure helpers for the configurable crypto coin list: validating config rows,
   building the CoinGecko request, and packing fetched prices into the wire
   string sent under the CryptoData message key. No Pebble/browser globals, so
   unit-testable with `node --test`. The coin list itself comes from config at
   runtime (read in crypto.ts); this module never hardcodes coins. */
var crypto_format_1 = require("./crypto_format");
exports.DELIM = '\x1f'; // unit separator between wire fields
var BASE_URL = 'https://api.coingecko.com/api/v3/simple/price';
function normalizeRows(raw) {
    var arr = Array.isArray(raw) ? raw : [];
    var out = [];
    for (var i = 0; i < arr.length; i++) {
        var r = arr[i];
        if (!r || typeof r !== 'object') {
            continue;
        }
        var wid = parseInt(r.wid, 10);
        var p = parseInt(r.p, 10);
        var coin = typeof r.coin === 'string' ? r.coin : '';
        if (isNaN(wid) || isNaN(p) || coin === '') {
            continue;
        }
        var vs = (r.vs === 'eur') ? 'eur' : 'usd';
        var label = (typeof r.label === 'string') ? r.label : '';
        var pc = p;
        if (pc > 8) {
            pc = 8;
        }
        if (pc < -8) {
            pc = -8;
        }
        var tc = parseInt(r.t, 10);
        if (isNaN(tc) || tc < 0) {
            tc = 0;
        }
        if (tc > 15) {
            tc = 15;
        }
        out.push({ wid: wid, coin: coin, vs: vs, p: pc, t: tc, label: label });
    }
    return out;
}
function uniq(values) {
    var out = [];
    for (var i = 0; i < values.length; i++) {
        if (out.indexOf(values[i]) === -1) {
            out.push(values[i]);
        }
    }
    return out;
}
function buildPriceUrl(rows) {
    var ids = uniq(rows.map(function (r) { return r.coin; }));
    var vs = uniq(rows.map(function (r) { return r.vs; }));
    return BASE_URL + '?ids=' + ids.join(',') +
        '&vs_currencies=' + vs.join(',') + '&precision=8';
}
function labelFor(r) {
    return r.label !== '' ? r.label : r.coin.toUpperCase();
}
function packCryptoData(rows, json) {
    var fields = [];
    for (var i = 0; i < rows.length; i++) {
        var r = rows[i];
        var price = json && json[r.coin] ? json[r.coin][r.vs] : undefined;
        var value = (typeof price === 'number' && isFinite(price))
            ? (0, crypto_format_1.formatPrice)(price, r.p, r.t) : '--';
        fields.push(String(r.wid), labelFor(r), value);
    }
    return fields.join(exports.DELIM);
}
/* How many configured coins resolved to a usable price in this response. A
   rate-limited / error response (e.g. CoinGecko's 429 JSON body) carries none of
   the requested coin keys, so this is 0 — the caller uses that to skip sending,
   leaving the watch's last-good prices intact instead of blanking them to "--". */
function countValidPrices(rows, json) {
    var n = 0;
    for (var i = 0; i < rows.length; i++) {
        var r = rows[i];
        var price = json && json[r.coin] ? json[r.coin][r.vs] : undefined;
        if (typeof price === 'number' && isFinite(price)) {
            n++;
        }
    }
    return n;
}

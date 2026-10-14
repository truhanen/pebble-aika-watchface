"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
Object.defineProperty(exports, "__esModule", { value: true });
exports.DELIM = void 0;
exports.normalizeRows = normalizeRows;
exports.distinctBases = distinctBases;
exports.buildRateUrl = buildRateUrl;
exports.packCurrencyData = packCurrencyData;
exports.countValidRates = countValidRates;
exports.parseLastSent = parseLastSent;
/* Pure helpers for the configurable currency-pair list: validating config rows,
   building the ExchangeRate-API request per base, and packing fetched rates into
   the wire string sent under the CurrencyData message key. No Pebble/browser
   globals, so unit-testable with `node --test`. The pair list comes from config
   at runtime (read in currency.ts); this module never hardcodes pairs. */
var crypto_format_1 = require("./crypto_format");
exports.DELIM = '\x1f'; // unit separator between wire fields
var BASE_URL = 'https://open.er-api.com/v6/latest/';
function up(s) {
    return (typeof s === 'string') ? s.trim().toUpperCase() : '';
}
function normalizeRows(raw) {
    var arr = Array.isArray(raw) ? raw : [];
    var out = [];
    for (var i = 0; i < arr.length; i++) {
        var r = arr[i];
        if (!r || typeof r !== 'object') {
            continue;
        }
        var wid = parseInt(r.wid, 10);
        var base = up(r.base);
        var quote = up(r.quote);
        if (isNaN(wid) || base === '' || quote === '') {
            continue;
        }
        // Same precision/trim semantics as the crypto list (crypto_parse.ts): p >= 0 is
        // decimal places, p < 0 rounds to the nearest 10^(-p); t trims leading digits.
        var p = parseInt(r.p, 10);
        if (isNaN(p)) {
            p = 4;
        }
        if (p > 8) {
            p = 8;
        }
        if (p < -8) {
            p = -8;
        }
        var t = parseInt(r.t, 10);
        if (isNaN(t) || t < 0) {
            t = 0;
        }
        if (t > 15) {
            t = 15;
        }
        var label = (typeof r.label === 'string') ? r.label : '';
        out.push({ wid: wid, base: base, quote: quote, p: p, t: t, label: label });
    }
    return out;
}
function distinctBases(rows) {
    var out = [];
    for (var i = 0; i < rows.length; i++) {
        if (out.indexOf(rows[i].base) === -1) {
            out.push(rows[i].base);
        }
    }
    return out;
}
function buildRateUrl(base) {
    return BASE_URL + base;
}
function labelFor(r) {
    // Empty label -> the quote currency alone (e.g. "USD"), which fits the narrow
    // sidebar; the full "BASE/QUOTE" would clip. Users can set any custom label.
    return r.label !== '' ? r.label : r.quote;
}
// ratesByBase: { BASE: { QUOTE: number, ... } } accumulated from the per-base
// responses. prevValues: { wid: value } from the last send, used as a fallback
// for a pair whose base hasn't answered (or failed) this round.
function packCurrencyData(rows, ratesByBase, prevValues) {
    var fields = [];
    var prev = prevValues || {};
    for (var i = 0; i < rows.length; i++) {
        var r = rows[i];
        var rates = ratesByBase ? ratesByBase[r.base] : undefined;
        var rate = rates ? rates[r.quote] : undefined;
        var value = void 0;
        if (typeof rate === 'number' && isFinite(rate)) {
            value = (0, crypto_format_1.formatPrice)(rate, r.p, r.t);
        }
        else if (prev[r.wid] !== undefined) {
            value = prev[r.wid];
        }
        else {
            value = '--';
        }
        fields.push(String(r.wid), labelFor(r), value);
    }
    return fields.join(exports.DELIM);
}
function countValidRates(rows, ratesByBase) {
    var n = 0;
    for (var i = 0; i < rows.length; i++) {
        var r = rows[i];
        var rates = ratesByBase ? ratesByBase[r.base] : undefined;
        var rate = rates ? rates[r.quote] : undefined;
        if (typeof rate === 'number' && isFinite(rate)) {
            n++;
        }
    }
    return n;
}
function parseLastSent(packed) {
    var out = {};
    if (!packed) {
        return out;
    }
    var f = packed.split(exports.DELIM);
    for (var i = 0; i + 2 <= f.length - 1; i += 3) {
        out[f[i]] = f[i + 2];
    }
    return out;
}

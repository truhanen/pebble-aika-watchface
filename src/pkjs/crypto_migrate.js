"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
Object.defineProperty(exports, "__esModule", { value: true });
exports.seedCryptoList = seedCryptoList;
exports.migrateCryptoList = migrateCryptoList;
var LEGACY = [
    { flag: 'disable_btc', wid: 15, coin: 'bitcoin', p: -3, label: 'BTC' },
    { flag: 'disable_xmr', wid: 16, coin: 'monero', p: 0, label: 'XMR' },
    { flag: 'disable_eurusd', wid: 17, coin: 'euro-coin', p: 3, label: 'EUR' },
];
function seedCryptoList(flags) {
    var out = [];
    for (var i = 0; i < LEGACY.length; i++) {
        var L = LEGACY[i];
        if (flags[L.flag] != null && flags[L.flag] !== 'yes') {
            out.push({ wid: L.wid, coin: L.coin, vs: 'usd', p: L.p, t: 0, label: L.label });
        }
    }
    return out;
}
function migrateCryptoList() {
    var stored;
    try {
        stored = JSON.parse(window.localStorage.getItem('clay-settings') || '{}') || {};
    }
    catch (e) {
        return;
    }
    if (stored.CryptoList !== undefined) {
        return;
    }
    var flags = {
        disable_btc: window.localStorage.getItem('disable_btc'),
        disable_xmr: window.localStorage.getItem('disable_xmr'),
        disable_eurusd: window.localStorage.getItem('disable_eurusd'),
    };
    stored.CryptoList = seedCryptoList(flags);
    window.localStorage.setItem('clay-settings', JSON.stringify(stored));
}

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
exports.updateElectricity = updateElectricity;
/* Pörssisähkö (spot electricity price). Fetches porssisahko.net v2 latest prices
   at most ~twice a day and pushes the whole 48 h quarter-hour schedule to the
   watch, which indexes "now" and computes today's average locally — so no
   Bluetooth traffic is needed between the 1-2 daily fetches. */
var weather = __importStar(require("./weather")); // reuse xhrRequest helper
var electricity_parse_1 = require("./electricity_parse");
var LATEST_PRICES_ENDPOINT = 'https://api.porssisahko.net/v2/latest-prices.json';
function updateElectricity(forceUpdate) {
    if (window.localStorage.getItem('disable_electricity') === 'yes') {
        return;
    }
    var last = parseInt(window.localStorage.getItem('electricity_last_fetch') || '0', 10);
    var tableEnd = parseInt(window.localStorage.getItem('electricity_table_end') || '0', 10);
    var now = Math.floor(Date.now() / 1000);
    // Local midnight + hour drive what coverage we expect (tomorrow's prices only
    // appear after the day-ahead publish, ~15:00 local).
    var localNow = new Date();
    var localMidnight = new Date(localNow.getFullYear(), localNow.getMonth(), localNow.getDate());
    var expectedEnd = (0, electricity_parse_1.elecExpectedTableEndEpoch)(Math.floor(localMidnight.getTime() / 1000), localNow.getHours());
    if (!(0, electricity_parse_1.elecFetchDue)(!!forceUpdate, now, last, tableEnd, expectedEnd)) {
        console.log('Electricity: skipping fetch, last was ' + (now - last)
            + 's ago, table reaches ' + tableEnd + ' (expected ' + expectedEnd + ')');
        return;
    }
    weather.xhrRequest(LATEST_PRICES_ENDPOINT, 'GET', function (responseText) {
        var parsed;
        try {
            parsed = (0, electricity_parse_1.parseLatestPrices)(JSON.parse(responseText));
        }
        catch (e) {
            console.log('Electricity: parse error ' + e);
            return;
        }
        if (!parsed.count) {
            console.log('Electricity: no prices in response');
            return;
        }
        var dict = {
            ElecStartEpoch: parsed.startEpoch,
            ElecPrices: parsed.bytes,
        };
        Pebble.sendAppMessage(dict, function () {
            console.log('Electricity: sent ' + parsed.count + ' quarters to Pebble');
            window.localStorage.setItem('electricity_last_fetch', String(now));
            // Record how far the watch's table now reaches so a short table (e.g.
            // today-only, before the day-ahead publish) triggers a prompt re-fetch.
            window.localStorage.setItem('electricity_table_end', String(parsed.startEpoch + parsed.count * 900));
        }, function () {
            console.log('Electricity: failed to send to Pebble');
        });
    });
}

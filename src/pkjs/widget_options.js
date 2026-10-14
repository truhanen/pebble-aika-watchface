"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
Object.defineProperty(exports, "__esModule", { value: true });
exports.ROTATING_ID = exports.STATIC_WIDGETS = void 0;
exports.buildOptions = buildOptions;
exports.memberOptions = memberOptions;
exports.STATIC_WIDGETS = [
    { id: 0, label: 'Empty' },
    { id: 255, label: '🔁 Rotating' },
    { id: 3, label: 'Alternate Time Zone' },
    { id: 5, label: 'Seconds' },
    { id: 11, label: 'Swatch Internet Time' },
    { id: 4, label: "Today's Date" },
    { id: 6, label: 'Week Number' },
    { id: 7, label: 'Current Weather' },
    { id: 8, label: "Today's Forecast" },
    { id: 13, label: 'UV Index' },
    { id: 14, label: 'Electricity price' },
    { id: 18, label: 'Next cheap electricity' },
    { id: 19, label: 'Cheapest electricity hour' },
    { id: 9, label: 'Sleep' },
    { id: 20, label: 'Deep Sleep' },
    { id: 24, label: 'Sleep + Deep Sleep' },
    { id: 25, label: 'Sleep + Deep Bar' },
    { id: 10, label: 'Steps' },
    { id: 21, label: 'Distance' },
    { id: 12, label: 'Heart Rate' },
    { id: 2, label: 'Battery' },
    { id: 22, label: 'Battery (days left)' },
    { id: 23, label: 'Tuya LEDs' },
];
exports.ROTATING_ID = 255;
function buildOptions(rows) {
    var out = exports.STATIC_WIDGETS.slice();
    var arr = Array.isArray(rows) ? rows : [];
    for (var i = 0; i < arr.length; i++) {
        var r = arr[i];
        if (!r) {
            continue;
        }
        var wid = parseInt(r.wid, 10);
        if (isNaN(wid)) {
            continue;
        }
        var coin = (typeof r.coin === 'string') ? r.coin : '';
        var label = (typeof r.label === 'string' && r.label !== '')
            ? r.label : coin.toUpperCase();
        out.push({ id: wid, label: label });
    }
    return out;
}
// Options valid as a rotating-group MEMBER: every drawable widget except Empty (0)
// and Rotating (255) — no nesting, no empty members.
function memberOptions(rows) {
    var all = buildOptions(rows);
    var out = [];
    for (var i = 0; i < all.length; i++) {
        var o = all[i];
        if (o.id !== 0 && o.id !== exports.ROTATING_ID) {
            out.push(o);
        }
    }
    return out;
}

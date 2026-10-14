"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
Object.defineProperty(exports, "__esModule", { value: true });
exports.MAX_TUYA_LEDS = void 0;
exports.normalizeLedRows = normalizeLedRows;
exports.ledDeviceIds = ledDeviceIds;
exports.packLedStates = packLedStates;
exports.countKnownStates = countKnownStates;
/* Pure helpers for the Tuya LED-row widget: validating the configured switch rows
   and packing their fetched states into the compact wire string sent under the
   TuyaLeds message key. One character per row, in row order:
     '1' = on, '0' = off, '?' = unknown (device or code missing, or not a boolean).
   No Pebble/browser globals -> unit-tested in tests/tuya_leds.test.js. */
exports.MAX_TUYA_LEDS = 6; // matches TUYA_LEDS_MAX in src/c/tuya_leds_parse.h
function normalizeLedRows(raw) {
    var arr = Array.isArray(raw) ? raw : [];
    var out = [];
    for (var i = 0; i < arr.length && out.length < exports.MAX_TUYA_LEDS; i++) {
        var r = arr[i];
        if (!r || typeof r !== 'object') {
            continue;
        }
        var deviceId = (typeof r.deviceId === 'string') ? r.deviceId : '';
        var code = (typeof r.code === 'string') ? r.code : '';
        if (deviceId === '' || code === '') {
            continue;
        }
        out.push({ deviceId: deviceId, code: code });
    }
    return out;
}
function ledDeviceIds(rows) {
    var out = [];
    for (var i = 0; i < rows.length; i++) {
        if (out.indexOf(rows[i].deviceId) === -1) {
            out.push(rows[i].deviceId);
        }
    }
    return out;
}
function boolFor(valuesById, row) {
    var dev = valuesById ? valuesById[row.deviceId] : undefined;
    var raw = dev ? dev[row.code] : undefined;
    return (typeof raw === 'boolean') ? raw : null;
}
function packLedStates(rows, valuesById) {
    var s = '';
    for (var i = 0; i < rows.length; i++) {
        var b = boolFor(valuesById, rows[i]);
        s += (b === null) ? '?' : (b ? '1' : '0');
    }
    return s;
}
function countKnownStates(rows, valuesById) {
    var n = 0;
    for (var i = 0; i < rows.length; i++) {
        if (boolFor(valuesById, rows[i]) !== null) {
            n++;
        }
    }
    return n;
}

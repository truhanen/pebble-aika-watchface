"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
Object.defineProperty(exports, "__esModule", { value: true });
exports.DELIM = void 0;
exports.normalizeRows = normalizeRows;
exports.distinctDevices = distinctDevices;
exports.buildPropertiesPath = buildPropertiesPath;
exports.packTuyaData = packTuyaData;
exports.countValidValues = countValidValues;
exports.parseLastSent = parseLastSent;
/* Pure helpers for the configurable Tuya sensor list: validating config rows,
   building the /status request path, and packing fetched datapoint values into the
   wire string sent under the TuyaData message key. No Pebble/browser globals ->
   unit-tested in tests/tuya_parse.test.js. Wire format matches crypto/currency:
   "wid<US>label<US>value<US>..." parsed by the shared CryptoSlot C code. */
var crypto_format_1 = require("./crypto_format");
exports.DELIM = '\x1f';
function normalizeRows(raw) {
    var arr = Array.isArray(raw) ? raw : [];
    var out = [];
    for (var i = 0; i < arr.length; i++) {
        var r = arr[i];
        if (!r || typeof r !== 'object') {
            continue;
        }
        var wid = parseInt(r.wid, 10);
        var deviceId = (typeof r.deviceId === 'string') ? r.deviceId : '';
        var code = (typeof r.code === 'string') ? r.code : '';
        if (isNaN(wid) || deviceId === '' || code === '') {
            continue;
        }
        var p = parseInt(r.p, 10);
        if (isNaN(p)) {
            p = 0;
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
        out.push({ wid: wid, deviceId: deviceId, code: code, p: p, t: t, label: label });
    }
    return out;
}
function distinctDevices(rows) {
    var out = [];
    for (var i = 0; i < rows.length; i++) {
        if (out.indexOf(rows[i].deviceId) === -1) {
            out.push(rows[i].deviceId);
        }
    }
    return out;
}
// The v2.0 "thing shadow" endpoint returns ALL datapoints (incl. manufacturer-custom
// DPs like a soil sensor's real moisture `humidity1`) with fresh values — unlike
// iot-03/status, which only exposes the standard instruction set (and can be stale).
function buildPropertiesPath(deviceId) {
    return '/v2.0/cloud/thing/' + deviceId + '/shadow/properties';
}
function rawFor(valuesById, deviceId, code) {
    var dev = valuesById ? valuesById[deviceId] : undefined;
    return dev ? dev[code] : undefined;
}
// valuesById: { deviceId: { code: rawValue } }; scaleMap: { deviceId: { code: {scale,unit} } }
// (from tuya_spec.catalogScaleMap); prevValues: { wid: value } fallback from the last send.
function packTuyaData(rows, valuesById, scaleMap, prevValues) {
    var fields = [];
    var prev = prevValues || {};
    for (var i = 0; i < rows.length; i++) {
        var r = rows[i];
        var raw = rawFor(valuesById, r.deviceId, r.code);
        var su = (scaleMap && scaleMap[r.deviceId] && scaleMap[r.deviceId][r.code]) || { scale: 0, unit: '' };
        var value = void 0;
        if (typeof raw === 'number' && isFinite(raw)) {
            // Unit suffix is intentionally NOT appended: the narrow sidebar is tight and
            // the user knows what each configured sensor means (per product decision).
            var scaled = raw / Math.pow(10, su.scale || 0);
            value = (0, crypto_format_1.formatPrice)(scaled, r.p, r.t);
        }
        else if (typeof raw === 'boolean') {
            value = raw ? 'On' : 'Off';
        }
        else if (typeof raw === 'string' && raw !== '') {
            value = raw;
        }
        else if (prev[r.wid] !== undefined) {
            value = prev[r.wid];
        }
        else {
            value = '--';
        }
        var label = (r.label !== '') ? r.label : r.code;
        fields.push(String(r.wid), label, value);
    }
    return fields.join(exports.DELIM);
}
function countValidValues(rows, valuesById) {
    var n = 0;
    for (var i = 0; i < rows.length; i++) {
        var raw = rawFor(valuesById, rows[i].deviceId, rows[i].code);
        if (raw !== undefined && raw !== null) {
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

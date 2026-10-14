"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseSpecCodes = parseSpecCodes;
exports.parseProperties = parseProperties;
exports.mergeCodes = mergeCodes;
exports.catalogCodesById = catalogCodesById;
exports.shortUnit = shortUnit;
exports.buildCatalog = buildCatalog;
exports.catalogScaleMap = catalogScaleMap;
function parseValues(values) {
    try {
        var v = JSON.parse(values);
        var scale = (typeof v.scale === 'number') ? v.scale : 0;
        var unit = (typeof v.unit === 'string') ? v.unit : '';
        return { scale: scale, unit: unit };
    }
    catch (e) {
        return { scale: 0, unit: '' };
    }
}
// Reportable datapoints from a /specification result. Tuya lists read-only sensor
// dps under result.status; some devices also expose them under result.functions.
// status wins on a code collision.
function parseSpecCodes(specResult) {
    var byCode = {};
    var order = [];
    function add(entry) {
        if (!entry || typeof entry.code !== 'string') {
            return;
        }
        var pv = parseValues(entry.values);
        if (!(entry.code in byCode)) {
            order.push(entry.code);
        }
        byCode[entry.code] = { code: entry.code, type: String(entry.type || ''), scale: pv.scale, unit: pv.unit };
    }
    var fns = (specResult && specResult.functions) || [];
    for (var i = 0; i < fns.length; i++) {
        add(fns[i]);
    }
    var st = (specResult && specResult.status) || [];
    for (var i = 0; i < st.length; i++) {
        add(st[i]);
    } // status overrides functions
    return order.map(function (c) { return byCode[c]; });
}
// Parse a /v2.0/cloud/thing/{id}/shadow/properties result into [{code, type, value}].
// This endpoint returns ALL datapoints INCLUDING manufacturer-custom DPs (e.g. a soil
// sensor's real moisture under `humidity1`) that /specification + iot-03/status omit,
// with fresh values. It carries no scale/unit, so scale/unit still come from the spec.
function parseProperties(propsResult) {
    var props = (propsResult && propsResult.properties) || [];
    var out = [];
    for (var i = 0; i < props.length; i++) {
        var p = props[i];
        if (!p || typeof p.code !== 'string') {
            continue;
        }
        out.push({ code: p.code, type: String(p.type || ''), value: p.value });
    }
    return out;
}
// Build the catalog code list for one device: EVERY reported property becomes a
// selectable code, carrying its current value as `sample` (shown in the config
// dropdown) and its scale/unit from the /specification where the code is declared
// there (custom DPs absent from the spec default to scale 0 / no unit).
function mergeCodes(specCodes, props, prevCodes) {
    var specByCode = {};
    var sc = Array.isArray(specCodes) ? specCodes : [];
    for (var i = 0; i < sc.length; i++) {
        specByCode[sc[i].code] = sc[i];
    }
    // Fallback scale/unit from a previously-cached catalog for this device. When a
    // device's /specification fetch fails transiently (shadow/properties still
    // succeeds), the spec is empty and every code would default to scale 0 — showing
    // e.g. a temperature of 235 instead of 23.5 until a future good discovery. Keep
    // the last-known scale/unit for those codes instead of degrading to 0.
    var prevByCode = {};
    var pc = Array.isArray(prevCodes) ? prevCodes : [];
    for (var i = 0; i < pc.length; i++) {
        prevByCode[pc[i].code] = pc[i];
    }
    var out = [];
    var ps = Array.isArray(props) ? props : [];
    for (var i = 0; i < ps.length; i++) {
        var p = ps[i];
        var s = specByCode[p.code];
        var fb = prevByCode[p.code];
        // Only carry a sample for numeric/boolean readings. Some DPs hold huge config
        // strings (e.g. a soil sensor's `plants_1` plant table); keeping those as the
        // dropdown sample bloats the catalog and breaks the config layout.
        var sample = (typeof p.value === 'number' || typeof p.value === 'boolean') ? p.value : undefined;
        out.push({
            code: p.code,
            type: p.type || (s ? s.type : (fb ? fb.type : '')),
            scale: s ? s.scale : (fb ? fb.scale : 0),
            unit: s ? s.unit : (fb ? fb.unit : ''),
            sample: sample,
        });
    }
    return out;
}
// Extract {deviceId: SensorCode[]} from a cached catalog (buildCatalog's shape),
// so a re-discovery can fall back to the last-known scale/unit per device when a
// device's /specification fetch fails. Missing/malformed catalog -> {}.
function catalogCodesById(catalog) {
    var out = {};
    var devs = (catalog && Array.isArray(catalog.devices)) ? catalog.devices : [];
    for (var i = 0; i < devs.length; i++) {
        var d = devs[i];
        if (d && typeof d.id === 'string' && Array.isArray(d.codes)) {
            out[d.id] = d.codes;
        }
    }
    return out;
}
function shortUnit(unit) {
    var u = (typeof unit === 'string') ? unit : '';
    if (u === '℃' || u === '°C' || u === '℉' || u === '°F' || u === '°') {
        return '°';
    }
    if (u === '%') {
        return '%';
    }
    return '';
}
function buildCatalog(devices, codesById) {
    var arr = [];
    var list = Array.isArray(devices) ? devices : [];
    for (var i = 0; i < list.length; i++) {
        var d = list[i];
        if (!d || typeof d.id !== 'string') {
            continue;
        }
        arr.push({ id: d.id, name: String(d.name || d.id), codes: codesById[d.id] || [] });
    }
    return { v: 1, devices: arr };
}
function catalogScaleMap(catalog) {
    var out = {};
    var devs = (catalog && Array.isArray(catalog.devices)) ? catalog.devices : [];
    for (var i = 0; i < devs.length; i++) {
        var d = devs[i];
        if (!d || typeof d.id !== 'string') {
            continue;
        }
        var codes = Array.isArray(d.codes) ? d.codes : [];
        var cm = {};
        for (var j = 0; j < codes.length; j++) {
            var c = codes[j];
            if (c && typeof c.code === 'string') {
                cm[c.code] = { scale: c.scale || 0, unit: c.unit || '' };
            }
        }
        out[d.id] = cm;
    }
    return out;
}

"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
Object.defineProperty(exports, "__esModule", { value: true });
exports.hasCachedCatalog = hasCachedCatalog;
exports.discoverCatalog = discoverCatalog;
exports.updateTuya = updateTuya;
/* Phone-side Tuya sensor driver. Reads the user's sensor rows (Clay key TuyaList in
   clay-settings), authenticates to the Tuya cloud (creds in TuyaAccessId/Secret/Region),
   and on each shared poll fetches device /status and pushes packed readings to the watch
   under the TuyaData key. Scale/unit come from the catalog discovered at config-open and
   cached in localStorage['tuya-catalog'] (no per-poll /specification call). Throttled far
   longer than crypto since sensor readings change slowly. Pure helpers live in
   tuya_client.ts / tuya_spec.ts / tuya_parse.ts (unit-tested). */
var tuya_client_1 = require("./tuya_client");
var tuya_spec_1 = require("./tuya_spec");
var tuya_parse_1 = require("./tuya_parse");
var tuya_leds_1 = require("./tuya_leds");
// All six Tuya data centers. Argentina/Latin America (and anything not in Tuya's
// country table) lives in Western America — 'us'; see the config page's tip text.
var REGION_HOST = {
    eu: 'https://openapi.tuyaeu.com', us: 'https://openapi.tuyaus.com',
    cn: 'https://openapi.tuyacn.com', in: 'https://openapi.tuyain.com',
    'eu-w': 'https://openapi-weaz.tuyaeu.com', 'us-e': 'https://openapi-ueaz.tuyaus.com',
};
var LAST_SENT_KEY = 'tuya_last_sent';
var LEDS_LAST_SENT_KEY = 'tuya_leds_last_sent';
var LAST_FETCH_KEY = 'tuya_last_fetch';
var CATALOG_KEY = 'tuya-catalog';
var TOKEN_KEY = 'tuya-token';
var MIN_FETCH_INTERVAL_S = 10 * 60; // sensor readings change slowly
function readSettings() {
    try {
        return JSON.parse(window.localStorage.getItem('clay-settings') || '{}') || {};
    }
    catch (e) {
        return {};
    }
}
function readCfg() {
    var s = readSettings();
    var id = s.TuyaAccessId, secret = s.TuyaAccessSecret, region = s.TuyaRegion || 'eu';
    if (!id || !secret) {
        return null;
    }
    return { clientId: id, secret: secret, host: REGION_HOST[region] || REGION_HOST.eu };
}
function readTuyaRows() {
    return (0, tuya_parse_1.normalizeRows)(readSettings().TuyaList);
}
// Switch rows drawn by the Tuya LED-row widget (id 23). Separate Clay list from the
// sensor rows: no label/precision, and their state is sent as a compact string.
function readLedRows() {
    return (0, tuya_leds_1.normalizeLedRows)(readSettings().TuyaLedList);
}
function loadCatalog() {
    try {
        return JSON.parse(window.localStorage.getItem(CATALOG_KEY) || 'null');
    }
    catch (e) {
        return null;
    }
}
function saveCatalog(cat) {
    try {
        window.localStorage.setItem(CATALOG_KEY, JSON.stringify(cat));
    }
    catch (e) { }
}
// True once a previous config-open discovered at least one device. Lets the config
// page open instantly from cache (refreshing in the background) instead of blocking
// on a full re-discovery every time — see index.ts showConfiguration.
function hasCachedCatalog() {
    var cat = loadCatalog();
    return !!(cat && cat.devices && cat.devices.length > 0);
}
// XMLHttpRequest -> parsed Tuya envelope (same wrapper as PebbleTuyaControl).
function http(opts) {
    return new Promise(function (resolve, reject) {
        var xhr = new XMLHttpRequest();
        xhr.open(opts.method, opts.url, true);
        var h = opts.headers || {};
        Object.keys(h).forEach(function (k) { xhr.setRequestHeader(k, h[k]); });
        xhr.onload = function () { try {
            resolve(JSON.parse(xhr.responseText));
        }
        catch (e) {
            reject(new Error('Bad JSON from Tuya'));
        } };
        xhr.onerror = function () { reject(new Error('Network error')); };
        xhr.timeout = 15000;
        xhr.ontimeout = function () { reject(new Error('Timeout')); };
        xhr.send(opts.body || null);
    });
}
var deps = {
    now: function () { return Date.now(); },
    nonce: function () { return 'xxxxxxxx'.replace(/x/g, function () { return Math.floor(Math.random() * 16).toString(16); }); },
    loadToken: function (clientId) {
        try {
            var o = JSON.parse(window.localStorage.getItem(TOKEN_KEY) || 'null');
            return (o && o.clientId === clientId) ? o : null;
        }
        catch (e) {
            return null;
        }
    },
    saveToken: function (clientId, v) {
        try {
            window.localStorage.setItem(TOKEN_KEY, JSON.stringify({ clientId: clientId, token: v.token, expiresAt: v.expiresAt }));
        }
        catch (e) { }
    },
};
var _client = null, _clientKey = null;
function getClient() {
    var cfg = readCfg();
    if (!cfg) {
        return null;
    }
    var key = cfg.clientId + '|' + cfg.host;
    if (!_client || _clientKey !== key) {
        _client = (0, tuya_client_1.createClient)(cfg, http, deps);
        _clientKey = key;
    }
    return _client;
}
// Config-open: list devices + fetch each /specification, build the catalog, cache it,
// then run cb (best-effort: any failure / no creds still calls cb so config opens).
function discoverCatalog(cb) {
    var c = getClient();
    if (!c) {
        cb();
        return;
    }
    // Last-known per-device codes (scale/unit): fall back to these when a device's
    // /specification fetch fails, so a transient error doesn't degrade good scales.
    var prevByDevice = (0, tuya_spec_1.catalogCodesById)(loadCatalog());
    var devices = [];
    var codesById = {};
    c.request('GET', '/v1.0/iot-01/associated-users/devices').then(function (resp) {
        devices = (resp.result && resp.result.devices) || [];
        var chain = Promise.resolve();
        devices.forEach(function (d) {
            chain = chain.then(function () {
                // /specification gives scale+unit (standard codes only); shadow/properties gives
                // the FULL code list + current values (incl. custom DPs). Merge so every reported
                // datapoint is selectable, with a sample value shown in the config dropdown.
                var specCodes = [];
                return c.request('GET', '/v1.0/iot-03/devices/' + d.id + '/specification')
                    .then(function (spec) { specCodes = (0, tuya_spec_1.parseSpecCodes)(spec.result); })
                    .catch(function () { specCodes = []; })
                    .then(function () { return c.request('GET', (0, tuya_parse_1.buildPropertiesPath)(d.id)); })
                    .then(function (props) { codesById[d.id] = (0, tuya_spec_1.mergeCodes)(specCodes, (0, tuya_spec_1.parseProperties)(props.result), prevByDevice[d.id]); })
                    .catch(function () { codesById[d.id] = specCodes.length ? specCodes : (prevByDevice[d.id] || []); });
            });
        });
        return chain;
    }).then(function () {
        // Don't let a degraded result (device list came back empty — lapsed app-account
        // link, transient token/region error) wipe a good cached catalog: the poll's
        // scale map and the config picker both depend on it. Keep the old cache instead.
        var fresh = (0, tuya_spec_1.buildCatalog)(devices, codesById);
        var old = loadCatalog();
        if (fresh.devices.length === 0 && old && Array.isArray(old.devices) && old.devices.length > 0) {
            console.log('tuya: discovery returned no devices, keeping cached catalog');
        }
        else {
            saveCatalog(fresh);
        }
        cb();
    }).catch(function () { cb(); });
}
function updateTuya(forceUpdate) {
    if (window.localStorage.getItem('disable_tuya') === 'yes') {
        return;
    }
    var rows = readTuyaRows();
    var ledRows = readLedRows();
    if (rows.length === 0 && ledRows.length === 0) {
        return;
    }
    var last = parseInt(window.localStorage.getItem(LAST_FETCH_KEY) || '0', 10);
    var now = Math.floor(Date.now() / 1000);
    if (!forceUpdate && (now - last) < MIN_FETCH_INTERVAL_S) {
        console.log('tuya: skipping fetch, last was ' + (now - last) + 's ago');
        return;
    }
    window.localStorage.setItem(LAST_FETCH_KEY, String(now));
    var c = getClient();
    if (!c) {
        console.log('tuya: no credentials');
        return;
    }
    var scaleMap = (0, tuya_spec_1.catalogScaleMap)(loadCatalog());
    var prev = (0, tuya_parse_1.parseLastSent)(window.localStorage.getItem(LAST_SENT_KEY) || '');
    // Union of the sensor devices and the LED devices: a device used by both is
    // fetched exactly once per poll.
    var devices = (0, tuya_parse_1.distinctDevices)(rows);
    var ledDevices = (0, tuya_leds_1.ledDeviceIds)(ledRows);
    for (var i = 0; i < ledDevices.length; i++) {
        if (devices.indexOf(ledDevices[i]) === -1) {
            devices.push(ledDevices[i]);
        }
    }
    var valuesById = {};
    var chain = Promise.resolve();
    devices.forEach(function (id) {
        chain = chain.then(function () {
            return c.request('GET', (0, tuya_parse_1.buildPropertiesPath)(id)).then(function (stat) {
                var arr = (stat && stat.result && stat.result.properties) || [];
                var m = {};
                for (var i = 0; i < arr.length; i++) {
                    m[arr[i].code] = arr[i].value;
                }
                valuesById[id] = m;
            }).catch(function () { });
        });
    });
    chain.then(function () {
        var dict = {};
        // Sensors: unchanged semantics -- skip entirely when nothing valid came back.
        if (rows.length > 0) {
            if ((0, tuya_parse_1.countValidValues)(rows, valuesById) === 0) {
                console.log('tuya: no valid readings, keeping last');
            }
            else {
                var packed = (0, tuya_parse_1.packTuyaData)(rows, valuesById, scaleMap, prev);
                if (forceUpdate || window.localStorage.getItem(LAST_SENT_KEY) !== packed) {
                    dict.TuyaData = packed;
                }
                else {
                    console.log('tuya: sensors unchanged');
                }
            }
        }
        // LEDs: a PARTIAL answer IS sent (missing rows show as '?'), but an answer with
        // no known state at all (network down / every device offline) is suppressed so
        // the watch keeps its persisted last-good states.
        var ledPacked = null;
        if (ledRows.length > 0) {
            if ((0, tuya_leds_1.countKnownStates)(ledRows, valuesById) === 0) {
                console.log('tuya: no known led states, keeping last');
            }
            else {
                var packedLeds = (0, tuya_leds_1.packLedStates)(ledRows, valuesById);
                if (forceUpdate || window.localStorage.getItem(LEDS_LAST_SENT_KEY) !== packedLeds) {
                    dict.TuyaLeds = packedLeds;
                    ledPacked = packedLeds;
                }
                else {
                    console.log('tuya: leds unchanged');
                }
            }
        }
        if (Object.keys(dict).length === 0) {
            return;
        }
        Pebble.sendAppMessage(dict, function () {
            if (dict.TuyaData !== undefined) {
                window.localStorage.setItem(LAST_SENT_KEY, dict.TuyaData);
            }
            if (ledPacked !== null) {
                window.localStorage.setItem(LEDS_LAST_SENT_KEY, ledPacked);
            }
            console.log('tuya: sent ' + JSON.stringify(dict));
        }, function () { console.log('tuya: failed to send to Pebble'); });
    });
}

"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseFmiForecastTvp = parseFmiForecastTvp;
exports.abbreviateLocationName = abbreviateLocationName;
/* Pure parser for FMI open-data WFS "point timevaluepair" forecast XML
   (fmi::forecast::edited::weather::scandinavia::point::timevaluepair). No
   Pebble/browser globals -- unit-testable with `node --test`. Unlike the
   ::simple variant, this one snaps the queried lat/lon to FMI's nearest *named*
   forecast location (the same one ilmatieteenlaitos.fi shows, e.g. "Jyskä") and
   returns its gml:name. One <wfs:member> per parameter: an om:OM_Observation
   whose observedProperty href carries param=<name>, with a MeasurementTimeseries
   of <wml2:time>/<wml2:value> pairs ('NaN' = gap). */
var NAME_RE = /<gml:name[^>]*locationcode\/name[^>]*>([^<]+)<\/gml:name>/;
var PARAM_RE = /param=([a-z0-9]+)/;
var TVP_RE = /<wml2:time>([^<]+)<\/wml2:time>\s*<wml2:value>([^<]+)<\/wml2:value>/g;
function nearest(series, nowEpochSec) {
    var best = null;
    var bestDiff = Infinity;
    for (var _i = 0, series_1 = series; _i < series_1.length; _i++) {
        var s = series_1[_i];
        var d = Math.abs(s.epoch - nowEpochSec);
        if (d < bestDiff) {
            bestDiff = d;
            best = s;
        }
    }
    return best;
}
function parseFmiForecastTvp(xml, nowEpochSec, fcSymbolEpochSec) {
    if (typeof xml !== 'string' || xml.indexOf('ExceptionReport') !== -1) {
        return { ok: false };
    }
    var nm = NAME_RE.exec(xml);
    var name = nm ? nm[1] : '';
    var temps = [];
    var syms = [];
    var uvs = [];
    var members = xml.split('<wfs:member>');
    for (var i = 1; i < members.length; i++) { // [0] is the header chunk
        var chunk = members[i];
        var pm = PARAM_RE.exec(chunk);
        if (!pm) {
            continue;
        }
        var target = pm[1] === 'temperature' ? temps
            : pm[1] === 'smartsymbol' ? syms
                : pm[1] === 'uvindex' ? uvs : null;
        if (!target) {
            continue;
        }
        var m = void 0;
        TVP_RE.lastIndex = 0;
        while ((m = TVP_RE.exec(chunk)) !== null) {
            var epoch = Math.floor(Date.parse(m[1]) / 1000);
            var value = parseFloat(m[2]);
            if (isNaN(epoch) || isNaN(value)) {
                continue;
            }
            target.push({ epoch: epoch, value: value });
        }
    }
    if (temps.length === 0) {
        return { ok: false };
    }
    var curTemp = nearest(temps, nowEpochSec);
    var curSym = syms.length ? nearest(syms, nowEpochSec) : null;
    var curUv = uvs.length ? nearest(uvs, nowEpochSec) : null;
    var high = temps[0].value;
    var low = temps[0].value;
    for (var j = 1; j < temps.length; j++) {
        if (temps[j].value > high) {
            high = temps[j].value;
        }
        if (temps[j].value < low) {
            low = temps[j].value;
        }
    }
    // Forecast condition = the smartsymbol at ~15:00 local (fcSymbolEpochSec), or
    // the nearest sample -- this is how FMI derives the day's forecast symbol shown
    // on ilmatieteenlaitos.fi (a single representative afternoon reading), NOT the
    // day's most notable weather (which spuriously showed rain when a lone morning
    // or night hour was wet). Strip the +100 night offset so the mapping yields a
    // daytime icon. Falls back to now when no target time is supplied.
    var fcTarget = (typeof fcSymbolEpochSec === 'number' && !isNaN(fcSymbolEpochSec))
        ? fcSymbolEpochSec : nowEpochSec;
    var fcSample = syms.length ? nearest(syms, fcTarget) : null;
    var fcSym = fcSample ? Math.round(fcSample.value) % 100 : null;
    return {
        ok: true,
        currentTemp: Math.round(curTemp.value),
        currentSymbol: curSym ? Math.round(curSym.value) : null,
        forecastSymbol: fcSym,
        forecastHigh: Math.round(high),
        forecastLow: Math.round(low),
        uvIndex: curUv ? Math.round(curUv.value) : 0,
        name: name,
    };
}
// FMI named location -> short label: drop the leading municipality token
// ("Jyväskylä keskus" -> "keskus"), plain-truncate to maxLen (no ellipsis), and
// force an uppercase first character.
function abbreviateLocationName(name, maxLen) {
    if (typeof name !== 'string') {
        return '';
    }
    var s = name.trim();
    var sp = s.indexOf(' ');
    if (sp > 0 && sp < s.length - 1) {
        s = s.substring(sp + 1);
    }
    if (maxLen && s.length > maxLen) {
        s = s.substring(0, maxLen);
    }
    if (s.length > 0) {
        s = s.charAt(0).toUpperCase() + s.slice(1);
    }
    return s;
}

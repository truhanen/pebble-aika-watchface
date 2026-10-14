"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseFmiForecast = parseFmiForecast;
exports.smartSymbolToIcon = smartSymbolToIcon;
/* Pure parser for FMI open-data WFS "simple feature" forecast XML.
   No Pebble/browser globals -- unit-testable with `node --test`.
   The XML is a flat list of <BsWfs:BsWfsElement> with consecutive
   <BsWfs:Time> / <BsWfs:ParameterName> / <BsWfs:ParameterValue>. */
var ELEMENT_RE = /<BsWfs:Time>([^<]+)<\/BsWfs:Time>\s*<BsWfs:ParameterName>([^<]+)<\/BsWfs:ParameterName>\s*<BsWfs:ParameterValue>([^<]+)<\/BsWfs:ParameterValue>/g;
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
function parseFmiForecast(xml, nowEpochSec, fcSymbolEpochSec) {
    if (typeof xml !== 'string' || xml.indexOf('ExceptionReport') !== -1) {
        return { ok: false };
    }
    var temps = [];
    var syms = [];
    var uvs = [];
    var m;
    ELEMENT_RE.lastIndex = 0;
    while ((m = ELEMENT_RE.exec(xml)) !== null) {
        var epoch = Math.floor(Date.parse(m[1]) / 1000);
        var value = parseFloat(m[3]);
        if (isNaN(epoch) || isNaN(value)) {
            continue;
        }
        if (m[2] === 'temperature') {
            temps.push({ epoch: epoch, value: value });
        }
        else if (m[2] === 'smartsymbol') {
            syms.push({ epoch: epoch, value: value });
        }
        else if (m[2] === 'uvindex') {
            uvs.push({ epoch: epoch, value: value });
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
    for (var i = 1; i < temps.length; i++) {
        if (temps[i].value > high) {
            high = temps[i].value;
        }
        if (temps[i].value < low) {
            low = temps[i].value;
        }
    }
    // Forecast condition = the smartsymbol at ~15:00 local (fcSymbolEpochSec), or
    // the nearest sample -- how FMI derives the day's forecast symbol (a single
    // representative afternoon reading), NOT the day's most notable weather. Strip
    // the +100 night offset so the mapping yields a daytime icon. Falls back to now
    // when no target time is supplied.
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
    };
}
// Map an FMI smartsymbol code to a TimeStyle WeatherIcons value. `icons` is the
// WeatherIcons map (injected so this stays pure/testable). Night = code >= 100.
function smartSymbolToIcon(code, icons) {
    if (typeof code !== 'number' || isNaN(code)) {
        return icons.WEATHER_GENERIC;
    }
    var night = code >= 100;
    var base = code % 100;
    switch (base) {
        case 1:
        case 2:
            return night ? icons.CLEAR_NIGHT : icons.CLEAR_DAY;
        case 4:
        case 6:
            return night ? icons.PARTLY_CLOUDY_NIGHT : icons.PARTLY_CLOUDY;
        case 7:
        case 9:
            return icons.CLOUDY_DAY;
    }
    // night variants exist only for clear/partly-cloudy (handled in the switch);
    // all precipitation/thunder conditions share a single day icon.
    if (base >= 71 && base <= 77) {
        return icons.THUNDERSTORM;
    } // 71-77 thunder
    if (base >= 61 && base <= 67) {
        return icons.RAINING_AND_SNOWING;
    } // 61-67 hail (no hail icon)
    if (base >= 51 && base <= 59) {
        return base >= 56 ? icons.HEAVY_SNOW : icons.LIGHT_SNOW;
    } // 51-59
    if (base >= 41 && base <= 49) {
        return icons.RAINING_AND_SNOWING;
    } // 41-49 sleet
    if (base === 33 || base === 36 || base === 39) {
        return icons.HEAVY_RAIN;
    }
    if (base >= 11 && base <= 39) {
        return icons.LIGHT_RAIN;
    } // 11-39 (minus heavy) rain/drizzle/showers
    return icons.WEATHER_GENERIC;
}

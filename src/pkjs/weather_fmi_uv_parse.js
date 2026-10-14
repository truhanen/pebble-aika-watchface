"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseFmiUvObs = parseFmiUvObs;
/* Pure parser for FMI open-data WFS radiation-observation XML
   (fmi::observations::radiation::timevaluepair&parameters=UVB_U). FMI does NOT
   publish a UV index in its forecast models (the `uvindex` forecast parameter is
   accepted but is NaN everywhere), but it DOES measure it: UVB_U is labelled
   "Ultraviolet irradiance" with uom="index" -- i.e. the UV index -- at a handful
   of fixed radiation stations (Utö, Kumpula, Jokioinen, Sotkamo, Sodankylä, ...),
   not per municipality. We therefore pick the station nearest the queried lat/lon
   that has a recent measurement and use its latest non-NaN value.

   One <wfs:member> per station: an om:OM_Observation whose observedProperty href
   carries param=UVB_U, a SF_SpatialSamplingFeature with the station name and a
   <gml:pos>LAT LON</gml:pos>, and a MeasurementTimeseries of <wml2:time>/
   <wml2:value> pairs ('NaN' = no measurement). No Pebble/browser globals --
   unit-testable with `node --test`. */
var PARAM_RE = /param=([A-Za-z0-9_]+)/;
var POS_RE = /<gml:pos>\s*([-\d.]+)\s+([-\d.]+)/;
var NAME_RE = /locationcode\/name[^>]*>([^<]+)/;
var TVP_RE = /<wml2:time>([^<]+)<\/wml2:time>\s*<wml2:value>([^<]+)<\/wml2:value>/g;
// Squared great-circle-ish distance between two lat/lon points, good enough for
// ranking nearest (longitude degrees shrink by cos(latitude) this far north).
function distSq(aLat, aLon, bLat, bLon) {
    var dLat = aLat - bLat;
    var dLon = (aLon - bLon) * Math.cos((aLat * Math.PI) / 180);
    return dLat * dLat + dLon * dLon;
}
function parseFmiUvObs(xml, lat, lon) {
    if (typeof xml !== 'string' || xml.length === 0 || xml.indexOf('ExceptionReport') !== -1) {
        return null;
    }
    var best = null;
    var members = xml.split('<wfs:member>');
    for (var i = 1; i < members.length; i++) { // [0] is the header chunk
        var chunk = members[i];
        var pm = PARAM_RE.exec(chunk);
        if (!pm || pm[1] !== 'UVB_U') {
            continue;
        }
        var pos = POS_RE.exec(chunk);
        if (!pos) {
            continue;
        }
        var sLat = parseFloat(pos[1]);
        var sLon = parseFloat(pos[2]);
        if (isNaN(sLat) || isNaN(sLon)) {
            continue;
        }
        // Latest non-NaN sample in this station's series ('NaN' = night/no data).
        var latestEpoch = -Infinity;
        var latestVal = NaN;
        var m = void 0;
        TVP_RE.lastIndex = 0;
        while ((m = TVP_RE.exec(chunk)) !== null) {
            var value = parseFloat(m[2]);
            if (isNaN(value)) {
                continue;
            }
            var epoch = Date.parse(m[1]);
            if (!isNaN(epoch) && epoch > latestEpoch) {
                latestEpoch = epoch;
                latestVal = value;
            }
        }
        if (isNaN(latestVal)) {
            continue;
        } // station has no usable measurement
        var dist = distSq(lat, lon, sLat, sLon);
        if (best === null || dist < best.dist) {
            var nm = NAME_RE.exec(chunk);
            best = { dist: dist, value: latestVal, station: nm ? nm[1] : '' };
        }
    }
    if (best === null) {
        return null;
    }
    return { uvIndex: Math.round(best.value), station: best.station };
}

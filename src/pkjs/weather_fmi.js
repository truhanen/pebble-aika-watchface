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
exports.getWeatherFromCoords = getWeatherFromCoords;
/* FMI (Finnish Meteorological Institute / Ilmatieteen laitos) weather provider.
   The forecast comes from the official "edited" model via the ::point::timevaluepair
   stored query, which snaps the queried lat/lon to FMI's nearest *named* forecast
   location (the same one ilmatieteenlaitos.fi shows for you, e.g. "Jyskä") and
   returns its name alongside the forecast values. The current-temp widget therefore
   shows that named point's forecast (matching the website), and the abbreviated
   location name is shown beneath it.
   Falls back to Open-Meteo when FMI returns no usable data.

   UV index: FMI's forecast carries NO UV (the `uvindex` forecast parameter is
   accepted but NaN everywhere), so when the UV widget is installed we make a
   SECOND request to the radiation OBSERVATIONS (UVB_U, uom="index") and use the
   nearest of FMI's ~7 radiation stations. See weather_fmi_uv_parse.ts. */
var weatherCommon = __importStar(require("./weather"));
var openmeteo = __importStar(require("./weather_openmeteo"));
var weather_fmi_parse_1 = require("./weather_fmi_parse");
var weather_fmi_fc_parse_1 = require("./weather_fmi_fc_parse");
var weather_fmi_uv_parse_1 = require("./weather_fmi_uv_parse");
var uv_widget_1 = require("./uv_widget");
var STORED_QUERY = 'fmi::forecast::edited::weather::scandinavia::point::timevaluepair';
var UV_STORED_QUERY = 'fmi::observations::radiation::timevaluepair';
// Sentinel for "no UV reading obtained" -- a real UV index is always >= 0, so the
// watch (sidebar_widgets.c UVIndex_draw) renders any negative value as "--"
// instead of a misleading 0. FMI has no UV in its forecast, so this is what the
// widget shows whenever the radiation obs gives nothing (night / all stations NaN
// / request failed).
var UV_NO_DATA = -1;
// Cap the location name sent to the watch; the C side stacks it as two 4-char
// lines (8 chars total). Truncated plainly, no ellipsis, first char uppercased.
var LOCATION_NAME_MAXLEN = 8;
function getWeatherFromCoords(pos) {
    var lat = pos.coords.latitude;
    var lon = pos.coords.longitude;
    var now = new Date();
    var start = new Date(now);
    start.setHours(0, 0, 0, 0); // local start of today
    var end = new Date(now);
    end.setHours(23, 59, 0, 0); // local end of today
    // FMI represents the day's forecast condition with the ~15:00-local reading
    // (see weather_fmi_fc_parse.parseFmiForecastTvp); compute that instant here
    // where the phone's local timezone is available.
    var fcSymTime = new Date(now);
    fcSymTime.setHours(15, 0, 0, 0);
    var fcSymbolEpochSec = Math.floor(fcSymTime.getTime() / 1000);
    var url = 'https://opendata.fmi.fi/wfs?service=WFS&version=2.0.0' +
        '&request=getFeature&storedquery_id=' + STORED_QUERY +
        '&latlon=' + lat + ',' + lon +
        '&parameters=temperature,smartsymbol' +
        '&timestep=60' +
        '&starttime=' + start.toISOString() +
        '&endtime=' + end.toISOString();
    console.log('FMI forecast URL: ' + url);
    weatherCommon.xhrRequest(url, 'GET', function (responseText) {
        var f = (0, weather_fmi_fc_parse_1.parseFmiForecastTvp)(responseText, Math.floor(now.getTime() / 1000), fcSymbolEpochSec);
        if (!f.ok) {
            console.log('FMI: no usable data, falling back to Open-Meteo');
            openmeteo.getWeatherFromCoords(pos);
            return;
        }
        var icons = weatherCommon.icons;
        var dictionary = {
            WeatherTemperature: f.currentTemp,
            WeatherCondition: (0, weather_fmi_parse_1.smartSymbolToIcon)(f.currentSymbol, icons),
            WeatherForecastHighTemp: f.forecastHigh,
            WeatherForecastLowTemp: f.forecastLow,
            WeatherForecastCondition: (0, weather_fmi_parse_1.smartSymbolToIcon)(f.forecastSymbol, icons),
            WeatherUVIndex: UV_NO_DATA, // FMI forecast has no UV; filled from observations below
            WeatherStationName: (0, weather_fmi_fc_parse_1.abbreviateLocationName)(f.name, LOCATION_NAME_MAXLEN),
        };
        // The UV obs query is a separate request to a sparse station network, so only
        // pay for it when the UV widget is actually installed; otherwise send now.
        if ((0, uv_widget_1.isUvWidgetConfigured)(window.localStorage)) {
            fetchUvIndexThenSend(Number(lat), Number(lon), dictionary);
        }
        else {
            console.log('FMI weather: ' + JSON.stringify(dictionary));
            weatherCommon.sendWeatherToPebble(dictionary);
        }
    });
}
// Fetch the measured UV index from FMI's radiation observations, set it on the
// dictionary if a usable value is found, then send the weather exactly once.
// Resilient on its own XHR (xhrRequest has no error path): a failed/timed-out UV
// request must NOT block the weather send, so onerror/ontimeout still send.
function fetchUvIndexThenSend(lat, lon, dictionary) {
    var now = new Date();
    var start = new Date(now.getTime() - 3 * 60 * 60 * 1000); // last 3 h -> "current"
    var url = 'https://opendata.fmi.fi/wfs?service=WFS&version=2.0.0' +
        '&request=getFeature&storedquery_id=' + UV_STORED_QUERY +
        '&parameters=UVB_U' +
        '&timestep=60' +
        '&starttime=' + start.toISOString() +
        '&endtime=' + now.toISOString();
    console.log('FMI UV obs URL: ' + url);
    var sent = false;
    var finish = function (responseText) {
        if (sent) {
            return;
        }
        sent = true;
        if (responseText) {
            var uv = (0, weather_fmi_uv_parse_1.parseFmiUvObs)(responseText, lat, lon);
            if (uv) {
                dictionary.WeatherUVIndex = uv.uvIndex;
                console.log('FMI UV index ' + uv.uvIndex + ' from ' + uv.station);
            }
            else {
                console.log('FMI UV: no usable observation (night/gap); showing "--"');
            }
        }
        console.log('FMI weather: ' + JSON.stringify(dictionary));
        weatherCommon.sendWeatherToPebble(dictionary);
    };
    var xhr = new XMLHttpRequest();
    xhr.onload = function () { finish(this.responseText); };
    xhr.onerror = function () { console.log('FMI UV request failed'); finish(null); };
    xhr.ontimeout = function () { console.log('FMI UV request timed out'); finish(null); };
    xhr.timeout = 15000;
    xhr.open('GET', url);
    xhr.send();
}

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
exports.icons = void 0;
exports.updateWeather = updateWeather;
exports.sendWeatherToPebble = sendWeatherToPebble;
exports.xhrRequest = xhrRequest;
/* General utility stuff related to weather. */
var openmeteo = __importStar(require("./weather_openmeteo"));
var fmi = __importStar(require("./weather_fmi"));
var weatherProviders = {
    openmeteo: openmeteo,
    fmi: fmi,
};
var DEFAULT_WEATHER_PROVIDER = 'openmeteo';
// get new forecasts if 3 hours have elapsed
var MAX_FAILURES = 3;
var currentFailures = 0;
// BUG (pre-existing, preserved): `failureRetryAmount` was never defined in the
// original JS, so `currentFailures < failureRetryAmount` was always false and
// the send-failure retry branch below never actually ran. Kept dead here to
// preserve behaviour (currentFailures is always >= 0, so `< 0` is likewise
// always false). MAX_FAILURES is the intended cap if this is ever revived.
var failureRetryAmount = 0;
// icon codes for sending weather icons to pebble
exports.icons = {
    CLEAR_DAY: 0,
    CLEAR_NIGHT: 1,
    CLOUDY_DAY: 2,
    HEAVY_RAIN: 3,
    HEAVY_SNOW: 4,
    LIGHT_RAIN: 5,
    LIGHT_SNOW: 6,
    PARTLY_CLOUDY_NIGHT: 7,
    PARTLY_CLOUDY: 8,
    RAINING_AND_SNOWING: 9,
    THUNDERSTORM: 10,
    WEATHER_GENERIC: 11,
};
function getCurrentWeatherProvider() {
    var currentWeatherProvider = window.localStorage.getItem('weather_datasource');
    if (currentWeatherProvider && weatherProviders[currentWeatherProvider] !== undefined) {
        return weatherProviders[currentWeatherProvider];
    }
    return weatherProviders[DEFAULT_WEATHER_PROVIDER];
}
// Set by a FORCED update (a cold request, or webviewclosed after a config change) and
// consumed by the next sendWeatherToPebble. The providers fetch asynchronously and do not
// carry the flag, so it is parked here rather than threaded through both provider modules.
var pendingForceSend = false;
function updateWeather(forceUpdate) {
    if (forceUpdate) {
        pendingForceSend = true;
    }
    var weatherDisabled = window.localStorage.getItem('disable_weather');
    console.log("Get weather function called! DisableWeather is '" + weatherDisabled + "'");
    if (weatherDisabled === 'yes') {
        return;
    }
    // in case "disable_weather" is empty or something weird, set it to "no"
    // since we already know it's not "yes"
    window.localStorage.setItem('disable_weather', 'no');
    var weatherLoc = window.localStorage.getItem('weather_loc');
    var storedLat = window.localStorage.getItem('weather_loc_lat');
    var storedLng = window.localStorage.getItem('weather_loc_lng');
    if (weatherLoc) { // do we have a stored location?
        // if so, check if we have valid LAT and LNG coords
        var hasLocationCoords = storedLat != null && storedLng != null
            && storedLat !== '' && storedLng !== '';
        if (hasLocationCoords) {
            var pos = {
                coords: {
                    latitude: storedLat,
                    longitude: storedLng,
                },
            };
            getCurrentWeatherProvider().getWeatherFromCoords(pos);
        }
    }
    else {
        // if we don't have a stored location, get the GPS location
        getLocation();
    }
}
function getLocation() {
    navigator.geolocation.getCurrentPosition(locationSuccess, locationError, { timeout: 15000, maximumAge: 60000 });
}
function locationError(err) {
    console.log('location error on the JS side! Failure #' + currentFailures);
    // if we fail, try using the cached location
    if (currentFailures <= MAX_FAILURES) {
        // reset cache time
        window.localStorage.setItem('weather_loc_cache_time', String(new Date().getTime() / 1000));
        currentFailures++;
        // try again
        updateWeather();
    }
    else {
        // until we get too many failures, at which point give up
        currentFailures = 0;
    }
}
function locationSuccess(pos) {
    getCurrentWeatherProvider().getWeatherFromCoords(pos);
}
var WEATHER_LAST_SENT_KEY = 'weather_last_sent';
function sendWeatherToPebble(dictionary) {
    // Weather was the ONLY source with no unchanged-suppression (crypto, currency and tuya
    // all have one), so every poll cost a Bluetooth round-trip and woke the watch even when
    // the temperature, condition, forecast and UV were all identical. Skip the send when the
    // payload is byte-identical to the last one the watch acknowledged.
    //
    // A forced update always sends: a cold request means the watch has NO data (its persist
    // was wiped), so "unchanged since we last sent" says nothing about what it holds.
    var packed = JSON.stringify(dictionary);
    var force = pendingForceSend;
    pendingForceSend = false;
    if (!force && window.localStorage.getItem(WEATHER_LAST_SENT_KEY) === packed) {
        console.log('weather: nothing changed, not sending');
        return;
    }
    Pebble.sendAppMessage(dictionary, function () {
        // only remember it once the watch has actually acknowledged it
        window.localStorage.setItem(WEATHER_LAST_SENT_KEY, packed);
        console.log('Weather info sent to Pebble successfully!');
    }, function () {
        // if we fail, wait a couple seconds, then try again
        if (currentFailures < failureRetryAmount) {
            // call it again somewhere between 3 and 10 seconds
            setTimeout(updateWeather, Math.floor(Math.random() * 10000) + 3000);
            currentFailures++;
        }
        else {
            currentFailures = 0;
        }
        console.log('Error sending weather info to Pebble! Count: #' + currentFailures);
    });
}
function xhrRequest(url, type, callback) {
    var xhr = new XMLHttpRequest();
    xhr.onload = function () {
        callback(this.responseText);
    };
    xhr.open(type, url);
    xhr.send();
}

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

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

import * as weatherCommon from './weather';
import type { GeoPosition, WeatherDict } from './weather';
import * as openmeteo from './weather_openmeteo';
import { smartSymbolToIcon } from './weather_fmi_parse';
import { parseFmiForecastTvp, abbreviateLocationName } from './weather_fmi_fc_parse';
import { parseFmiUvObs } from './weather_fmi_uv_parse';
import { isUvWidgetConfigured } from './uv_widget';

const STORED_QUERY = 'fmi::forecast::edited::weather::scandinavia::point::timevaluepair';
const UV_STORED_QUERY = 'fmi::observations::radiation::timevaluepair';

// Sentinel for "no UV reading obtained" -- a real UV index is always >= 0, so the
// watch (sidebar_widgets.c UVIndex_draw) renders any negative value as "--"
// instead of a misleading 0. FMI has no UV in its forecast, so this is what the
// widget shows whenever the radiation obs gives nothing (night / all stations NaN
// / request failed).
const UV_NO_DATA = -1;

// Cap the location name sent to the watch; the C side stacks it as two 4-char
// lines (8 chars total). Truncated plainly, no ellipsis, first char uppercased.
const LOCATION_NAME_MAXLEN = 8;

export function getWeatherFromCoords(pos: GeoPosition): void {
  const lat = pos.coords.latitude;
  const lon = pos.coords.longitude;

  const now = new Date();
  const start = new Date(now); start.setHours(0, 0, 0, 0);   // local start of today
  const end = new Date(now); end.setHours(23, 59, 0, 0);     // local end of today
  // FMI represents the day's forecast condition with the ~15:00-local reading
  // (see weather_fmi_fc_parse.parseFmiForecastTvp); compute that instant here
  // where the phone's local timezone is available.
  const fcSymTime = new Date(now); fcSymTime.setHours(15, 0, 0, 0);
  const fcSymbolEpochSec = Math.floor(fcSymTime.getTime() / 1000);

  const url = 'https://opendata.fmi.fi/wfs?service=WFS&version=2.0.0' +
    '&request=getFeature&storedquery_id=' + STORED_QUERY +
    '&latlon=' + lat + ',' + lon +
    '&parameters=temperature,smartsymbol' +
    '&timestep=60' +
    '&starttime=' + start.toISOString() +
    '&endtime=' + end.toISOString();

  console.log('FMI forecast URL: ' + url);

  weatherCommon.xhrRequest(url, 'GET', (responseText) => {
    const f = parseFmiForecastTvp(responseText, Math.floor(now.getTime() / 1000), fcSymbolEpochSec);

    if (!f.ok) {
      console.log('FMI: no usable data, falling back to Open-Meteo');
      openmeteo.getWeatherFromCoords(pos);
      return;
    }

    const icons = weatherCommon.icons;
    const dictionary: WeatherDict = {
      WeatherTemperature: f.currentTemp,
      WeatherCondition: smartSymbolToIcon(f.currentSymbol, icons),
      WeatherForecastHighTemp: f.forecastHigh,
      WeatherForecastLowTemp: f.forecastLow,
      WeatherForecastCondition: smartSymbolToIcon(f.forecastSymbol, icons),
      WeatherUVIndex: UV_NO_DATA,   // FMI forecast has no UV; filled from observations below
      WeatherStationName: abbreviateLocationName(f.name, LOCATION_NAME_MAXLEN),
    };

    // The UV obs query is a separate request to a sparse station network, so only
    // pay for it when the UV widget is actually installed; otherwise send now.
    if (isUvWidgetConfigured(window.localStorage)) {
      fetchUvIndexThenSend(Number(lat), Number(lon), dictionary);
    } else {
      console.log('FMI weather: ' + JSON.stringify(dictionary));
      weatherCommon.sendWeatherToPebble(dictionary);
    }
  });
}

// Fetch the measured UV index from FMI's radiation observations, set it on the
// dictionary if a usable value is found, then send the weather exactly once.
// Resilient on its own XHR (xhrRequest has no error path): a failed/timed-out UV
// request must NOT block the weather send, so onerror/ontimeout still send.
function fetchUvIndexThenSend(lat: number, lon: number, dictionary: WeatherDict): void {
  const now = new Date();
  const start = new Date(now.getTime() - 3 * 60 * 60 * 1000);   // last 3 h -> "current"

  const url = 'https://opendata.fmi.fi/wfs?service=WFS&version=2.0.0' +
    '&request=getFeature&storedquery_id=' + UV_STORED_QUERY +
    '&parameters=UVB_U' +
    '&timestep=60' +
    '&starttime=' + start.toISOString() +
    '&endtime=' + now.toISOString();

  console.log('FMI UV obs URL: ' + url);

  let sent = false;
  const finish = (responseText: string | null) => {
    if (sent) { return; }
    sent = true;
    if (responseText) {
      const uv = parseFmiUvObs(responseText, lat, lon);
      if (uv) {
        dictionary.WeatherUVIndex = uv.uvIndex;
        console.log('FMI UV index ' + uv.uvIndex + ' from ' + uv.station);
      } else {
        console.log('FMI UV: no usable observation (night/gap); showing "--"');
      }
    }
    console.log('FMI weather: ' + JSON.stringify(dictionary));
    weatherCommon.sendWeatherToPebble(dictionary);
  };

  const xhr = new XMLHttpRequest();
  xhr.onload = function (this: XMLHttpRequest) { finish(this.responseText); };
  xhr.onerror = function () { console.log('FMI UV request failed'); finish(null); };
  xhr.ontimeout = function () { console.log('FMI UV request timed out'); finish(null); };
  xhr.timeout = 15000;
  xhr.open('GET', url);
  xhr.send();
}

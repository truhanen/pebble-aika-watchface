// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// Precipitation from both sources: FMI Precipitation1h parsing + the full request ->
// WeatherRainMax message path for FMI and Open-Meteo, with fixture responses.

const test = require('node:test');
const assert = require('node:assert');
const { parseFmiForecastTvp } = require('../src/pkjs/weather_fmi_fc_parse');

function fcMember(param, tvps) {
  const pts = tvps.map((tv) =>
    '<wml2:point><wml2:MeasurementTVP><wml2:time>' + tv[0] + '</wml2:time>' +
    '<wml2:value>' + tv[1] + '</wml2:value></wml2:MeasurementTVP></wml2:point>').join('');
  return '<wfs:member><om:OM_Observation gml:id="WFS-x">' +
    '<om:observedProperty xlink:href="https://opendata.fmi.fi/meta?observableProperty=forecast&param=' + param + '&language=eng"/>' +
    '<om:featureOfInterest><sams:SF_SpatialSamplingFeature><sam:sampledFeature><target:LocationCollection>' +
    '<gml:name codeSpace="http://xml.fmi.fi/namespace/locationcode/name">Jyskä</gml:name>' +
    '</target:LocationCollection></sam:sampledFeature></sams:SF_SpatialSamplingFeature></om:featureOfInterest>' +
    '<om:result><wml2:MeasurementTimeseries gml:id="mts-1-1-' + param + '">' + pts +
    '</wml2:MeasurementTimeseries></om:result></om:OM_Observation></wfs:member>';
}

const NOW = Date.parse('2026-06-10T12:34:00Z') / 1000;
const hr = (h) => new Date(Date.parse('2026-06-10T00:00:00Z') + h * 3600000).toISOString().replace('.000', '');

const FMI_XML =
  '<wfs:FeatureCollection>' +
  fcMember('temperature', [[hr(12), '15'], [hr(13), '16'], [hr(23), '9'], [hr(30), '30']]) +
  fcMember('smartsymbol', [[hr(12), '1'], [hr(15), '2']]) +
  fcMember('Precipitation1h', [
    [hr(12), '5.0'],      // the hour that already ended: not part of p[]
    [hr(13), '0.2'],      // p[0]
    [hr(14), 'NaN'],      // gap
    [hr(15), '0.0'],
    [hr(16), '1.4'],      // p[3]
    [hr(24), '0.6'],      // p[11], past midnight
  ]) +
  '</wfs:FeatureCollection>';

test('FMI parser extracts Precipitation1h samples (case-preserved param, NaN dropped)', () => {
  const f = parseFmiForecastTvp(FMI_XML, NOW, undefined);
  assert.strictEqual(f.ok, true);
  assert.deepStrictEqual(f.precip.map((s) => s.value), [5, 0.2, 0, 1.4, 0.6]);
  assert.strictEqual(f.precip[1].epoch, Date.parse(hr(13)) / 1000);
});

test('FMI parser: no precipitation member -> empty list, still ok', () => {
  const xml = '<wfs:FeatureCollection>' + fcMember('temperature', [[hr(12), '15']]) + '</wfs:FeatureCollection>';
  const f = parseFmiForecastTvp(xml, NOW);
  assert.strictEqual(f.ok, true);
  assert.deepStrictEqual(f.precip, []);
});

test('FMI parser: dayEnd limits high/low to today though the window runs past midnight', () => {
  const dayEnd = Date.parse(hr(23)) / 1000;
  const f = parseFmiForecastTvp(FMI_XML, NOW, undefined, dayEnd);
  assert.strictEqual(f.forecastHigh, 16);
  assert.strictEqual(f.forecastLow, 9);
  const all = parseFmiForecastTvp(FMI_XML, NOW);
  assert.strictEqual(all.forecastHigh, 30);   // old behaviour without the limit
});

function setup(responseText) {
  const store = {};
  const sent = [];
  global.window = {
    localStorage: {
      getItem: (k) => (k in store ? store[k] : null),
      setItem: (k, v) => { store[k] = String(v); },
    },
  };
  global.Pebble = { sendAppMessage: (d, ok) => { sent.push(d); if (ok) { ok(); } } };
  const urls = [];
  global.XMLHttpRequest = function () {
    this.open = (m, u) => { urls.push(u); };
    this.send = () => { this.responseText = responseText; this.onload(); };
  };
  for (const m of ['weather', 'weather_fmi', 'weather_openmeteo']) {
    delete require.cache[require.resolve('../src/pkjs/' + m + '.js')];
  }
  return { sent, urls };
}

// Freeze "now" (both `new Date()` and Date.now()) at NOW for the duration of fn.
function withFixedNow(fn) {
  const RealDate = Date;
  global.Date = class extends RealDate {
    constructor(...a) { if (a.length === 0) { super(NOW * 1000); } else { super(...a); } }
    static now() { return NOW * 1000; }
  };
  try { fn(RealDate); } finally { global.Date = RealDate; }
}

test('FMI source: requests Precipitation1h and sends WeatherRainMax', () => {
  withFixedNow((RealDate) => {
    const { sent, urls } = setup(FMI_XML);
    require('../src/pkjs/weather_fmi.js').getWeatherFromCoords({ coords: { latitude: 60.17, longitude: 24.94 } });
    assert.ok(/parameters=temperature,smartsymbol,Precipitation1h/.test(urls[0]), urls[0]);
    const end = /endtime=([^&]+)/.exec(urls[0])[1];
    assert.ok(RealDate.parse(end) >= RealDate.parse(hr(24)), 'window must cover now+12h: ' + end);
    assert.strictEqual(sent.length, 1);
    // p[0]=.2 p[1]=NaN p[2]=0 p[3]=1.4 ... p[11]=.6 -> 20, 20, 140, 140, 140
    assert.deepStrictEqual(sent[0].WeatherRainMax, [20, 0, 20, 0, 140, 0, 140, 0, 140, 0]);
  });
});

test('FMI source: no precipitation data -> all 0xFFFF', () => {
  const xml = '<wfs:FeatureCollection>' + fcMember('temperature', [[hr(12), '15']]) + '</wfs:FeatureCollection>';
  const { sent } = setup(xml);
  require('../src/pkjs/weather_fmi.js').getWeatherFromCoords({ coords: { latitude: 60, longitude: 24 } });
  assert.deepStrictEqual(sent[0].WeatherRainMax, Array(10).fill(0xFF));
});

test('Open-Meteo source: requests hourly precipitation and sends WeatherRainMax', () => {
  withFixedNow((RealDate) => {
    // location is UTC+3: the local stamp is UTC + 3 h
    const times = []; const precip = []; const uv = [];
    for (let h = 0; h < 48; h++) {
      const d = new RealDate(RealDate.parse('2026-06-10T00:00:00Z') + (h + 3) * 3600000);
      times.push(d.toISOString().substring(0, 16));
      uv.push(1);
      precip.push(h === 13 ? 0.3 : h === 20 ? 2.5 : 0);   // h = UTC stamp hour
    }
    const json = {
      utc_offset_seconds: 10800,
      current_weather: { temperature: 15, weathercode: 3, is_day: 1 },
      daily: { temperature_2m_max: [20], temperature_2m_min: [10], weathercode: [3] },
      hourly: { time: times, uv_index: uv, precipitation: precip },
    };
    const { sent, urls } = setup(JSON.stringify(json));
    require('../src/pkjs/weather_openmeteo.js').getWeatherFromCoords({ coords: { latitude: 60, longitude: 24 } });
    assert.ok(/hourly=uv_index,precipitation/.test(urls[0]), urls[0]);
    // p[0] = stamp 13:00Z = 0.3; p[7] = stamp 20:00Z = 2.5
    assert.deepStrictEqual(sent[0].WeatherRainMax, [30, 0, 30, 0, 30, 0, 250, 0, 250, 0]);
  });
});

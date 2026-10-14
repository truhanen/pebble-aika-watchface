// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const { parseFmiForecast, smartSymbolToIcon } = require('../src/pkjs/weather_fmi_parse');

// Minimal WFS "simple feature" sample: three hourly timesteps, each with
// temperature + smartsymbol + uvindex (matches the real opendata.fmi.fi shape).
function member(time, name, value) {
  return '<wfs:member><BsWfs:BsWfsElement gml:id="x">' +
    '<BsWfs:Location><gml:Point><gml:pos>60.17 24.94</gml:pos></gml:Point></BsWfs:Location>' +
    '<BsWfs:Time>' + time + '</BsWfs:Time>' +
    '<BsWfs:ParameterName>' + name + '</BsWfs:ParameterName>' +
    '<BsWfs:ParameterValue>' + value + '</BsWfs:ParameterValue>' +
    '</BsWfs:BsWfsElement></wfs:member>';
}
// Rainy morning, dry afternoon: the 15:00-local sample (2, mostly clear) is the
// forecast symbol FMI shows for the day, NOT the morning's worst (39).
const SAMPLE =
  '<wfs:FeatureCollection>' +
  member('2026-06-09T06:00:00Z', 'temperature', '10.0') +
  member('2026-06-09T06:00:00Z', 'smartsymbol', '39') +   // heavy rain (the day's worst)
  member('2026-06-09T06:00:00Z', 'uvindex', '0') +
  member('2026-06-09T12:00:00Z', 'temperature', '19.0') +
  member('2026-06-09T12:00:00Z', 'smartsymbol', '1') +    // clear (nearest noon)
  member('2026-06-09T12:00:00Z', 'uvindex', '3') +
  member('2026-06-09T15:00:00Z', 'temperature', '11.0') +
  member('2026-06-09T15:00:00Z', 'smartsymbol', '2') +    // mostly clear at 15:00
  member('2026-06-09T15:00:00Z', 'uvindex', '2') +
  '</wfs:FeatureCollection>';

const NOON = Math.floor(Date.parse('2026-06-09T12:00:00Z') / 1000);
const AT15 = Math.floor(Date.parse('2026-06-09T15:00:00Z') / 1000);

test('parses current values at the timestep nearest to now', () => {
  const f = parseFmiForecast(SAMPLE, NOON, AT15);
  assert.strictEqual(f.ok, true);
  assert.strictEqual(f.currentTemp, 19);     // nearest to noon
  assert.strictEqual(f.currentSymbol, 1);    // clear at noon
  assert.strictEqual(f.uvIndex, 3);
});

test('derives day high/low across all timesteps', () => {
  const f = parseFmiForecast(SAMPLE, NOON, AT15);
  assert.strictEqual(f.forecastHigh, 19);
  assert.strictEqual(f.forecastLow, 10);
});

test('forecast symbol is the 15:00 sample (like FMI), not the day s worst', () => {
  const f = parseFmiForecast(SAMPLE, NOON, AT15);
  assert.strictEqual(f.forecastSymbol, 2);   // the 15:00 sample, NOT the morning s 39
});

test('returns ok:false on exception report, empty, or non-string', () => {
  assert.strictEqual(parseFmiForecast('<ExceptionReport>boom</ExceptionReport>', NOON).ok, false);
  assert.strictEqual(parseFmiForecast('<wfs:FeatureCollection></wfs:FeatureCollection>', NOON).ok, false);
  assert.strictEqual(parseFmiForecast(null, NOON).ok, false);
});

test('skips NaN parameter values (FMI emits NaN for missing data)', () => {
  const xml = '<wfs:FeatureCollection>' +
    member('2026-06-09T12:00:00Z', 'temperature', '12.0') +
    member('2026-06-09T12:00:00Z', 'uvindex', 'NaN') +
    '</wfs:FeatureCollection>';
  const f = parseFmiForecast(xml, NOON);
  assert.strictEqual(f.ok, true);
  assert.strictEqual(f.currentTemp, 12);
  assert.strictEqual(f.uvIndex, 0);          // no usable uv -> 0
});

test('smartSymbolToIcon maps day/night and severities', () => {
  const I = {
    CLEAR_DAY: 0, CLEAR_NIGHT: 1, CLOUDY_DAY: 2, HEAVY_RAIN: 3, HEAVY_SNOW: 4,
    LIGHT_RAIN: 5, LIGHT_SNOW: 6, PARTLY_CLOUDY_NIGHT: 7, PARTLY_CLOUDY: 8,
    RAINING_AND_SNOWING: 9, THUNDERSTORM: 10, WEATHER_GENERIC: 11
  };
  assert.strictEqual(smartSymbolToIcon(1, I), I.CLEAR_DAY);
  assert.strictEqual(smartSymbolToIcon(2, I), I.CLEAR_DAY);
  assert.strictEqual(smartSymbolToIcon(101, I), I.CLEAR_NIGHT);
  assert.strictEqual(smartSymbolToIcon(4, I), I.PARTLY_CLOUDY);
  assert.strictEqual(smartSymbolToIcon(6, I), I.PARTLY_CLOUDY);
  assert.strictEqual(smartSymbolToIcon(104, I), I.PARTLY_CLOUDY_NIGHT);
  assert.strictEqual(smartSymbolToIcon(7, I), I.CLOUDY_DAY);
  assert.strictEqual(smartSymbolToIcon(9, I), I.CLOUDY_DAY);
  assert.strictEqual(smartSymbolToIcon(27, I), I.LIGHT_RAIN);
  assert.strictEqual(smartSymbolToIcon(39, I), I.HEAVY_RAIN);
  assert.strictEqual(smartSymbolToIcon(42, I), I.RAINING_AND_SNOWING);
  assert.strictEqual(smartSymbolToIcon(52, I), I.LIGHT_SNOW);
  assert.strictEqual(smartSymbolToIcon(57, I), I.HEAVY_SNOW);
  assert.strictEqual(smartSymbolToIcon(64, I), I.RAINING_AND_SNOWING); // hail
  assert.strictEqual(smartSymbolToIcon(71, I), I.THUNDERSTORM);
  assert.strictEqual(smartSymbolToIcon(999, I), I.WEATHER_GENERIC);
  assert.strictEqual(smartSymbolToIcon(NaN, I), I.WEATHER_GENERIC);
  assert.strictEqual(smartSymbolToIcon(null, I), I.WEATHER_GENERIC);
});

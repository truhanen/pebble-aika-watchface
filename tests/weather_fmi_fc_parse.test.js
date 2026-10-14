// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const {
  parseFmiForecastTvp, abbreviateLocationName
} = require('../src/pkjs/weather_fmi_fc_parse');

// FMI ::point::timevaluepair forecast: one <wfs:member> per parameter, each an
// om:OM_Observation whose observedProperty href carries param=<name>, sharing a
// SF_SpatialSamplingFeature with the nearest named location, and a
// MeasurementTimeseries of <wml2:time>/<wml2:value> pairs.
function fcMember(param, name, tvps) {
  var pts = tvps.map(function (tv) {
    return '<wml2:point><wml2:MeasurementTVP>' +
      '<wml2:time>' + tv[0] + '</wml2:time>' +
      '<wml2:value>' + tv[1] + '</wml2:value>' +
      '</wml2:MeasurementTVP></wml2:point>';
  }).join('');
  return '<wfs:member><om:OM_Observation gml:id="WFS-x">' +
    '<om:observedProperty xlink:href="https://opendata.fmi.fi/meta?observableProperty=forecast&param=' + param + '&language=eng"/>' +
    '<om:featureOfInterest><sams:SF_SpatialSamplingFeature>' +
    '<sam:sampledFeature><target:LocationCollection>' +
    '<gml:name codeSpace="http://xml.fmi.fi/namespace/locationcode/name">' + name + '</gml:name>' +
    '</target:LocationCollection></sam:sampledFeature>' +
    '<gml:name>' + name + '</gml:name><gml:pos>62.26 25.86 </gml:pos>' +
    '</sams:SF_SpatialSamplingFeature></om:featureOfInterest>' +
    '<om:result><wml2:MeasurementTimeseries gml:id="mts-1-1-' + param + '">' +
    pts + '</wml2:MeasurementTimeseries></om:result>' +
    '</om:OM_Observation></wfs:member>';
}

// Rainy morning + drizzly night, but a dry afternoon: the 15:00-local sample (2,
// mostly clear) is what FMI's daily forecast shows, NOT the day's worst (39).
const SAMPLE =
  '<wfs:FeatureCollection>' +
  fcMember('temperature', 'Jyskä', [
    ['2026-06-10T06:00:00Z', '10.0'],
    ['2026-06-10T12:00:00Z', '19.0'],
    ['2026-06-10T15:00:00Z', '17.0'],
    ['2026-06-10T21:00:00Z', '12.0']
  ]) +
  fcMember('smartsymbol', 'Jyskä', [
    ['2026-06-10T06:00:00Z', '39'],   // heavy rain in the morning (the day's "worst")
    ['2026-06-10T12:00:00Z', '1'],    // clear at noon (nearest now -> currentSymbol)
    ['2026-06-10T15:00:00Z', '2'],    // mostly clear at 15:00 -> the forecast symbol
    ['2026-06-10T21:00:00Z', '111']   // night drizzle in the evening (must NOT win)
  ]) +
  fcMember('uvindex', 'Jyskä', [
    ['2026-06-10T12:00:00Z', '3'],
    ['2026-06-10T15:00:00Z', '2']
  ]) +
  '</wfs:FeatureCollection>';

const NOON = Math.floor(Date.parse('2026-06-10T12:00:00Z') / 1000);
const AT15 = Math.floor(Date.parse('2026-06-10T15:00:00Z') / 1000);
const AT21 = Math.floor(Date.parse('2026-06-10T21:00:00Z') / 1000);

test('parseFmiForecastTvp parses current values nearest now + the location name', () => {
  const f = parseFmiForecastTvp(SAMPLE, NOON, AT15);
  assert.strictEqual(f.ok, true);
  assert.strictEqual(f.currentTemp, 19);     // nearest noon
  assert.strictEqual(f.currentSymbol, 1);    // clear at noon
  assert.strictEqual(f.uvIndex, 3);
  assert.strictEqual(f.name, 'Jyskä');
});

test('parseFmiForecastTvp: day high/low, forecast symbol from the 15:00 sample (like FMI)', () => {
  const f = parseFmiForecastTvp(SAMPLE, NOON, AT15);
  assert.strictEqual(f.forecastHigh, 19);    // full-day max
  assert.strictEqual(f.forecastLow, 10);     // full-day min
  assert.strictEqual(f.forecastSymbol, 2);   // the 15:00 sample, NOT the morning's 39
});

test('parseFmiForecastTvp: forecast symbol tracks target time (nearest) and strips night offset', () => {
  const f = parseFmiForecastTvp(SAMPLE, NOON, AT21);
  assert.strictEqual(f.forecastSymbol, 11);  // 111 night drizzle -> 11 (day base)
});

test('parseFmiForecastTvp returns ok:false on exception/empty/non-string', () => {
  assert.strictEqual(parseFmiForecastTvp('<ExceptionReport>x</ExceptionReport>', NOON).ok, false);
  assert.strictEqual(parseFmiForecastTvp('<wfs:FeatureCollection></wfs:FeatureCollection>', NOON).ok, false);
  assert.strictEqual(parseFmiForecastTvp(null, NOON).ok, false);
});

test('parseFmiForecastTvp skips NaN values (FMI emits NaN for gaps)', () => {
  const xml = '<wfs:FeatureCollection>' +
    fcMember('temperature', 'Jyskä', [['2026-06-10T12:00:00Z', '12.0']]) +
    fcMember('uvindex', 'Jyskä', [['2026-06-10T12:00:00Z', 'NaN']]) +
    '</wfs:FeatureCollection>';
  const f = parseFmiForecastTvp(xml, NOON);
  assert.strictEqual(f.ok, true);
  assert.strictEqual(f.currentTemp, 12);
  assert.strictEqual(f.uvIndex, 0);
});

test('abbreviateLocationName: drop municipality, cap to 4, capitalize first char', () => {
  assert.strictEqual(abbreviateLocationName('Jyskä', 4), 'Jysk');
  assert.strictEqual(abbreviateLocationName('Jyväskylä keskus', 4), 'Kesk');
  assert.strictEqual(abbreviateLocationName('Helsinki Kaisaniemi', 4), 'Kais');
  assert.strictEqual(abbreviateLocationName('asmalampi', 4), 'Asma');  // forces capitalization
  assert.strictEqual(abbreviateLocationName('Utsjoki', 4), 'Utsj');
  assert.strictEqual(abbreviateLocationName('', 4), '');
  assert.strictEqual(abbreviateLocationName(null, 4), '');
});

test('abbreviateLocationName with maxLen 8 keeps up to 8 chars (C stacks 4+4)', () => {
  assert.strictEqual(abbreviateLocationName('Asmalampi', 8), 'Asmalamp');
  assert.strictEqual(abbreviateLocationName('Jyväskylä keskus', 8), 'Keskus');
  assert.strictEqual(abbreviateLocationName('Jyskä', 8), 'Jyskä');
  assert.strictEqual(abbreviateLocationName('Helsinki Kaisaniemi', 8), 'Kaisanie');
});

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const { parseFmiUvObs } = require('../src/pkjs/weather_fmi_uv_parse');

// fmi::observations::radiation::timevaluepair&parameters=UVB_U: one <wfs:member>
// per radiation station, each an om:OM_Observation whose observedProperty href
// carries param=UVB_U, a SF_SpatialSamplingFeature with the station name and a
// <gml:pos>LAT LON</gml:pos>, and a MeasurementTimeseries of <wml2:time>/
// <wml2:value> pairs ('NaN' = no measurement, e.g. night/cloud). uom = index.
function uvMember(name, lat, lon, tvps) {
  var pts = tvps.map(function (tv) {
    return '<wml2:point><wml2:MeasurementTVP>' +
      '<wml2:time>' + tv[0] + '</wml2:time>' +
      '<wml2:value>' + tv[1] + '</wml2:value>' +
      '</wml2:MeasurementTVP></wml2:point>';
  }).join('');
  return '<wfs:member><omso:PointTimeSeriesObservation gml:id="WFS-x">' +
    '<om:observedProperty xlink:href="https://opendata.fmi.fi/meta?observableProperty=observation&param=UVB_U&language=eng"/>' +
    '<om:featureOfInterest><sams:SF_SpatialSamplingFeature gml:id="fi-1-1-UVB_U">' +
    '<sam:sampledFeature><target:LocationCollection>' +
    '<target:member><target:Location>' +
    '<gml:name codeSpace="http://xml.fmi.fi/namespace/locationcode/name">' + name + '</gml:name>' +
    '</target:Location></target:member></target:LocationCollection></sam:sampledFeature>' +
    '<sams:shape><gml:Point srsName="http://www.opengis.net/def/crs/EPSG/0/4258">' +
    '<gml:name>' + name + '</gml:name><gml:pos>' + lat + ' ' + lon + ' </gml:pos>' +
    '</gml:Point></sams:shape>' +
    '</sams:SF_SpatialSamplingFeature></om:featureOfInterest>' +
    '<om:result><wml2:MeasurementTimeseries gml:id="obs-obs-1-1-UVB_U">' +
    pts + '</wml2:MeasurementTimeseries></om:result>' +
    '</omso:PointTimeSeriesObservation></wfs:member>';
}

// Real station coords (subset of FMI radiation stations).
const UTO = ['Parainen Utö', 59.77909, 21.37479];
const KUMPULA = ['Helsinki Kumpula', 60.20307, 24.96131];
const JOKIOINEN = ['Jokioinen Ilmala', 60.81397, 23.49825];
const SOTKAMO = ['Sotkamo Kuolaniemi', 64.11197, 28.33639];
const SODANKYLA = ['Sodankylä Tähtelä', 67.36663, 26.62901];

function collection(members) {
  return '<wfs:FeatureCollection>' + members.join('') + '</wfs:FeatureCollection>';
}

const SAMPLE = collection([
  uvMember(UTO[0], UTO[1], UTO[2], [
    ['2026-06-18T10:00:00Z', '1.4'], ['2026-06-18T11:00:00Z', '2.3'], ['2026-06-18T12:00:00Z', '3.8']]),
  uvMember(KUMPULA[0], KUMPULA[1], KUMPULA[2], [
    ['2026-06-18T10:00:00Z', '3.0'], ['2026-06-18T11:00:00Z', '3.9'], ['2026-06-18T12:00:00Z', '3.1']]),
  uvMember(JOKIOINEN[0], JOKIOINEN[1], JOKIOINEN[2], [
    ['2026-06-18T10:00:00Z', '2.8'], ['2026-06-18T11:00:00Z', '2.4'], ['2026-06-18T12:00:00Z', '3.7']]),
  uvMember(SOTKAMO[0], SOTKAMO[1], SOTKAMO[2], [
    ['2026-06-18T10:00:00Z', '1.1'], ['2026-06-18T11:00:00Z', '1.4'], ['2026-06-18T12:00:00Z', '1.7']]),
  uvMember(SODANKYLA[0], SODANKYLA[1], SODANKYLA[2], [
    ['2026-06-18T10:00:00Z', '1.4'], ['2026-06-18T11:00:00Z', '2.2'], ['2026-06-18T12:00:00Z', '2.0']]),
]);

test('picks the nearest station and its latest non-NaN UV index (rounded)', () => {
  // Jyväskylä (62.24, 25.75): nearest radiation station is Jokioinen; its latest
  // sample is 3.7 -> rounds to 4.
  const r = parseFmiUvObs(SAMPLE, 62.24, 25.75);
  assert.deepStrictEqual(r, { uvIndex: 4, station: 'Jokioinen Ilmala' });
});

test('picks Sodankylä for a far-north location', () => {
  const r = parseFmiUvObs(SAMPLE, 68.0, 27.0); // Utsjoki-ish -> Sodankylä nearest
  assert.strictEqual(r.station, 'Sodankylä Tähtelä');
  assert.strictEqual(r.uvIndex, 2); // latest 2.0
});

test('skips a NaN-only nearest station and falls back to the next nearest with data', () => {
  // Make Jokioinen (nearest to Jyväskylä) all-NaN -> should fall through to the
  // next nearest station that has a usable value.
  const xml = collection([
    uvMember(JOKIOINEN[0], JOKIOINEN[1], JOKIOINEN[2], [
      ['2026-06-18T11:00:00Z', 'NaN'], ['2026-06-18T12:00:00Z', 'NaN']]),
    uvMember(KUMPULA[0], KUMPULA[1], KUMPULA[2], [
      ['2026-06-18T12:00:00Z', '3.1']]),
    uvMember(SOTKAMO[0], SOTKAMO[1], SOTKAMO[2], [
      ['2026-06-18T12:00:00Z', '1.7']]),
  ]);
  const r = parseFmiUvObs(xml, 62.24, 25.75);
  // Kumpula (~4.30) is nearer than Sotkamo (~4.96) to Jyväskylä.
  assert.strictEqual(r.station, 'Helsinki Kumpula');
  assert.strictEqual(r.uvIndex, 3);
});

test('uses the latest sample within the series, ignoring NaN gaps', () => {
  const xml = collection([
    uvMember(JOKIOINEN[0], JOKIOINEN[1], JOKIOINEN[2], [
      ['2026-06-18T10:00:00Z', '2.0'], ['2026-06-18T11:00:00Z', '5.0'], ['2026-06-18T12:00:00Z', 'NaN']]),
  ]);
  const r = parseFmiUvObs(xml, 62.24, 25.75);
  assert.strictEqual(r.uvIndex, 5); // 11:00 is the latest non-NaN
});

test('returns null when every station is all-NaN (e.g. night)', () => {
  const xml = collection([
    uvMember(JOKIOINEN[0], JOKIOINEN[1], JOKIOINEN[2], [
      ['2026-06-18T22:00:00Z', 'NaN'], ['2026-06-18T23:00:00Z', 'NaN']]),
    uvMember(KUMPULA[0], KUMPULA[1], KUMPULA[2], [
      ['2026-06-18T23:00:00Z', 'NaN']]),
  ]);
  assert.strictEqual(parseFmiUvObs(xml, 62.24, 25.75), null);
});

test('returns null on exception report / non-string input', () => {
  assert.strictEqual(parseFmiUvObs('<ExceptionReport>boom</ExceptionReport>', 60, 25), null);
  assert.strictEqual(parseFmiUvObs(null, 60, 25), null);
  assert.strictEqual(parseFmiUvObs(undefined, 60, 25), null);
  assert.strictEqual(parseFmiUvObs('', 60, 25), null);
});

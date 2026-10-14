// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
const test = require('node:test');
const assert = require('node:assert');
const {
  parseSpecCodes, parseProperties, mergeCodes, shortUnit, buildCatalog, catalogScaleMap,
  catalogCodesById,
} = require('../src/pkjs/tuya_spec');

const SPEC = { status: [
  { code: 'va_temperature', type: 'Integer', values: '{"unit":"℃","min":-400,"max":1000,"scale":1,"step":1}' },
  { code: 'va_humidity', type: 'Integer', values: '{"unit":"%","min":0,"max":100,"scale":0,"step":1}' },
  { code: 'battery_percentage', type: 'Integer', values: '{"unit":"%","scale":0}' },
  { code: 'doorcontact_state', type: 'Boolean', values: '{}' },
]};

test('parseSpecCodes reads scale/unit from the values JSON', () => {
  const codes = parseSpecCodes(SPEC);
  const temp = codes.find((c) => c.code === 'va_temperature');
  assert.deepStrictEqual(temp, { code: 'va_temperature', type: 'Integer', scale: 1, unit: '℃' });
  const hum = codes.find((c) => c.code === 'va_humidity');
  assert.strictEqual(hum.scale, 0);
  assert.strictEqual(hum.unit, '%');
  const bool = codes.find((c) => c.code === 'doorcontact_state');
  assert.strictEqual(bool.scale, 0);   // no values -> scale 0
  assert.strictEqual(bool.unit, '');
});

test('parseSpecCodes tolerates missing/garbage spec', () => {
  assert.deepStrictEqual(parseSpecCodes(null), []);
  assert.deepStrictEqual(parseSpecCodes({}), []);
});

test('parseProperties reads code/type/value from a shadow/properties result', () => {
  const props = parseProperties({ properties: [
    { code: 'humidity', dp_id: 3, type: 'value', value: 0 },
    { code: 'humidity1', dp_id: 111, type: 'value', value: 96 },
    { code: 'temp_current', dp_id: 5, type: 'value', value: 152 },
    { bad: 'no code' },
  ]});
  assert.strictEqual(props.length, 3);
  assert.deepStrictEqual(props[1], { code: 'humidity1', type: 'value', value: 96 });
  assert.deepStrictEqual(parseProperties(null), []);
});

test('mergeCodes: every property is selectable, scale/unit from spec, sample from value', () => {
  const spec = parseSpecCodes({ status: [
    { code: 'humidity', type: 'Integer', values: '{"unit":"%","scale":0}' },
    { code: 'temp_current', type: 'Integer', values: '{"unit":"℃","scale":1}' },
  ]});
  const props = parseProperties({ properties: [
    { code: 'humidity', type: 'value', value: 0 },       // stale standard DP
    { code: 'temp_current', type: 'value', value: 152 },
    { code: 'humidity1', type: 'value', value: 96 },      // custom DP absent from spec
    { code: 'plants_1', type: 'string', value: 'Green Rose,25,70,150,300,0|Lantern,...' }, // huge string
  ]});
  const merged = mergeCodes(spec, props);
  assert.strictEqual(merged.length, 4);
  // custom humidity1 is included, scale defaults to 0, sample carries the fresh value
  const h1 = merged.find((c) => c.code === 'humidity1');
  assert.deepStrictEqual(h1, { code: 'humidity1', type: 'value', scale: 0, unit: '', sample: 96 });
  // a big string DP is still selectable but carries NO sample (avoids catalog bloat)
  const pl = merged.find((c) => c.code === 'plants_1');
  assert.strictEqual(pl.sample, undefined);
  // standard temp_current keeps its spec scale/unit + current sample
  const tc = merged.find((c) => c.code === 'temp_current');
  assert.strictEqual(tc.scale, 1);
  assert.strictEqual(tc.unit, '℃');
  assert.strictEqual(tc.sample, 152);
});

test('mergeCodes: falls back to a previous catalog scale/unit when the spec is empty', () => {
  // /specification fetch failed (spec empty) but shadow/properties succeeded; the
  // previously-cached codes still carry the right scale/unit -> keep them, don't zero.
  const props = parseProperties({ properties: [
    { code: 'temp_current', type: 'value', value: 235 },
    { code: 'humidity1', type: 'value', value: 96 },   // no prev entry -> defaults
  ]});
  const prev = [
    { code: 'temp_current', type: 'Integer', scale: 1, unit: '℃' },
    { code: 'battery', type: 'Integer', scale: 0, unit: '%' },
  ];
  const merged = mergeCodes([], props, prev);
  const tc = merged.find((c) => c.code === 'temp_current');
  assert.strictEqual(tc.scale, 1);      // preserved from prev, NOT degraded to 0
  assert.strictEqual(tc.unit, '℃');
  assert.strictEqual(tc.sample, 235);
  const h1 = merged.find((c) => c.code === 'humidity1');
  assert.strictEqual(h1.scale, 0);      // no prev + no spec -> 0
});

test('mergeCodes: a present spec still wins over the previous catalog', () => {
  const spec = parseSpecCodes({ status: [
    { code: 'temp_current', type: 'Integer', values: '{"unit":"℃","scale":2}' },
  ]});
  const props = parseProperties({ properties: [{ code: 'temp_current', type: 'value', value: 235 }] });
  const prev = [{ code: 'temp_current', type: 'Integer', scale: 1, unit: 'stale' }];
  const merged = mergeCodes(spec, props, prev);
  assert.strictEqual(merged[0].scale, 2);   // fresh spec, not the stale prev
  assert.strictEqual(merged[0].unit, '℃');
});

test('catalogCodesById extracts a deviceId->codes[] map from a cached catalog', () => {
  const cat = buildCatalog([{ id: 'dev1', name: 'Sauna' }], { dev1: parseSpecCodes(SPEC) });
  const byId = catalogCodesById(cat);
  assert.strictEqual(byId.dev1.length, 4);
  assert.strictEqual(byId.dev1[0].code, 'va_temperature');
  assert.deepStrictEqual(catalogCodesById(null), {});
  assert.deepStrictEqual(catalogCodesById({ devices: 'x' }), {});
});

test('shortUnit maps temperature to ° and keeps %', () => {
  assert.strictEqual(shortUnit('℃'), '°');
  assert.strictEqual(shortUnit('°C'), '°');
  assert.strictEqual(shortUnit('℉'), '°');
  assert.strictEqual(shortUnit('%'), '%');
  assert.strictEqual(shortUnit('ppm'), '');
  assert.strictEqual(shortUnit(''), '');
});

test('buildCatalog groups codes under devices', () => {
  const cat = buildCatalog(
    [{ id: 'dev1', name: 'Sauna' }],
    { dev1: parseSpecCodes(SPEC) });
  assert.strictEqual(cat.v, 1);
  assert.strictEqual(cat.devices[0].name, 'Sauna');
  assert.strictEqual(cat.devices[0].codes.length, 4);
});

test('catalogScaleMap builds a deviceId->code->{scale,unit} lookup', () => {
  const cat = buildCatalog([{ id: 'dev1', name: 'Sauna' }], { dev1: parseSpecCodes(SPEC) });
  const m = catalogScaleMap(cat);
  assert.deepStrictEqual(m.dev1.va_temperature, { scale: 1, unit: '℃' });
  assert.strictEqual(m.dev1.va_humidity.scale, 0);
});

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
const test = require('node:test');
const assert = require('node:assert');
const {
  normalizeRows, distinctDevices, buildPropertiesPath, packTuyaData,
  countValidValues, parseLastSent, DELIM,
} = require('../src/pkjs/tuya_parse');

const ROWS = [
  { wid: 128, deviceId: 'devA', code: 'va_temperature', p: 1, t: 0, label: 'Sauna' },
  { wid: 129, deviceId: 'devA', code: 'va_humidity', p: 0, t: 0, label: '' },
];
const SCALE = { devA: { va_temperature: { scale: 1, unit: '℃' }, va_humidity: { scale: 0, unit: '%' } } };

test('normalizeRows clamps p/t, drops rows missing wid/deviceId/code', () => {
  const out = normalizeRows([
    { wid: '128', deviceId: 'devA', code: 'va_temperature', p: '1', label: '' },
    { wid: 129, deviceId: '', code: 'x' },              // no deviceId -> dropped
    { deviceId: 'devA', code: 'y' },                    // no wid -> dropped
    { wid: 130, deviceId: 'devA', code: 'z', p: 99, t: -5, label: '' }, // p->8, t->0
    'garbage',
  ]);
  assert.strictEqual(out.length, 2);
  assert.deepStrictEqual(out[0], { wid: 128, deviceId: 'devA', code: 'va_temperature', p: 1, t: 0, label: '' });
  assert.deepStrictEqual(out[1], { wid: 130, deviceId: 'devA', code: 'z', p: 8, t: 0, label: '' });
});

test('distinctDevices returns unique ids in order', () => {
  assert.deepStrictEqual(distinctDevices(ROWS), ['devA']);
});

test('buildPropertiesPath targets the v2.0 thing-shadow properties endpoint', () => {
  assert.strictEqual(buildPropertiesPath('devA'), '/v2.0/cloud/thing/devA/shadow/properties');
});

test('packTuyaData applies scale + p/t (no unit suffix); label falls back to code', () => {
  const vals = { devA: { va_temperature: 235, va_humidity: 47 } };  // 235 scale1 -> 23.5
  const packed = packTuyaData(ROWS, vals, SCALE, {});
  // Unit suffix intentionally dropped (compact sidebar; user knows the meaning).
  assert.strictEqual(packed, ['128', 'Sauna', '23.5', '129', 'va_humidity', '47'].join(DELIM));
});

test('packTuyaData: boolean -> On/Off, string passthrough, missing -> prev then --', () => {
  const rows = [
    { wid: 128, deviceId: 'd', code: 'door', p: 0, t: 0, label: 'Door' },
    { wid: 129, deviceId: 'd', code: 'mode', p: 0, t: 0, label: 'Mode' },
    { wid: 130, deviceId: 'd', code: 'gone', p: 0, t: 0, label: 'G' },
  ];
  const packed = packTuyaData(rows, { d: { door: true, mode: 'auto' } }, {}, { 130: '9' });
  assert.strictEqual(packed, ['128', 'Door', 'On', '129', 'Mode', 'auto', '130', 'G', '9'].join(DELIM));
  const packed2 = packTuyaData(rows, { d: {} }, {}, {});
  assert.strictEqual(packed2, ['128', 'Door', '--', '129', 'Mode', '--', '130', 'G', '--'].join(DELIM));
});

test('countValidValues counts rows with a defined reading', () => {
  assert.strictEqual(countValidValues(ROWS, { devA: { va_temperature: 235, va_humidity: 47 } }), 2);
  assert.strictEqual(countValidValues(ROWS, { devA: { va_temperature: 235 } }), 1);
  assert.strictEqual(countValidValues(ROWS, {}), 0);
});

test('parseLastSent maps wid -> value', () => {
  const packed = ['128', 'Sauna', '23.5°', '129', 'H', '47%'].join(DELIM);
  assert.deepStrictEqual(parseLastSent(packed), { '128': '23.5°', '129': '47%' });
});

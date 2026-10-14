// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const {
  MAX_TUYA_LEDS, normalizeLedRows, ledDeviceIds, packLedStates, countKnownStates,
} = require('../src/pkjs/tuya_leds');

test('normalizeLedRows keeps well-formed rows in order and caps at MAX_TUYA_LEDS', () => {
  const rows = normalizeLedRows([
    { deviceId: 'a', code: 'switch_1' },
    { deviceId: 'b', code: 'switch_led' },
  ]);
  assert.deepStrictEqual(rows, [
    { deviceId: 'a', code: 'switch_1' },
    { deviceId: 'b', code: 'switch_led' },
  ]);
  const many = [];
  for (let i = 0; i < 10; i++) { many.push({ deviceId: 'd' + i, code: 'switch_1' }); }
  assert.strictEqual(normalizeLedRows(many).length, MAX_TUYA_LEDS);
});

test('normalizeLedRows drops malformed rows', () => {
  assert.deepStrictEqual(normalizeLedRows(null), []);
  assert.deepStrictEqual(normalizeLedRows('nope'), []);
  assert.deepStrictEqual(normalizeLedRows([
    null,
    { deviceId: '', code: 'switch_1' },
    { deviceId: 'a', code: '' },
    { deviceId: 'a' },
    { deviceId: 'a', code: 'switch_1' },
  ]), [{ deviceId: 'a', code: 'switch_1' }]);
});

test('ledDeviceIds returns each device once, in first-seen order', () => {
  const rows = normalizeLedRows([
    { deviceId: 'b', code: 'switch_1' },
    { deviceId: 'a', code: 'switch_1' },
    { deviceId: 'b', code: 'switch_2' },
  ]);
  assert.deepStrictEqual(ledDeviceIds(rows), ['b', 'a']);
});

test('packLedStates maps booleans to 1/0 in row order', () => {
  const rows = normalizeLedRows([
    { deviceId: 'a', code: 'switch_1' },
    { deviceId: 'a', code: 'switch_2' },
    { deviceId: 'b', code: 'switch_1' },
  ]);
  const values = { a: { switch_1: true, switch_2: false }, b: { switch_1: true } };
  assert.strictEqual(packLedStates(rows, values), '101');
});

test('packLedStates yields ? for missing device, missing code and non-boolean values', () => {
  const rows = normalizeLedRows([
    { deviceId: 'a', code: 'switch_1' },   // device absent from the answer
    { deviceId: 'b', code: 'missing' },    // code absent
    { deviceId: 'b', code: 'temp' },       // number, not a switch
    { deviceId: 'b', code: 'mode' },       // string, not a switch
    { deviceId: 'b', code: 'nul' },        // explicit null
  ]);
  const values = { b: { temp: 235, mode: 'auto', nul: null } };
  assert.strictEqual(packLedStates(rows, values), '?????');
});

test('countKnownStates counts only boolean answers', () => {
  const rows = normalizeLedRows([
    { deviceId: 'a', code: 'switch_1' },
    { deviceId: 'b', code: 'switch_1' },
    { deviceId: 'b', code: 'temp' },
  ]);
  assert.strictEqual(countKnownStates(rows, { b: { switch_1: false, temp: 235 } }), 1);
  assert.strictEqual(countKnownStates(rows, {}), 0);
});

test('packLedStates on an empty row list is an empty string', () => {
  assert.strictEqual(packLedStates([], { a: { switch_1: true } }), '');
});

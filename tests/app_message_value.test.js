// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const { toAppMessageValue } = require('../src/pkjs/app_message_value');

test('boolean true -> 1, false -> 0 (Clay toggles arrive as booleans)', () => {
  assert.strictEqual(toAppMessageValue(true, false), 1);
  assert.strictEqual(toAppMessageValue(false, false), 0);
});

test('numeric string -> int (radiogroup/select/input arrive as strings)', () => {
  assert.strictEqual(toAppMessageValue('2', false), 2);
  assert.strictEqual(toAppMessageValue('0', false), 0);
});

test('string key passes through unchanged', () => {
  assert.strictEqual(toAppMessageValue('Helsinki', true), 'Helsinki');
  assert.strictEqual(toAppMessageValue(',', true), ',');
});

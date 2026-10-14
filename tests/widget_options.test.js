// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const { STATIC_WIDGETS, buildOptions } = require('../src/pkjs/widget_options');

test('STATIC_WIDGETS contains no crypto coin entries (those are dynamic)', () => {
  const ids = STATIC_WIDGETS.map((o) => o.id);
  [15, 16, 17].forEach((cid) => assert.strictEqual(ids.indexOf(cid), -1));
  assert.ok(ids.indexOf(0) !== -1, 'Empty present');
  assert.ok(ids.indexOf(2) !== -1, 'Battery present');
});

test('STATIC_WIDGETS includes the Deep Sleep widget (id 20)', () => {
  const opt = STATIC_WIDGETS.find((o) => o.id === 20);
  assert.ok(opt, 'Deep Sleep option present');
  assert.strictEqual(opt.label, 'Deep Sleep');
});

test('STATIC_WIDGETS includes the Distance widget (id 21)', () => {
  const opt = STATIC_WIDGETS.find((o) => o.id === 21);
  assert.ok(opt, 'Distance option present');
  assert.strictEqual(opt.label, 'Distance');
});

test('STATIC_WIDGETS includes the Battery Days widget (id 22)', () => {
  const opt = STATIC_WIDGETS.find((o) => o.id === 22);
  assert.ok(opt, 'Battery Days option present');
  assert.strictEqual(opt.label, 'Battery (days left)');
});

test('buildOptions appends one option per crypto row, labelled by label||coin', () => {
  const opts = buildOptions([
    { wid: 15, coin: 'bitcoin', label: 'BTC' },
    { wid: 200, coin: 'dogecoin', label: '' },
  ]);
  const tail = opts.slice(opts.length - 2);
  assert.deepStrictEqual(tail, [
    { id: 15, label: 'BTC' },
    { id: 200, label: 'DOGECOIN' },
  ]);
  assert.strictEqual(opts.length, STATIC_WIDGETS.length + 2);
});

test('buildOptions tolerates an empty / non-array row list', () => {
  assert.deepStrictEqual(buildOptions([]), STATIC_WIDGETS);
  assert.deepStrictEqual(buildOptions(null), STATIC_WIDGETS);
});

const { ROTATING_ID, memberOptions } = require('../src/pkjs/widget_options');

test('buildOptions includes the Rotating pseudo-widget', () => {
  const ids = buildOptions([]).map((o) => o.id);
  assert.ok(ids.indexOf(255) !== -1);
});

test('memberOptions excludes Empty and Rotating', () => {
  const ids = memberOptions([]).map((o) => o.id);
  assert.ok(ids.indexOf(0) === -1);
  assert.ok(ids.indexOf(255) === -1);
  assert.ok(ids.indexOf(2) !== -1);   // Battery still present
});

test('memberOptions includes crypto coins', () => {
  const ids = memberOptions([{ wid: 200, coin: 'btc', label: 'BTC' }]).map((o) => o.id);
  assert.ok(ids.indexOf(200) !== -1);
  assert.ok(ids.indexOf(255) === -1);
});

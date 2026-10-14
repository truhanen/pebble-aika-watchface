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

test('STATIC_WIDGETS offers exactly the contract ids (no 1, no retired 4, no legacy 15/16/17)', () => {
  const ids = STATIC_WIDGETS.map((o) => o.id).sort((a, b) => a - b);
  assert.deepStrictEqual(ids, [0, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 18, 19, 20, 21, 22, 23, 24, 25,
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 255]);
  assert.strictEqual(new Set(ids).size, ids.length, 'ids are unique');
});

test('STATIC_WIDGETS labels for the Phase 4 widgets', () => {
  const label = (id) => STATIC_WIDGETS.find((o) => o.id === id).label;
  assert.strictEqual(label(2), 'Battery');
  assert.strictEqual(label(39), 'Battery %');
  assert.deepStrictEqual([26, 27, 28, 29, 30, 31, 32, 33, 34].map(label), [
    'Date d.m.', 'Date dd.mm.', 'Date dd.mm', 'Date d.m.yyyy', 'Date Wkday dd',
    'Date Wkday d.m.', 'Date Wkday', 'Date dd Mon', 'Date yyyy-mm-dd']);
  assert.strictEqual(label(35), 'Digital time');
  assert.strictEqual(label(36), 'Battery (icon only)');
  assert.strictEqual(label(37), 'Weather (icon only)');
  assert.strictEqual(label(38), 'Bluetooth (icon only)');
});

test("the retired \"Today's Date\" widget (4) is not offered", () => {
  assert.strictEqual(STATIC_WIDGETS.find((o) => o.id === 4), undefined);
  assert.ok(!STATIC_WIDGETS.some((o) => /Today's Date/.test(o.label)));
});

test('FONT_OPTIONS has the 17 contract labels; FONT_DEFAULT is 31', () => {
  const { FONT_OPTIONS, FONT_DEFAULT } = require('../src/pkjs/widget_options');
  assert.strictEqual(FONT_DEFAULT, 31);
  assert.strictEqual(FONT_OPTIONS.length, 17);
  assert.strictEqual(FONT_OPTIONS[0], 'Gothic 3x-small');
  assert.strictEqual(FONT_OPTIONS[5], 'Gothic medium');
  assert.strictEqual(FONT_OPTIONS[7], 'Bitham large');
  assert.strictEqual(FONT_OPTIONS[16], 'Leco 5x-large');
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

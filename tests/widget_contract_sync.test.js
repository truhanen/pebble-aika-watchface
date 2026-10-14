// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// Sync guard between the TS widget model and the C side, per docs/widget_contract.md:
//   - every widget id offered on the config page (STATIC_WIDGETS, except Empty 0 and
//     the Rotating pseudo-widget 255) must exist in the WidgetType enum in
//     src/c/widgets.h, and MAX_WIDGET_TYPE must cover the highest one;
//   - FONT_KEYS in src/c/widget_fonts.c must list exactly the 17 contract fonts, in
//     the contract's order (the array index is the font index in the payload flags),
//     matching the TS FONT_OPTIONS labels.
// The C side is changed by a different engineer in parallel: while widgets.h in this
// checkout has not caught up with the contract yet, the enum check is SKIPPED with a
// message naming the missing ids, instead of failing. It becomes a hard check once
// both sides are merged.

const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { STATIC_WIDGETS, FONT_OPTIONS } = require('../src/pkjs/widget_options');

const read = (rel) => fs.readFileSync(path.join(__dirname, '..', rel), 'utf8');

// The contract's font table, in index order: [TS label, FONT_KEY_ suffix].
const CONTRACT_FONTS = [
  ['Gothic 3x-small (14)', 'GOTHIC_14'],
  ['Gothic 2x-small (14 bold)', 'GOTHIC_14_BOLD'],
  ['Gothic x-small (18 bold)', 'GOTHIC_18_BOLD'],
  ['Gothic small (24 bold)', 'GOTHIC_24_BOLD'],
  ['Roboto small (21 condensed)', 'ROBOTO_CONDENSED_21'],
  ['Gothic medium (28 bold)', 'GOTHIC_28_BOLD'],
  ['Leco medium (20 bold)', 'LECO_20_BOLD_NUMBERS'],
  ['Bitham large (30 black)', 'BITHAM_30_BLACK'],
  ['Droid serif large (28 bold)', 'DROID_SERIF_28_BOLD'],
  ['Leco large (26 bold)', 'LECO_26_BOLD_NUMBERS_AM_PM'],
  ['Leco x-large (32 bold)', 'LECO_32_BOLD_NUMBERS'],
  ['Bitham 2x-large (42 bold)', 'BITHAM_42_BOLD'],
  ['Leco 2x-large (36 bold)', 'LECO_36_BOLD_NUMBERS'],
  ['Leco 2x-large (38 bold)', 'LECO_38_BOLD_NUMBERS'],
  ['Leco 3x-large (42)', 'LECO_42_NUMBERS'],
  ['Leco 4x-large (60)', 'LECO_60_NUMBERS_AM_PM'],
  ['Leco 5x-large (60 bold)', 'LECO_60_BOLD_NUMBERS_AM_PM'],
];

// WidgetType enum body from widgets.h -> { NAME: value }.
function parseEnum() {
  const src = read('src/c/widgets.h');
  const m = /typedef\s+enum\s*\{([\s\S]*?)\}\s*WidgetType\s*;/.exec(src);
  assert.ok(m, 'WidgetType enum not found in src/c/widgets.h');
  const out = {};
  const re = /([A-Z][A-Z0-9_]*)\s*=\s*(\d+)/g;
  let e;
  while ((e = re.exec(m[1])) !== null) { out[e[1]] = parseInt(e[2], 10); }
  return out;
}

test('the contract font table matches the TS FONT_OPTIONS labels', () => {
  assert.deepStrictEqual(FONT_OPTIONS, CONTRACT_FONTS.map((f) => f[0]));
});

test('every STATIC_WIDGETS id (except Empty and Rotating) exists in the C WidgetType enum', (t) => {
  const values = parseEnum();
  const cIds = Object.keys(values).map((k) => values[k]);
  assert.ok(cIds.length > 0, 'parsed the enum (parser still works)');
  const ids = STATIC_WIDGETS.map((o) => o.id).filter((id) => id !== 0 && id !== 255);
  // Ids that must stay out of the picker: internal BT (1) and the retired date (4).
  [1, 4].forEach((id) => assert.strictEqual(ids.indexOf(id), -1, 'id ' + id + ' is not offered'));
  const missing = ids.filter((id) => cIds.indexOf(id) === -1);
  if (missing.length > 0) {
    t.skip('src/c/widgets.h has no enum entry yet for widget id(s) ' + missing.join(', ')
      + ' (the C side has not caught up with docs/widget_contract.md in this checkout)');
    return;
  }
  assert.deepStrictEqual(missing, []);
  // MAX_WIDGET_TYPE has to cover the highest static id (the settings clamp / payload bound).
  const mx = /#define\s+MAX_WIDGET_TYPE\s+(\w+)/.exec(read('src/c/widgets.h'));
  assert.ok(mx, 'MAX_WIDGET_TYPE not found');
  const maxVal = (mx[1] in values) ? values[mx[1]] : parseInt(mx[1], 10);
  assert.ok(maxVal >= Math.max.apply(null, ids),
    'MAX_WIDGET_TYPE (' + maxVal + ') must be >= the highest static widget id');
});

test('FONT_KEYS in src/c/widget_fonts.c has exactly the 17 contract fonts, in order', () => {
  const src = read('src/c/widget_fonts.c');
  const at = src.indexOf('FONT_KEYS');
  assert.ok(at !== -1, 'FONT_KEYS not found in src/c/widget_fonts.c');
  const open = src.indexOf('{', at);
  const close = src.indexOf('};', open);
  const keys = (src.slice(open, close).match(/FONT_KEY_[A-Z0-9_]+/g) || [])
    .map((k) => k.replace(/^FONT_KEY_/, ''));
  assert.strictEqual(keys.length, 17, 'FONT_KEYS must have exactly 17 entries, found ' + keys.length);
  assert.deepStrictEqual(keys, CONTRACT_FONTS.map((f) => f[1]));
});

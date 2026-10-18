// SPDX-License-Identifier: GPL-3.0-only

// Font indices are PERSISTED: the config page stores each widget's font as an index, the
// watch stores it in the widget-list flags (bits 0-4). An index must therefore always mean
// the same system font. This pins index -> FONT_KEY independently of the display labels
// (which may be renamed freely) and checks every index-ordered table against it.
// New fonts may only be APPENDED (add them at the end here too); never reorder, insert or
// remove. Index 31 is reserved for "position default".

const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { FONT_OPTIONS } = require('../src/pkjs/widget_options');

const read = (rel) => fs.readFileSync(path.join(__dirname, '..', rel), 'utf8');

const PINNED_FONT_KEYS = Object.freeze([
  'GOTHIC_14',                    //  0
  'GOTHIC_14_BOLD',               //  1
  'GOTHIC_18_BOLD',               //  2
  'GOTHIC_24_BOLD',               //  3
  'ROBOTO_CONDENSED_21',          //  4
  'GOTHIC_28_BOLD',               //  5
  'LECO_20_BOLD_NUMBERS',         //  6
  'BITHAM_30_BLACK',              //  7
  'DROID_SERIF_28_BOLD',          //  8
  'LECO_26_BOLD_NUMBERS_AM_PM',   //  9
  'LECO_32_BOLD_NUMBERS',         // 10
  'BITHAM_42_BOLD',               // 11
  'LECO_36_BOLD_NUMBERS',         // 12
  'LECO_38_BOLD_NUMBERS',         // 13
  'LECO_42_NUMBERS',              // 14
  'LECO_60_NUMBERS_AM_PM',        // 15
  'LECO_60_BOLD_NUMBERS_AM_PM',   // 16
]);

// The C font list, index order.
function cFontKeys() {
  const src = read('src/c/widget_fonts.c');
  const open = src.indexOf('{', src.indexOf('FONT_KEYS'));
  const close = src.indexOf('};', open);
  return (src.slice(open, close).match(/FONT_KEY_[A-Z0-9_]+/g) || []).map((k) => k.slice(9));
}

// The measured metrics rows, index order (each row's comment names its index and key).
function metricsRows() {
  const re = /\/\*\s*(\d+)\s[^(]*\(([A-Z0-9_]+)\)\s*\*\//g;
  const rows = [];
  let m;
  while ((m = re.exec(read('src/c/widget_font_metrics.h'))) !== null) {
    rows.push({ index: parseInt(m[1], 10), key: m[2] });
  }
  return rows;
}

test('existing font indices keep their system fonts (append-only)', () => {
  const keys = cFontKeys();
  assert.ok(keys.length >= PINNED_FONT_KEYS.length,
    'FONT_KEYS lost entries: fonts may only be appended');
  assert.deepStrictEqual(keys.slice(0, PINNED_FONT_KEYS.length), PINNED_FONT_KEYS.slice(),
    'FONT_KEYS reordered or changed an existing index: indices are persisted');
  assert.ok(keys.length <= 31, 'index 31 is reserved for "position default"');
});

test('the metrics table rows follow the same index order', () => {
  const keys = cFontKeys();
  const rows = metricsRows();
  assert.strictEqual(rows.length, keys.length, 'one metrics row per font');
  rows.forEach((r, i) => {
    assert.strictEqual(r.index, i, 'metrics row ' + i + ' is labelled index ' + r.index);
    assert.strictEqual(r.key, keys[i], 'metrics row ' + i + ' is for ' + r.key + ', not ' + keys[i]);
  });
});

test('the config page offers one label per font index', () => {
  assert.strictEqual(FONT_OPTIONS.length, cFontKeys().length);
});

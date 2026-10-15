// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// Payload v2 encoder (docs/widget_contract.md): plain slot = id, flags; rotating group
// = 0xFF, count, interval, (id, flags) x count; flags = font (bits 0-4, 31 = default)
// | hide (bit 5).
const test = require('node:test');
const assert = require('node:assert');
const { widgetListToPayload, widgetListIds } = require('../src/pkjs/widget_list_payload');

const w = (id, hide, font) => ({ id: id, hide: !!hide, font: font === undefined ? 31 : font });
const rot = (interval, members) => ({ rotating: true, interval: interval, members: members });

test('plain slots encode as id, flags (default font 31, shown)', () => {
  assert.deepStrictEqual(widgetListToPayload([w(7), w(10)]), [7, 31, 10, 31]);
});

test('font index goes into bits 0-4, hide into bit 5', () => {
  assert.deepStrictEqual(widgetListToPayload([w(7, false, 0)]), [7, 0]);
  assert.deepStrictEqual(widgetListToPayload([w(7, false, 16)]), [7, 16]);
  assert.deepStrictEqual(widgetListToPayload([w(7, true, 5)]), [7, 0x20 | 5]);
  assert.deepStrictEqual(widgetListToPayload([w(7, true, 31)]), [7, 0x20 | 31]);
});

test('bits 6-7 of the flags are always zero', () => {
  const p = widgetListToPayload([w(7, true, 16), w(8, true, 31), w(9, false, 0)]);
  for (let i = 1; i < p.length; i += 2) { assert.strictEqual(p[i] & 0xC0, 0); }
});

test('an invalid font falls back to the default (31)', () => {
  assert.deepStrictEqual(widgetListToPayload([w(7, false, 17)]), [7, 31]);
  assert.deepStrictEqual(widgetListToPayload([w(7, false, -1)]), [7, 31]);
  assert.deepStrictEqual(widgetListToPayload([w(7, false, 30)]), [7, 31]);
  assert.deepStrictEqual(widgetListToPayload([{ id: 7 }]), [7, 31]);          // font missing
  assert.deepStrictEqual(widgetListToPayload([w(7, false, 'x')]), [7, 31]);   // not a number
  assert.deepStrictEqual(widgetListToPayload([w(7, false, '5')]), [7, 5]);    // string select value
});

test('string ids (Clay select values) are accepted', () => {
  assert.deepStrictEqual(widgetListToPayload([{ id: '7', hide: false, font: '31' }]), [7, 31]);
});

test('non-array and junk input yields an empty payload', () => {
  assert.deepStrictEqual(widgetListToPayload(undefined), []);
  assert.deepStrictEqual(widgetListToPayload(null), []);
  assert.deepStrictEqual(widgetListToPayload('x'), []);
  assert.deepStrictEqual(widgetListToPayload([null, undefined, {}, { id: 'x' }]), []);
});

test('Empty (0) is a valid slot', () => {
  assert.deepStrictEqual(widgetListToPayload([w(12), w(0), w(10)]), [12, 31, 0, 31, 10, 31]);
});

test('every contract id is accepted, in order', () => {
  const ok = [0, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38];
  // list in two halves so the 64-byte cap does not truncate
  const half = widgetListToPayload(ok.slice(0, 20).map((id) => w(id)));
  assert.deepStrictEqual(half.filter((_, i) => i % 2 === 0), ok.slice(0, 20));
  const rest = widgetListToPayload(ok.slice(20).map((id) => w(id)));
  assert.deepStrictEqual(rest.filter((_, i) => i % 2 === 0), ok.slice(20));
});

test('accepts the dynamic ranges: tuya 128..143, crypto 200..215, currency 216..222', () => {
  const ids = [128, 143, 200, 215, 216, 222];
  assert.deepStrictEqual(widgetListToPayload(ids.map((id) => w(id))).filter((_, i) => i % 2 === 0), ids);
});

test('drops ids outside the contract: 1 (internal), 4 (retired), 40+, gaps, 144..199, 223+', () => {
  [1, 4, 40, 63, 64, 100, 127, 144, 199, 223, 254, 255, 256, -1].forEach((id) => {
    assert.deepStrictEqual(widgetListToPayload([w(id)]), [], 'id ' + id + ' must be dropped');
  });
  assert.deepStrictEqual(widgetListToPayload([w(7), w(99), w(10)]), [7, 31, 10, 31]);
});

test('ids with bit 0x20 are real ids now (no hide flag in the id)', () => {
  assert.deepStrictEqual(widgetListToPayload([w(32), w(33), w(34), w(35)]), [32, 31, 33, 31, 34, 31, 35, 31]);
});

test('side lists are capped at 64 bytes (32 plain slots)', () => {
  const long = [];
  for (let i = 0; i < 40; i++) { long.push(w(5)); }
  assert.strictEqual(widgetListToPayload(long).length, 64);
  assert.strictEqual(widgetListToPayload(long, 'side').length, 64);
});

test('a rotating group encodes as 0xFF, count, interval, (id, flags) x count', () => {
  const g = rot(1, [w(15, false, 3), w(16, true, 31), w(3, false, 0)]);
  assert.deepStrictEqual(widgetListToPayload([w(2), g]),
    [2, 31, 255, 3, 1, 15, 3, 16, 0x20 | 31, 3, 0]);
});

test('every group member keeps its own font and hide flag', () => {
  const g = rot(0, [w(7, true, 5), w(10, false, 12)]);
  assert.deepStrictEqual(widgetListToPayload([g]), [255, 2, 0, 7, 0x20 | 5, 10, 12]);
});

test('drops invalid members (and Empty) and fixes the group count', () => {
  const g = rot(0, [w(0), w(15), w(99), w(16)]);
  assert.deepStrictEqual(widgetListToPayload([g]), [255, 2, 0, 15, 31, 16, 31]);
});

test('a group left with one valid member degrades to a plain slot', () => {
  assert.deepStrictEqual(widgetListToPayload([rot(1, [w(15, true, 4), w(99)])]), [15, 0x20 | 4]);
  assert.deepStrictEqual(widgetListToPayload([rot(1, [w(15, false, 4)])]), [15, 4]);
});

test('a group with no valid members is dropped', () => {
  assert.deepStrictEqual(widgetListToPayload([w(7), rot(1, [w(0), w(99)]), w(10)]), [7, 31, 10, 31]);
  assert.deepStrictEqual(widgetListToPayload([rot(1, [])]), []);
  assert.deepStrictEqual(widgetListToPayload([{ rotating: true }]), []);
});

test('clamps a bad interval code to 1 min (3)', () => {
  [9, -1, 'x', undefined].forEach((iv) => {
    assert.deepStrictEqual(widgetListToPayload([rot(iv, [w(15), w(16)])]), [255, 2, 3, 15, 31, 16, 31]);
  });
  // 0 (5 s) is a valid interval and must not be coerced
  assert.deepStrictEqual(widgetListToPayload([rot(0, [w(15), w(16)])]), [255, 2, 0, 15, 31, 16, 31]);
  assert.deepStrictEqual(widgetListToPayload([rot(5, [w(15), w(16)])]), [255, 2, 5, 15, 31, 16, 31]);
});

test('clamps a group to 6 members', () => {
  const eight = rot(1, [200, 201, 202, 203, 204, 205, 206, 207].map((id) => w(id)));
  assert.deepStrictEqual(widgetListToPayload([eight]),
    [255, 6, 1, 200, 31, 201, 31, 202, 31, 203, 31, 204, 31, 205, 31]);
});

test('packs whole groups only within the 64-byte cap', () => {
  // each 6-member group is 15 bytes; four fit (60), a fifth (-> 75) is dropped whole
  const mk = () => rot(1, [200, 201, 202, 203, 204, 205].map((id) => w(id)));
  const one = widgetListToPayload([mk()]);
  assert.strictEqual(one.length, 15);
  const five = widgetListToPayload([mk(), mk(), mk(), mk(), mk()]);
  assert.strictEqual(five.length, 60);
  assert.deepStrictEqual(five, one.concat(one, one, one));
});

test('stops at the first group that does not fit, even if later plain slots would', () => {
  const g = rot(1, [200, 201, 202, 203, 204, 205].map((id) => w(id)));
  const slots = [g, g, g, g, w(5), w(5)];   // 60 bytes, then plain slots (2 bytes each)
  const p = widgetListToPayload(slots);
  assert.strictEqual(p.length, 64);
  assert.deepStrictEqual(p.slice(60), [5, 31, 5, 31]);
  const over = widgetListToPayload([g, g, g, g, g, w(5)]);   // fifth group dropped -> stop
  assert.strictEqual(over.length, 60);
});

// ----------------------------------------------------------- centre (top / bottom)

test("centre lists keep at most one slot, capped at 16 bytes", () => {
  assert.deepStrictEqual(widgetListToPayload([w(30), w(7)], 'center'), [30, 31]);
  assert.deepStrictEqual(widgetListToPayload([w(30, false, 11)], 'center'), [30, 11]);
  assert.deepStrictEqual(widgetListToPayload([], 'center'), []);
  // the same list on a side keeps both
  assert.deepStrictEqual(widgetListToPayload([w(30), w(7)], 'side'), [30, 31, 7, 31]);
});

test('centre: the first VALID slot wins; invalid leading slots are skipped', () => {
  assert.deepStrictEqual(widgetListToPayload([w(99), w(4), w(30)], 'center'), [30, 31]);
});

test('centre: a rotating group counts as one slot and fits the 16-byte cap', () => {
  const g = rot(2, [200, 201, 202, 203, 204, 205].map((id) => w(id)));   // 15 bytes
  const p = widgetListToPayload([g, w(7)], 'center');
  assert.strictEqual(p.length, 15);
  assert.deepStrictEqual(p.slice(0, 3), [255, 6, 2]);
});

// ----------------------------------------------------------- legacy stored values

test('legacy flat lists: plain ids get the default font', () => {
  assert.deepStrictEqual(widgetListToPayload([12, 15, 17]), [12, 31, 15, 31, 17, 31]);
});

test('legacy flat lists: the 0x20 hide flag in the id becomes the hide bit', () => {
  // 7|0x20 = 39 (hidden weather), 2|0x20 = 34 (hidden battery), 25|0x20 = 57
  assert.deepStrictEqual(widgetListToPayload([39, 34, 57]), [7, 0x20 | 31, 2, 0x20 | 31, 25, 0x20 | 31]);
});

test('legacy flat lists: rotating group markers still parse', () => {
  assert.deepStrictEqual(widgetListToPayload([2, 255, 2, 1, 15, 16, 3]),
    [2, 31, 255, 2, 1, 15, 31, 16, 31, 3, 31]);
  // a hidden member: 15|0x20 = 47
  assert.deepStrictEqual(widgetListToPayload([255, 2, 1, 47, 16]), [255, 2, 1, 15, 0x20 | 31, 16, 31]);
  // invalid member + degrade + clamp
  assert.deepStrictEqual(widgetListToPayload([255, 3, 0, 15, 99, 16]), [255, 2, 0, 15, 31, 16, 31]);
  assert.deepStrictEqual(widgetListToPayload([255, 2, 1, 15, 99]), [15, 31]);
});

test('legacy flat lists: the retired date (4) and its hidden form (36) are dropped', () => {
  assert.deepStrictEqual(widgetListToPayload([4, 36, 7]), [7, 31]);
});

test('legacy flat lists: a malformed group tail stops parsing', () => {
  assert.deepStrictEqual(widgetListToPayload([7, 255]), [7, 31]);
});

// ----------------------------------------------------------- widgetListIds

test('widgetListIds returns the plain ids, without flags or group markers', () => {
  const list = [w(7, true, 5), rot(1, [w(15, true, 3), w(16)]), w(10, false, 12)];
  assert.deepStrictEqual(widgetListIds(list), [7, 15, 16, 10]);
  assert.deepStrictEqual(widgetListIds([w(37, true)]), [37]);
  assert.deepStrictEqual(widgetListIds(undefined), []);
});

test('widgetListIds honours the centre slot limit and legacy hide flags', () => {
  assert.deepStrictEqual(widgetListIds([w(30), w(7)], 'center'), [30]);
  assert.deepStrictEqual(widgetListIds([7 | 0x20, 255, 2, 3, 14, 18]), [7, 14, 18]);
});

// ----------------------------------------------------------- identifier modes

const wi = (id, ident, font) => ({ id: id, ident: ident, font: font === undefined ? 31 : font });

test('ident icon / unit / none go into bits 5-6 (none 0x20, unit 0x40)', () => {
  assert.deepStrictEqual(widgetListToPayload([wi(2, 'icon')]), [2, 31]);
  assert.deepStrictEqual(widgetListToPayload([wi(2, 'unit')]), [2, 0x40 | 31]);
  assert.deepStrictEqual(widgetListToPayload([wi(7, 'none', 5)]), [7, 0x20 | 5]);
  assert.deepStrictEqual(widgetListToPayload([wi(12, 'unit', 0)]), [12, 0x40]);
});

test('ident wins over the older hide field; hide alone still means none', () => {
  assert.deepStrictEqual(widgetListToPayload([{ id: 7, ident: 'unit', hide: true, font: 31 }]), [7, 0x40 | 31]);
  assert.deepStrictEqual(widgetListToPayload([{ id: 7, hide: true, font: 31 }]), [7, 0x20 | 31]);
  assert.deepStrictEqual(widgetListToPayload([{ id: 7, hide: false, font: 31 }]), [7, 31]);
  assert.deepStrictEqual(widgetListToPayload([{ id: 7, ident: 'bogus', font: 31 }]), [7, 31]);
});

test('ident works for rotating members; the encoder never sets both bits or bit 7', () => {
  const g = { rotating: true, interval: 3, members: [wi(2, 'unit'), wi(12, 'none')] };
  assert.deepStrictEqual(widgetListToPayload([g]), [255, 2, 3, 2, 0x40 | 31, 12, 0x20 | 31]);
  widgetListToPayload([wi(2, 'unit'), wi(7, 'none'), wi(10, 'icon')]).forEach((b, i) => {
    if (i % 2 === 1) { assert.strictEqual(b & 0x80, 0); assert.notStrictEqual(b & 0x60, 0x60); }
  });
});

test('id 39 (the retired Battery % widget) is no longer valid; 38 still is', () => {
  assert.deepStrictEqual(widgetListToPayload([wi(39, 'icon'), wi(38, 'icon')]), [38, 31]);
});

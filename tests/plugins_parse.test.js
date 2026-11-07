// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// Pure plugin-widget helpers: selection parsing, instance picking, value formatting,
// the PluginData wire string and the placed-widget gate.
const test = require('node:test');
const assert = require('node:assert');
const P = require('../src/pkjs/plugins_parse');
const { widgetListToPayload } = require('../src/pkjs/widget_list_payload');

const WEATHER = '00000000-0000-0000-0001-000000000002';
const env = {
  pluginUuid: WEATHER,
  instances: [
    { instanceId: 'aaa', properties: { temperature: { numericValue: { value: 18.2999999, unit: '°C' } } } },
    { instanceId: 'bbb', properties: { temperature: { numericValue: { value: -4, unit: '°C' } } } },
  ],
};

test('parseSelections keeps slot indices, nulls bad entries, caps at 16', () => {
  const good = { plugin: WEATHER, category: 'weather', item: 'location', instanceId: '2', property: 'temperature' };
  const out = P.parseSelections(JSON.stringify([good, { category: 'x' }, 'junk', good]));
  assert.strictEqual(out.length, 4);
  assert.deepStrictEqual(out[0], good);
  assert.strictEqual(out[1], null);
  assert.strictEqual(out[2], null);
  assert.deepStrictEqual(out[3], good);
  assert.strictEqual(P.parseSelections(JSON.stringify(new Array(20).fill(good))).length, 16);
  assert.deepStrictEqual(P.parseSelections('not json'), []);
  assert.deepStrictEqual(P.parseSelections(null), []);
  assert.deepStrictEqual(P.parseSelections('{"a":1}'), []);
});

test('instanceId is optional and a number is stringified', () => {
  const s = P.normalizeSelection({ category: 'c', item: 'i', property: 'p', instanceId: 2 });
  assert.strictEqual(s.instanceId, '2');
  assert.strictEqual(P.normalizeSelection({ category: 'c', item: 'i', property: 'p' }).instanceId, undefined);
});

test('groupSelections groups by plugin/category/item and merges properties', () => {
  const a = { plugin: WEATHER, category: 'weather', item: 'location', property: 'temperature' };
  const b = { plugin: WEATHER, category: 'weather', item: 'location', property: 'uv_index' };
  const c = { plugin: WEATHER, category: 'weather', item: 'other', property: 'temperature' };
  const g = P.groupSelections([a, null, b, c, a]);
  assert.strictEqual(g.length, 2);
  assert.deepStrictEqual(g[0].properties, ['temperature', 'uv_index']);
  assert.deepStrictEqual(g[0].slots, [0, 2, 4]);
  assert.deepStrictEqual(g[1].slots, [3]);
});

test('pickInstance: by id, by 1-based number, first when none chosen, null otherwise', () => {
  assert.strictEqual(P.pickInstance(env, 'bbb').instanceId, 'bbb');
  assert.strictEqual(P.pickInstance(env, undefined).instanceId, 'aaa');
  assert.strictEqual(P.pickInstance(env, '2').instanceId, 'bbb');
  assert.strictEqual(P.pickInstance(env, '3'), null);
  assert.strictEqual(P.pickInstance(env, 'zzz'), null);
  assert.strictEqual(P.pickInstance({ instances: [] }, undefined), null);
  assert.strictEqual(P.pickInstance(null, undefined), null);
});

test('formatNumber: short, no float noise, sign kept', () => {
  assert.strictEqual(P.formatNumber(18.2999999), '18.3');
  assert.strictEqual(P.formatNumber(12), '12');
  assert.strictEqual(P.formatNumber(-4), '-4');
  assert.strictEqual(P.formatNumber(-0.0001), '0');
  assert.strictEqual(P.formatNumber(0.1 + 0.2), '0.3');
  assert.strictEqual(P.formatNumber(2.5), '2.5');
  assert.strictEqual(P.formatNumber(1013.25), '1013');
  assert.strictEqual(P.formatNumber(0.06), '0.06');
  assert.strictEqual(P.formatNumber(123456789012), '--');
  assert.strictEqual(P.formatNumber(NaN), '--');
  assert.strictEqual(P.formatNumber('5'), '--');
});

test('truncateUtf8 never splits a character', () => {
  assert.strictEqual(P.truncateUtf8('°C', 11), '°C');
  assert.strictEqual(P.truncateUtf8('°°°', 5), '°°');
  assert.strictEqual(P.truncateUtf8('abcdef', 3), 'abc');
});

test('resultFor reads the chosen instance; missing data -> "--"', () => {
  const sel = { category: 'weather', item: 'location', property: 'temperature', instanceId: 'bbb' };
  assert.deepStrictEqual(P.resultFor(3, sel, env), { slot: 3, value: '-4', unit: '°C' });
  assert.deepStrictEqual(P.resultFor(0, { category: 'weather', item: 'location', property: 'temperature' }, env),
    { slot: 0, value: '18.3', unit: '°C' });
  assert.deepStrictEqual(P.resultFor(1, { category: 'weather', item: 'location', property: 'nope' }, env), P.noData(1));
  assert.deepStrictEqual(P.resultFor(1, { category: 'weather', item: 'location', property: 'temperature', instanceId: 'zzz' }, env), P.noData(1));
  assert.deepStrictEqual(P.resultFor(1, sel, { instances: [{ instanceId: 'bbb', properties: { temperature: { shortText: { text: '1' } } } }] }), P.noData(1));
});

test('packPluginData: 16 records, three US-separated fields, defaults to "--"', () => {
  const s = P.packPluginData([{ slot: 2, value: '12', unit: '°C' }]);
  const f = s.split('\x1f');
  assert.strictEqual(f.length, 48);
  assert.deepStrictEqual(f.slice(0, 3), ['0', '--', '']);
  assert.deepStrictEqual(f.slice(6, 9), ['2', '12', '°C']);
  assert.deepStrictEqual(f.slice(45, 48), ['15', '--', '']);
  assert.deepStrictEqual(Object.keys(P.buildPluginMessage([])), ['PluginData']);
});

test('anyPluginWidget gates on ids 144..159; the widget list accepts them', () => {
  assert.strictEqual(P.anyPluginWidget([7, 144]), true);
  assert.strictEqual(P.anyPluginWidget([159]), true);
  assert.strictEqual(P.anyPluginWidget([143, 160, 7]), false);
  assert.deepStrictEqual(widgetListToPayload([{ id: 144, ident: 'unit', font: 31 }, { id: 159, ident: 'none', font: 31 }, { id: 160 }]),
    [144, 31 | 0x40, 159, 31 | 0x20]);
});

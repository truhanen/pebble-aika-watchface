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

test('allNoData: true only when every result is "--" (failed fetch -> immediate retry)', () => {
  const m = require('../src/pkjs/plugins_parse');
  assert.strictEqual(m.allNoData([m.noData(0), m.noData(3)]), true);
  assert.strictEqual(m.allNoData([m.noData(0), { slot: 1, value: '18', unit: 'C' }]), false);
  assert.strictEqual(m.allNoData([]), true);
});

test('formatNumber: negative and long values stay within the 11-byte watch buffer', () => {
  const m = require('../src/pkjs/plugins_parse');
  assert.strictEqual(m.formatNumber(-12.54), '-12.5');
  assert.strictEqual(m.formatNumber(-1234567.89), '-1234568');
  assert.ok(m.formatNumber(1e21).length <= 11);
  assert.strictEqual(m.formatNumber(0.0004), '0');
});

// ---- icons (the plugin `icon` shape: 4bpp palette-indexed bitmap, base64 in the envelope)
const b64 = (bytes) => Buffer.from(bytes).toString('base64');
const px23 = Array.from({ length: 276 }, (_, i) => (i * 7) & 0xff);
const iconEnv = (pixels, palette, w, h) => ({ pluginUuid: 'u', instances: [{ instanceId: 'i1',
  properties: { condition: { icon: { pixels: b64(pixels), palette: b64(palette), width: w, height: h } } } }] });
const ICON_SEL = { category: 'weather', item: 'location', instanceId: 'i1', property: 'condition', shape: 'icon' };

test('base64ToBytes matches Buffer for every length, with or without padding', () => {
  for (let n = 0; n < 12; n++) {
    const bytes = Array.from({ length: n }, (_, i) => (i * 37 + 11) & 0xff);
    const enc = b64(bytes);
    assert.deepStrictEqual(P.base64ToBytes(enc), bytes);
    assert.deepStrictEqual(P.base64ToBytes(enc.replace(/=+$/, '')), bytes);
  }
  assert.deepStrictEqual(P.base64ToBytes('QUJD\nREVG'), [65, 66, 67, 68, 69, 70]);   // whitespace ignored
  assert.deepStrictEqual(P.base64ToBytes(b64(px23)), px23);
  assert.strictEqual(P.base64ToBytes('QU!D'), null);
  assert.strictEqual(P.base64ToBytes('Q'), null);
  assert.strictEqual(P.base64ToBytes(undefined), null);
});

test('decodeIcon: 23x23 icon decodes; bad size / palette / short pixels -> null', () => {
  const pal = [0xc0, 0xff, 0x00];
  const ok = P.decodeIcon({ pixels: b64(px23), palette: b64(pal), width: 23, height: 23 });
  assert.deepStrictEqual(ok, { width: 23, height: 23, palette: pal, pixels: px23 });
  assert.strictEqual(P.decodeIcon({ pixels: b64(px23.slice(1)), palette: b64(pal), width: 23, height: 23 }), null);
  assert.strictEqual(P.decodeIcon({ pixels: b64(px23), palette: b64([1, 2]), width: 23, height: 23 }), null);
  assert.strictEqual(P.decodeIcon({ pixels: b64(px23), palette: b64(pal), width: 24, height: 23 }), null);
  assert.strictEqual(P.decodeIcon({ pixels: b64(px23), palette: b64(pal), width: 0, height: 23 }), null);
  assert.strictEqual(P.decodeIcon(null), null);
  // extra trailing bytes are cut to h * ceil(w/2); a smaller square (non-square request) works
  const small = P.decodeIcon({ pixels: b64([1, 2, 3, 4, 5, 6, 7, 8, 9]), palette: b64(pal), width: 3, height: 4 });
  assert.deepStrictEqual(small.pixels, [1, 2, 3, 4, 5, 6, 7, 8]);
});

test('selections: shape icon survives parse/serialize; groups ask for icons only when needed', () => {
  const json = JSON.stringify([ICON_SEL, { category: 'weather', item: 'location', property: 'temperature', shape: 'bogus' }]);
  const sels = P.parseSelections(json);
  assert.strictEqual(sels[0].shape, 'icon');
  assert.strictEqual(sels[1].shape, undefined);
  assert.deepStrictEqual(P.parseSelections(P.serializeSelections(sels)), sels);
  assert.strictEqual(P.groupSelections(sels)[0].icons, true);
  assert.strictEqual(P.groupSelections([sels[1]])[0].icons, false);
});

test('resultFor an icon selection: ICON_MARK value + decoded bitmap; missing icon -> "--"', () => {
  const pal = [0xc0, 0xff, 0x00];
  const r = P.resultFor(3, ICON_SEL, iconEnv(px23, pal, 23, 23));
  assert.strictEqual(r.slot, 3);
  assert.strictEqual(r.value, P.ICON_MARK);
  assert.strictEqual(r.unit, '');
  assert.strictEqual(r.icon.width, 23);
  assert.deepStrictEqual(r.icon.pixels, px23);
  assert.deepStrictEqual(P.resultFor(3, ICON_SEL, { instances: [{ instanceId: 'i1', properties: {} }] }), P.noData(3));
  assert.deepStrictEqual(P.resultFor(3, ICON_SEL, iconEnv(px23.slice(5), pal, 23, 23)), P.noData(3));
  // an icon slot's PluginData record is just the marker
  const packed = P.packPluginData([r]).split('\x1f');
  assert.deepStrictEqual(packed.slice(9, 12), ['3', '@', '']);
});

test('icon messages: own message per slot, byte arrays; unchanged bitmaps are not resent unless forced', () => {
  const pal = [0xc0, 0xff, 0x00];
  const a = P.resultFor(0, ICON_SEL, iconEnv(px23, pal, 23, 23));
  const b = P.resultFor(5, ICON_SEL, iconEnv(px23.map((v) => v ^ 1), pal, 23, 23));
  const num = { slot: 1, value: '18', unit: 'C' };
  const first = P.iconMessages([a, b, num], {}, false);
  assert.deepStrictEqual(first.map((m) => m.slot), [0, 5]);
  const msg = first[0].msg;
  assert.deepStrictEqual(Object.keys(msg).sort(), ['PluginIconHeight', 'PluginIconPalette',
    'PluginIconPixels', 'PluginIconSlot', 'PluginIconWidth']);
  assert.strictEqual(msg.PluginIconSlot, 0);
  assert.deepStrictEqual(msg.PluginIconPalette, pal);
  assert.strictEqual(msg.PluginIconPixels.length, 276);
  // one dict stays far below the 1024-byte inbox (7 bytes per tuple header + payloads)
  assert.ok(276 + 3 + 3 * 4 + 5 * 7 < 400);
  const sent = {};
  first.forEach((m) => { sent[String(m.slot)] = m.key; });
  assert.deepStrictEqual(P.iconMessages([a, b, num], sent, false), []);             // deduped
  assert.strictEqual(P.iconMessages([a, b, num], sent, true).length, 2);            // cold request: resend all
  const b2 = P.resultFor(5, ICON_SEL, iconEnv(px23.map((v) => v ^ 2), pal, 23, 23));
  assert.deepStrictEqual(P.iconMessages([a, b2], sent, false).map((m) => m.slot), [5]);   // only the changed one
  // a slot that stopped being an icon forgets its key, so re-selecting resends
  assert.deepStrictEqual(P.pruneIconKeys(sent, [a, num]), { 0: sent['0'] });
});

test('docs/widget_contract.md documents the icon wire format as implemented', () => {
  const fs = require('node:fs');
  const path = require('node:path');
  const doc = fs.readFileSync(path.join(__dirname, '..', 'docs', 'widget_contract.md'), 'utf8');
  const keys = require('../package.json').pebble.messageKeys;
  ['PluginIconSlot', 'PluginIconWidth', 'PluginIconHeight', 'PluginIconPalette', 'PluginIconPixels'].forEach((k) => {
    assert.ok(keys.indexOf(k) !== -1, k + ' is a message key');
    assert.ok(doc.indexOf('`' + k + '`') !== -1, k + ' is documented');
  });
  // the same constants on both sides: marker, 23 px icon box, 276-byte bitmap
  const h = fs.readFileSync(path.join(__dirname, '..', 'src', 'c', 'plugin_slots.h'), 'utf8');
  assert.ok(h.indexOf('#define PLUGIN_ICON_MARK   "' + P.ICON_MARK + '"') !== -1);
  assert.ok(h.indexOf('#define PLUGIN_ICON_MAX_DIM    ' + P.ICON_PIXEL_SIZE) !== -1);
  const wl = fs.readFileSync(path.join(__dirname, '..', 'src', 'c', 'widget_layout_calc.h'), 'utf8');
  assert.ok(wl.indexOf('#define WL_ICON_H ' + P.ICON_PIXEL_SIZE) !== -1);
  assert.ok(doc.indexOf('`' + P.ICON_MARK + '`') !== -1);
  assert.ok(doc.indexOf('276 bytes at 23 x 23') !== -1);
});

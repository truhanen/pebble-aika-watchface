// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// Plugin widget names (plugin_names.ts) and the copy inlined in config_plugin_list.ts.

const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { foldAscii, shortPart, shortenName, uniqueNames } = require('../src/pkjs/plugin_names');

const P = (category, plugin, item, instance, property) =>
  ({ category, plugin, item, instance, property });

test('foldAscii strips diacritics and non-alphanumerics', () => {
  assert.strictEqual(foldAscii('Jyväskylä'), 'Jyvaskyla');
  assert.strictEqual(foldAscii('Tromsø'), 'Tromso');
  assert.strictEqual(foldAscii('Åbo-Turku!'), 'AboTurku');
  assert.strictEqual(foldAscii(undefined), '');
});

test('shortPart cuts to len and capitalises the first character only', () => {
  assert.strictEqual(shortPart('Jyväskylä', 3), 'Jyv');
  assert.strictEqual(shortPart('temperature', 3), 'Tem');
  assert.strictEqual(shortPart('FMI', 3), 'FMI');
  assert.strictEqual(shortPart('uv_index', 4), 'Uvin');
  assert.strictEqual(shortPart('', 3), '');
});

test('shortenName: the documented example and the numbered / absent instance', () => {
  assert.strictEqual(shortenName(P('weather', 'FMI', 'location', 'Jyväskylä', 'temperature')),
    'WEATHER FMI Loc Jyv Tem');
  assert.strictEqual(shortenName(P('weather', 'FMI', 'location', '1', 'temperature')),
    'WEATHER FMI Loc 1 Tem');
  assert.strictEqual(shortenName(P('weather', 'Weather', 'location', '', 'uv_index')),
    'WEATHER Wea Loc Uvi');
});

test('uniqueNames: distinct entries keep their 3-char names; empty slots stay empty', () => {
  const l = [P('weather', 'FMI', 'location', 'Jyväskylä', 'temperature'), null,
    P('weather', 'FMI', 'location', 'Helsinki', 'temperature')];
  assert.deepStrictEqual(uniqueNames(l), ['WEATHER FMI Loc Jyv Tem', '', 'WEATHER FMI Loc Hel Tem']);
});

test('uniqueNames: a collision extends the cut (4+ chars) until the names differ', () => {
  // 'Helsinki' / 'Helsingborg' only differ at the 7th character
  const l = [P('weather', 'FMI', 'location', 'Helsinki', 'temperature'),
    P('weather', 'FMI', 'location', 'Helsingborg', 'temperature')];
  assert.deepStrictEqual(uniqueNames(l),
    ['WEATHER FMI Locatio Helsink Tempera', 'WEATHER FMI Locatio Helsing Tempera']);
  // only the colliding pair grows; an entry that is already distinct keeps 3 chars
  const m = [P('weather', 'FMI', 'location', 'Jyväskylä', 'temperature'),
    P('weather', 'FMI', 'location', 'Helsinki', 'temperature'),
    P('weather', 'FMI', 'location', 'Hello', 'temperature')];
  assert.deepStrictEqual(uniqueNames(m).map((n) => n.split(' ')[3]), ['Jyv', 'Hels', 'Hell']);
});

test('uniqueNames: numbered instances stay distinct without growing', () => {
  const l = [P('weather', 'FMI', 'location', '1', 'temperature'),
    P('weather', 'FMI', 'location', '2', 'temperature')];
  assert.deepStrictEqual(uniqueNames(l), ['WEATHER FMI Loc 1 Tem', 'WEATHER FMI Loc 2 Tem']);
});

test('uniqueNames: identical selections get a numeric suffix as the last resort', () => {
  const same = P('weather', 'FMI', 'location', 'Helsinki', 'temperature');
  const out = uniqueNames([same, same, same]);
  assert.strictEqual(new Set(out).size, 3);
  assert.ok(out[1].endsWith(' 2') && out[2].endsWith(' 3'));
});

// The component inlines the algorithm between BEGIN/END naming markers (Clay toSource()
// cannot import). Evaluate the compiled segment and compare on a corpus.
test('config_plugin_list inline naming copy gives the same names as plugin_names.ts', () => {
  const src = fs.readFileSync(path.join(__dirname, '..', 'src/pkjs/config_plugin_list.js'), 'utf8');
  const a = src.indexOf('// BEGIN naming');
  const b = src.indexOf('// END naming');
  assert.ok(a !== -1 && b > a, 'naming markers found');
  const inline = new Function(src.slice(a, b) + '; return { uniqueNames: uniqueNames, shortenName: shortenName };')();
  const corpus = [
    P('weather', 'FMI', 'location', 'Jyväskylä', 'temperature'),
    P('weather', 'FMI', 'location', 'Helsinki', 'temperature'),
    P('weather', 'FMI', 'location', 'Helsingborg', 'temperature'),
    P('weather', 'FMI', 'location', '1', 'uv_index'),
    P('weather', 'Weather', 'location', '', 'uv_index'),
    P('health', 'Åland Øy', 'steps', '', 'count'),
    P('health', 'Åland Øy', 'steps', '', 'count'),
    null,
    P('misc', 'Ærø Straße', 'x', 'Łódź', 'y'),
  ];
  assert.deepStrictEqual(inline.uniqueNames(corpus), uniqueNames(corpus));
  corpus.filter(Boolean).forEach((p) => {
    for (let n = 1; n <= 6; n++) { assert.strictEqual(inline.shortenName(p, n), shortenName(p, n)); }
  });
});

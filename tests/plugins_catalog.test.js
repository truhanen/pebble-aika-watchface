// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const { buildCatalogue, gatherInstances, instancesFromEnvelope, multiInstanceItems } =
  require('../src/pkjs/plugins_catalog');
const { serializeSelections, parseSelections } = require('../src/pkjs/plugins_parse');

const WEATHER = '00000000-0000-0000-0001-000000000002';
const ENUM = [
  { uuid: WEATHER, name: 'Weather', actions: [], sources: [
    { category: 'weather', items: ['location'], supportsMultiple: true, properties: {
      temperature: ['shortText', 'longText', 'numericValue'],
      condition: ['shortText', 'icon'],
      forecast: ['series.numeric'],
      name: ['shortText'],
      pic: ['image', 'numericValue'],
    } },
  ] },
  { uuid: 'p2', name: 'Steps', sources: [
    { category: 'health', items: ['steps', 'sleep'], properties: { count: ['numericValue'] } },
    { category: 'empty', items: ['x'], properties: { t: ['shortText'] } },   // nothing supported
  ] },
  { bogus: true }, null,
];

test('buildCatalogue keeps supported shapes only, skips series.* and unsupported', () => {
  const cat = buildCatalogue(ENUM);
  assert.strictEqual(cat.v, 1);
  assert.deepStrictEqual(cat.c.map((c) => c.c), ['weather', 'health']);
  const loc = cat.c[0].p[0];
  assert.strictEqual(loc.u, WEATHER);
  assert.strictEqual(loc.n, 'Weather');
  assert.deepStrictEqual(loc.i[0], { i: 'location', m: 1,
    s: [['temperature', 'n'], ['condition', 'i'], ['pic', 'n']] });   // `image` is not drawable: 'pic' stays numeric only
  const steps = cat.c[1].p[0];
  assert.deepStrictEqual(steps.i.map((i) => i.i), ['steps', 'sleep']);
  assert.strictEqual(steps.i[0].m, undefined);
});

test('buildCatalogue: absent / garbage enumeration -> empty catalogue', () => {
  assert.deepStrictEqual(buildCatalogue(undefined), { v: 1, c: [] });
  assert.deepStrictEqual(buildCatalogue('x'), { v: 1, c: [] });
});

test('the catalogue stays small (built-in weather-sized plugin well under 1 KB)', () => {
  assert.ok(JSON.stringify(buildCatalogue(ENUM)).length < 700);
});

test('instancesFromEnvelope reads names from properties.name.shortText.text', () => {
  const env = { instances: [
    { instanceId: 'a-1', properties: { name: { shortText: { text: 'Jyväskylä' } } } },
    { instanceId: 'b-2', properties: {} },
    { instanceId: 7 },
    { properties: {} },
  ] };
  assert.deepStrictEqual(instancesFromEnvelope(env),
    [{ i: 'a-1', n: 'Jyväskylä' }, { i: 'b-2', n: '' }, { i: '7', n: '' }]);
  assert.deepStrictEqual(instancesFromEnvelope(null), []);
});

function fakeTimers() {
  const t = { fn: null, cleared: false };
  return { t, set: (fn) => { t.fn = fn; return 1; }, clear: () => { t.cleared = true; } };
}

test('gatherInstances fills the instances of multi-instance items and unsubscribes', () => {
  const cat = buildCatalogue(ENUM);
  const unsubs = [];
  const reqs = [];
  const subscribe = (req) => {
    reqs.push(req);
    req.onData({ instances: [{ instanceId: 'u1', properties: { name: { shortText: { text: 'Home' } } } },
      { instanceId: 'u2' }] });
    return { unsubscribe: () => unsubs.push(req.item) };
  };
  const ft = fakeTimers();
  let called = 0;
  gatherInstances(cat, subscribe, 2000, () => { called++; }, ft.set, ft.clear);
  assert.strictEqual(called, 1);
  assert.strictEqual(reqs.length, 1);   // only weather/location is multi-instance
  assert.deepStrictEqual(reqs[0].properties, ['name']);
  assert.deepStrictEqual(cat.c[0].p[0].i[0].n, [{ i: 'u1', n: 'Home' }, { i: 'u2', n: '' }]);
  assert.deepStrictEqual(unsubs, ['location']);
  assert.ok(ft.t.cleared);
});

test('gatherInstances is time-bounded: a source that never answers leaves no instances', () => {
  const cat = buildCatalogue(ENUM);
  let unsub = 0;
  const ft = fakeTimers();
  let called = 0;
  gatherInstances(cat, () => ({ unsubscribe: () => { unsub++; } }), 2000, () => { called++; }, ft.set, ft.clear);
  assert.strictEqual(called, 0);
  ft.t.fn();
  assert.strictEqual(called, 1);
  assert.strictEqual(unsub, 1);
  assert.strictEqual(cat.c[0].p[0].i[0].n, undefined);
  ft.t.fn();   // a second firing does nothing
  assert.strictEqual(called, 1);
});

test('gatherInstances: errors, throwing subscribe and no subscribe fn all complete', () => {
  const cat = buildCatalogue(ENUM);
  let called = 0;
  gatherInstances(cat, (req) => { req.onError({ code: 1 }); return { unsubscribe() {} }; }, 2000, () => { called++; }, fakeTimers().set, () => {});
  gatherInstances(cat, () => { throw new Error('x'); }, 2000, () => { called++; }, fakeTimers().set, () => {});
  gatherInstances(cat, undefined, 2000, () => { called++; });
  assert.strictEqual(called, 3);
  assert.strictEqual(multiInstanceItems(cat).length, 1);
});

test('serializeSelections: slot order kept, bad entries null, trailing nulls trimmed, capped at 16', () => {
  const a = { plugin: WEATHER, category: 'weather', item: 'location', instanceId: 'u1', property: 'temperature' };
  const b = { category: 'health', item: 'steps', property: 'count' };
  const json = serializeSelections([null, a, { category: 'x' }, b, null, null]);
  assert.deepStrictEqual(JSON.parse(json), [null, a, null, b]);
  assert.deepStrictEqual(parseSelections(json), [null, a, null, b]);   // round trip
  assert.strictEqual(serializeSelections([null]), '[]');
  assert.strictEqual(serializeSelections('garbage'), '[]');
  assert.deepStrictEqual(JSON.parse(serializeSelections(JSON.stringify([a]))), [a]);
  const many = []; for (let i = 0; i < 20; i++) { many.push(a); }
  assert.strictEqual(JSON.parse(serializeSelections(many)).length, 16);
});

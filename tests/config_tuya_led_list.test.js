// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// The tuyaLedList Clay component. Clay serializes components with toSource() and
// re-evals them in the config webview, so the component must be self-contained:
// the last test asserts the generated JS has no TS downlevel helper references.

const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { JSDOM } = require('jsdom');
const component = require('../src/pkjs/config_tuya_led_list');

const CATALOG = {
  v: 1,
  devices: [
    { id: 'dev1', name: 'Kitchen plug', codes: [
      { code: 'switch_1', type: 'Boolean', scale: 0, unit: '', sample: true },
      { code: 'va_temperature', type: 'Integer', scale: 1, unit: '℃', sample: 235 },
      { code: 'countdown_1', type: 'Integer', scale: 0, unit: 's', sample: 0 },
    ] },
    { id: 'dev2', name: 'Lamp', codes: [
      { code: 'switch_led', type: '', scale: 0, unit: '', sample: false },  // type missing, sample boolean
      { code: 'work_mode', type: 'Enum', scale: 0, unit: '', sample: 'white' },
    ] },
  ],
};

// Minimal Clay stand-in: builds the component's template into a JSDOM document and
// fires AFTER_BUILD the way clay-config.js does.
function mount(initialRows, catalog) {
  const dom = new JSDOM('<!doctype html><html><body><div id="root"></div></body></html>');
  global.window = dom.window;
  global.document = dom.window.document;
  const host = dom.window.document.getElementById('root');
  host.innerHTML = component.template;
  const handlers = {};
  const clayConfig = {
    EVENTS: { BEFORE_BUILD: 'b', AFTER_BUILD: 'a', BEFORE_DESTROY: 'c', AFTER_DESTROY: 'd' },
    on: (ev, fn) => { handlers[ev] = fn; },
    getItemByMessageKey: (key) => (key === 'TuyaCatalog'
      ? { get: () => JSON.stringify(catalog || CATALOG) } : null),
  };
  const self = { $element: [host.firstElementChild], trigger: () => {} };
  component.initialize.call(self, null, clayConfig);
  component.manipulator.set.call(self, initialRows);
  handlers.a();           // AFTER_BUILD -> read the catalog, re-render rows
  return { self, root: host.firstElementChild, dom };
}

test('only boolean datapoints are offered (spec type OR boolean sample)', () => {
  const { root } = mount([{ deviceId: 'dev1', code: 'switch_1' }]);
  const codeOpts = Array.from(root.querySelectorAll('.tll-code option')).map((o) => o.value);
  assert.deepStrictEqual(codeOpts, ['switch_1']);
  const devOpts = Array.from(root.querySelectorAll('.tll-device option')).map((o) => o.value);
  assert.deepStrictEqual(devOpts, ['dev1', 'dev2']);
});

test('rows round-trip through set() and get() in order', () => {
  const rows = [{ deviceId: 'dev1', code: 'switch_1' }, { deviceId: 'dev2', code: 'switch_led' }];
  const { self } = mount(rows);
  assert.deepStrictEqual(component.manipulator.get.call(self), rows);
});

test('the up button swaps a row with the one above it', () => {
  const { self, root } = mount([
    { deviceId: 'dev1', code: 'switch_1' },
    { deviceId: 'dev2', code: 'switch_led' },
  ]);
  root.querySelectorAll('.tll-row')[1].querySelector('.tll-up').click();
  assert.deepStrictEqual(component.manipulator.get.call(self), [
    { deviceId: 'dev2', code: 'switch_led' },
    { deviceId: 'dev1', code: 'switch_1' },
  ]);
});

test('the down button swaps a row with the one below it', () => {
  const { self, root } = mount([
    { deviceId: 'dev1', code: 'switch_1' },
    { deviceId: 'dev2', code: 'switch_led' },
  ]);
  root.querySelectorAll('.tll-row')[0].querySelector('.tll-down').click();
  assert.deepStrictEqual(component.manipulator.get.call(self), [
    { deviceId: 'dev2', code: 'switch_led' },
    { deviceId: 'dev1', code: 'switch_1' },
  ]);
});

test('up on the first row and down on the last row are no-ops', () => {
  const rows = [{ deviceId: 'dev1', code: 'switch_1' }, { deviceId: 'dev2', code: 'switch_led' }];
  const { self, root } = mount(rows);
  root.querySelectorAll('.tll-row')[0].querySelector('.tll-up').click();
  root.querySelectorAll('.tll-row')[1].querySelector('.tll-down').click();
  assert.deepStrictEqual(component.manipulator.get.call(self), rows);
});

test('the delete button removes that row', () => {
  const { self, root } = mount([
    { deviceId: 'dev1', code: 'switch_1' },
    { deviceId: 'dev2', code: 'switch_led' },
  ]);
  root.querySelectorAll('.tll-row')[0].querySelector('.tll-del').click();
  assert.deepStrictEqual(component.manipulator.get.call(self),
    [{ deviceId: 'dev2', code: 'switch_led' }]);
});

test('add appends a row and is hidden once six rows exist', () => {
  const rows = [];
  for (let i = 0; i < 5; i++) { rows.push({ deviceId: 'dev1', code: 'switch_1' }); }
  const { self, root } = mount(rows);
  const add = root.querySelector('.tll-add');
  assert.notStrictEqual(add.style.display, 'none');
  add.click();
  assert.strictEqual(component.manipulator.get.call(self).length, 6);
  assert.strictEqual(root.querySelector('.tll-add').style.display, 'none');
});

test('an empty catalog hides add and explains why', () => {
  const { root } = mount([], { v: 1, devices: [] });
  assert.strictEqual(root.querySelector('.tll-add').style.display, 'none');
  assert.match(root.querySelector('.tll-status').textContent, /credentials/);
});

test('a catalog with devices but no switch datapoint hides add and says so', () => {
  const { root } = mount([], { v: 1, devices: [
    { id: 'dev9', name: 'Soil sensor', codes: [
      { code: 'humidity1', type: 'Integer', scale: 0, unit: '%', sample: 96 }] },
  ] });
  assert.strictEqual(root.querySelector('.tll-add').style.display, 'none');
  assert.match(root.querySelector('.tll-status').textContent, /switch/i);
});

test('changing the device repopulates that row switch list', () => {
  const { self, root } = mount([{ deviceId: 'dev1', code: 'switch_1' }]);
  const devSel = root.querySelector('.tll-device');
  devSel.value = 'dev2';
  devSel.dispatchEvent(new global.window.Event('change', { bubbles: true }));
  const codeOpts = Array.from(root.querySelectorAll('.tll-code option')).map((o) => o.value);
  assert.deepStrictEqual(codeOpts, ['switch_led']);
  assert.deepStrictEqual(component.manipulator.get.call(self),
    [{ deviceId: 'dev2', code: 'switch_led' }]);
});

test('a configured device missing from the catalog is kept and marked unavailable', () => {
  const { self, root } = mount([{ deviceId: 'gone', code: 'switch_1' }]);
  const devOpts = Array.from(root.querySelectorAll('.tll-device option')).map((o) => o.textContent);
  assert.ok(devOpts.some((t) => /unavailable/.test(t)));
  assert.deepStrictEqual(component.manipulator.get.call(self),
    [{ deviceId: 'gone', code: 'switch_1' }]);
});

test('the generated component is toSource()-safe (no TS downlevel helpers)', () => {
  const js = fs.readFileSync(
    path.join(__dirname, '..', 'src', 'pkjs', 'config_tuya_led_list.js'), 'utf8');
  assert.ok(!/__spreadArray|__assign|__read|_this/.test(js),
    'component must not reference module-scope TS helpers');
});

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// The widgetList Clay component: the compact one-line row with the "Aa" font picker.

const test = require('node:test');
const assert = require('node:assert');
const { JSDOM } = require('jsdom');
const component = require('../src/pkjs/config_widget_list');
const { widgetListToPayload } = require('../src/pkjs/widget_list_payload');

function mount(value, config) {
  const dom = new JSDOM('<!doctype html><html><body><div id="root"></div></body></html>');
  global.window = dom.window;
  global.document = dom.window.document;
  const host = dom.window.document.getElementById('root');
  host.innerHTML = component.template;
  const self = { $element: [host.firstElementChild], trigger: () => {}, config: config || {} };
  component.initialize.call(self, null, { on: () => {}, EVENTS: {} });
  component.manipulator.set.call(self, value);
  return { self, root: host.firstElementChild };
}
const w = (id, font) => ({ id, hide: false, font: font === undefined ? 31 : font });
const defaultLabel = (el) => el.querySelector('.wl-fontbox option[value="31"]').textContent;

test('every font-capable row has one "Aa" control named Font, in the main line', () => {
  const { root } = mount([w(10), w(7)]);
  const rows = root.querySelectorAll('.wl-row');
  rows.forEach((r) => {
    const box = r.querySelector('.wl-main .wl-fontbox');
    assert.ok(box, 'font control inside .wl-main');
    assert.strictEqual(box.querySelector('.wl-aa').textContent, 'Aa');
    const sel = box.querySelector('select');
    assert.strictEqual(sel.getAttribute('aria-label'), 'Font');
    assert.strictEqual(sel.getAttribute('title'), 'Font');
    assert.strictEqual(r.querySelectorAll('.wl-fontrow').length, 0, 'no second line');
  });
});

test('Default option names the list default font (side 5, top/bottom 7)', () => {
  const side = mount([w(10)], { defaultFont: 5 });
  assert.strictEqual(defaultLabel(side.root), 'Default (Gothic medium)');
  const top = mount([w(30)], { defaultFont: 7, max: 1 });
  assert.strictEqual(defaultLabel(top.root), 'Default (Bitham large)');
  const none = mount([w(10)]);
  assert.strictEqual(defaultLabel(none.root), 'Default');
});

test('a non-default font is marked on the Aa control', () => {
  const { root } = mount([w(10), w(10, 3)]);
  const boxes = root.querySelectorAll('.wl-fontbox');
  assert.ok(!boxes[0].classList.contains('wl-custom'));
  assert.ok(boxes[1].classList.contains('wl-custom'));
});

test('Empty rows have a font control and its value is read and encoded', () => {
  const { self, root } = mount([w(0, 2)]);
  const sel = root.querySelector('.wl-row .wl-font');
  assert.ok(sel, 'Empty has a font select');
  assert.strictEqual(sel.value, '2');
  sel.value = '11';
  const v = component.manipulator.get.call(self);
  assert.deepStrictEqual(v, [{ id: 0, hide: false, font: 11 }]);
  assert.deepStrictEqual(widgetListToPayload(v), [0, 11]);
});

test('icon-only widgets have no font control; text widgets in groups do', () => {
  const { root } = mount([w(36), w(37), w(38), w(10),
    { rotating: true, interval: 3, members: [w(37), w(10), w(0 + 2)] }]);
  const rows = root.querySelectorAll('.wl-row');
  for (let i = 0; i < 3; i++) { assert.strictEqual(rows[i].querySelectorAll('.wl-fontbox').length, 0); }
  assert.strictEqual(rows[3].querySelectorAll('.wl-fontbox').length, 1);
  const mems = rows[4].querySelectorAll('.wl-mem');
  assert.strictEqual(mems[0].querySelectorAll('.wl-fontbox').length, 0);
  assert.strictEqual(mems[1].querySelectorAll('.wl-mmain .wl-fontbox').length, 1);
  assert.strictEqual(rows[4].querySelectorAll('.wl-fontrow').length, 0);
});

test('rotating members with Empty are still dropped by the encoder', () => {
  const g = { rotating: true, interval: 3, members: [w(0, 5), w(10), w(12)] };
  assert.deepStrictEqual(widgetListToPayload([g]), [255, 2, 3, 10, 31, 12, 31]);
});

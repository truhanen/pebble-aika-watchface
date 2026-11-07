// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// The pluginList Clay component (cascading selects, slot-stable removal, names), its
// live feed into the widget picker, and buildOptions' plugin options. Clay serialises
// components with toSource(), so the last test checks the generated JS is self-contained.

const test = require('node:test');
const assert = require('node:assert');
const { JSDOM } = require('jsdom');
const component = require('../src/pkjs/config_plugin_list');
const catalogComponent = require('../src/pkjs/config_plugin_catalog');
const widgetList = require('../src/pkjs/config_widget_list');
const { buildOptions, memberOptions, identModes, STATIC_WIDGETS } = require('../src/pkjs/widget_options');
const { buildCatalogue } = require('../src/pkjs/plugins_catalog');

const WEATHER = '00000000-0000-0000-0001-000000000002';
const CAT = buildCatalogue([
  { uuid: WEATHER, name: 'FMI', sources: [
    { category: 'weather', items: ['location'], supportsMultiple: true,
      properties: { temperature: ['numericValue'], uv_index: ['numericValue'], condition: ['icon'] } } ] },
  { uuid: 'p2', name: 'Steps', sources: [
    { category: 'health', items: ['steps'], properties: { count: ['numericValue'] } } ] },
  { uuid: 'p3', name: 'Icons', sources: [
    { category: 'iconsonly', items: ['x'], properties: { pic: ['icon'] } } ] },
]);
CAT.c[0].p[0].i[0].n = [{ i: 'u-jkl', n: 'Jyväskylä' }, { i: 'u-hki', n: '' }];

function setup(initial, catalogue, opts) {
  const dom = new JSDOM('<!doctype html><html><body><div id="wl"></div><div id="pl"></div></body></html>');
  global.window = dom.window;
  global.document = dom.window.document;
  const handlers = [];
  const cat = catalogue === undefined ? CAT : catalogue;
  const store = { get: () => JSON.stringify(cat) };
  const clayConfig = {
    EVENTS: { AFTER_BUILD: 'a' },
    on: (ev, fn) => { handlers.push(fn); },
    getItemByMessageKey: (k) => (k === 'PluginCatalog' ? store : null),
  };
  const out = {};
  // widget list first (as on the page), then the plugin list
  const wlHost = document.getElementById('wl');
  wlHost.innerHTML = widgetList.template;
  out.wself = { $element: [wlHost.firstElementChild], trigger() {}, config: {} };
  widgetList.initialize.call(out.wself, null, clayConfig);
  widgetList.manipulator.set.call(out.wself, (opts && opts.widgets) || []);
  const host = document.getElementById('pl');
  host.innerHTML = component.template;
  out.changes = 0;
  out.self = { $element: [host.firstElementChild], trigger: () => { out.changes++; } };
  component.initialize.call(out.self, null, clayConfig);
  component.manipulator.set.call(out.self, initial);
  handlers.forEach((h) => h());       // AFTER_BUILD: read catalogue, re-render
  out.root = host.firstElementChild;
  out.wroot = wlHost.firstElementChild;
  out.get = () => component.manipulator.get.call(out.self);
  out.rows = () => Array.from(out.root.querySelectorAll('.pl-row'));
  out.names = () => out.rows().map((r) => r.getAttribute('data-name'));
  out.pickerLabels = () => Array.from(out.wroot.querySelectorAll('.wl-sel option')).map((o) => o.textContent);
  out.choose = (row, cls, value) => {
    const sel = row.querySelector('.' + cls);
    sel.value = value;
    sel.dispatchEvent(new dom.window.Event('change', { bubbles: true }));
  };
  out.click = (cls, row) => {
    (row ? row.querySelector('.' + cls) : out.root.querySelector('.' + cls))
      .dispatchEvent(new dom.window.MouseEvent('click', { bubbles: true }));
  };
  return out;
}
const SEL1 = { plugin: WEATHER, category: 'weather', item: 'location', instanceId: 'u-jkl', property: 'temperature' };

test('stored selections round-trip and rows map to widget ids 144+n', () => {
  const t = setup([SEL1, null, { plugin: 'p2', category: 'health', item: 'steps', property: 'count' }]);
  assert.deepStrictEqual(t.get(), [SEL1, null, { plugin: 'p2', category: 'health', item: 'steps', property: 'count' }]);
  assert.deepStrictEqual(t.rows().map((r) => r.getAttribute('data-wid')), ['144', '145', '146']);
  assert.deepStrictEqual(t.names(), ['WEATHER FMI Loc Jyv Tem', '', 'HEALTH Ste Ste Cou']);
});

test('instance select shows names (else numbers); stores the stable instanceId', () => {
  const t = setup([SEL1]);
  const inst = t.rows()[0].querySelector('.pl-inst');
  assert.notStrictEqual(inst.style.display, 'none');
  assert.deepStrictEqual(Array.from(inst.options).map((o) => [o.value, o.textContent]),
    [['u-jkl', 'Jyväskylä'], ['u-hki', '2']]);
  t.choose(t.rows()[0], 'pl-inst', 'u-hki');
  assert.strictEqual(t.get()[0].instanceId, 'u-hki');
  assert.strictEqual(t.names()[0], 'WEATHER FMI Loc 2 Tem');
});

test('instance select is hidden for single-instance items and instanceId is omitted', () => {
  const t = setup([{ plugin: 'p2', category: 'health', item: 'steps', property: 'count' }]);
  assert.strictEqual(t.rows()[0].querySelector('.pl-inst').style.display, 'none');
  assert.strictEqual('instanceId' in t.get()[0], false);
});

test('only numeric properties and categories with one are offered', () => {
  const t = setup([SEL1]);
  const cats = Array.from(t.rows()[0].querySelectorAll('.pl-cat option')).map((o) => o.value);
  assert.deepStrictEqual(cats, ['', 'weather', 'health', 'iconsonly']);
  const props = Array.from(t.rows()[0].querySelectorAll('.pl-prop option')).map((o) => o.value);
  assert.deepStrictEqual(props, ['temperature', 'uv_index', 'condition|i']);
});

test('Add picks first choices, fills the first empty slot, caps at 16', () => {
  const t = setup([]);
  t.click('pl-add');
  assert.deepStrictEqual(t.get(), [{ plugin: WEATHER, category: 'weather', item: 'location',
    instanceId: 'u-jkl', property: 'temperature' }]);
  assert.ok(t.changes > 0);
  for (let i = 0; i < 20; i++) { t.click('pl-add'); }
  assert.strictEqual(t.rows().length, 16);
  assert.strictEqual(t.root.querySelector('.pl-add').style.display, 'none');
  // identical selections still get unique names
  assert.strictEqual(new Set(t.names()).size, 16);
});

test('removing a middle row empties its slot; later slots keep their ids; trailing empties drop', () => {
  const a = SEL1;
  const b = { plugin: 'p2', category: 'health', item: 'steps', property: 'count' };
  const t = setup([a, b, a]);
  t.click('pl-del', t.rows()[1]);
  assert.deepStrictEqual(t.get(), [a, null, a]);
  assert.deepStrictEqual(t.rows().map((r) => r.getAttribute('data-wid')), ['144', '145', '146']);
  assert.strictEqual(t.names()[1], '');
  assert.ok(t.rows()[1].classList.contains('pl-empty'));
  // the freed slot is reused by the next Add
  t.click('pl-add');
  assert.strictEqual(t.get().length, 3);
  assert.ok(t.get()[1]);
  // removing the last row shrinks the list (and any empties before it)
  t.click('pl-del', t.rows()[2]);
  t.click('pl-del', t.rows()[1]);
  assert.deepStrictEqual(t.get(), [a]);
  assert.strictEqual(t.rows().length, 1);
});

test('cascade: changing category/plugin/item resets everything below to the first choices', () => {
  const t = setup([SEL1]);
  t.choose(t.rows()[0], 'pl-cat', 'health');
  assert.deepStrictEqual(t.get(), [{ plugin: 'p2', category: 'health', item: 'steps', property: 'count' }]);
  t.choose(t.rows()[0], 'pl-prop', 'count');
  t.choose(t.rows()[0], 'pl-cat', '');   // blank category empties the slot
  assert.deepStrictEqual(t.get(), []);
});

test('unavailable stored selections survive a load (not silently dropped)', () => {
  const gone = { plugin: 'old-uuid', category: 'gone', item: 'thing', property: 'val' };
  const t = setup([gone]);
  assert.deepStrictEqual(t.get(), [gone]);
  assert.match(t.rows()[0].querySelector('.pl-cat').textContent, /GONE \(unavailable\)/);
});

test('no plugins on the phone: hint shown, Add hidden, stored selections kept', () => {
  const t = setup([SEL1], { v: 1, c: [] });
  assert.match(t.root.querySelector('.pl-status').textContent, /No plugins available/);
  assert.strictEqual(t.root.querySelector('.pl-add').style.display, 'none');
  assert.deepStrictEqual(t.get(), [SEL1]);
  const t2 = setup([], '');   // catalogue store empty
  assert.match(t2.root.querySelector('.pl-status').textContent, /No plugins available/);
});

test('widget picker lists one option per plugin row, grouped by category, live', () => {
  const t = setup([SEL1], undefined, { widgets: [{ id: 144, ident: 'unit', font: 31 }] });
  // the id placed in the widget list resolves to the plugin name after the page built
  const sel = t.wroot.querySelector('.wl-row .wl-sel');
  assert.strictEqual(sel.value, '144');
  assert.strictEqual(sel.options[sel.selectedIndex].textContent, 'WEATHER FMI Loc Jyv Tem');
  // grouped right after the static WEATHER options
  const labels = t.pickerLabels();
  const idx = labels.indexOf('WEATHER FMI Loc Jyv Tem');
  assert.strictEqual(labels[idx - 1], 'WEATHER max rain 12 h');
  // adding a row updates the picker without reopening it; a new category is appended
  t.click('pl-add');
  t.choose(t.rows()[1], 'pl-cat', 'health');
  const after = t.pickerLabels();
  assert.ok(after.indexOf('HEALTH Ste Ste Cou') > after.indexOf('HEALTH heart rate'));
  // emptied slot: the placed widget degrades to a placeholder label, no crash
  t.click('pl-del', t.rows()[0]);
  const sel2 = t.wroot.querySelector('.wl-row .wl-sel');
  assert.strictEqual(sel2.value, '144');
  assert.strictEqual(sel2.options[sel2.selectedIndex].textContent, 'PLUGIN #1');
});

test('buildOptions appends plugin options; static list unchanged without them', () => {
  assert.deepStrictEqual(buildOptions([]), STATIC_WIDGETS);
  const opts = buildOptions([], [{ wid: 145, name: 'WEATHER FMI Loc 2 Tem' }, { wid: 144, name: '' },
    { wid: 100, name: 'BAD' }, { wid: 146, name: 'TUYA X Y' }]);
  assert.strictEqual(opts.length, STATIC_WIDGETS.length + 2);
  const i = opts.findIndex((o) => o.id === 145);
  assert.strictEqual(opts[i - 1].label, 'WEATHER max rain 12 h');
  assert.deepStrictEqual(opts[opts.length - 1], { id: 146, label: 'TUYA X Y' });
  assert.ok(memberOptions([], [{ wid: 145, name: 'A B' }]).some((o) => o.id === 145));
});

test('plugin widgets offer the unit / none identifier modes like other text slots', () => {
  for (let id = 144; id < 160; id++) { assert.deepStrictEqual(identModes(id), ['unit', 'none']); }
  assert.deepStrictEqual(identModes(160), []);
});

test('components are self-contained (toSource-safe)', () => {
  [component.initialize, component.manipulator.get, component.manipulator.set,
    catalogComponent.manipulator.get, catalogComponent.manipulator.set].forEach((fn) => {
    assert.ok(!/__spread|__read|__assign|_this|require\(/.test(fn.toString()), 'no helper/module references');
  });
});

test('icon properties: selectable, stored with shape icon, row marked, picker offers no identifier switch', () => {
  const ICON = { plugin: WEATHER, category: 'weather', item: 'location', instanceId: 'u-jkl',
    property: 'condition', shape: 'icon' };
  const t = setup([SEL1, ICON], undefined, { widgets: [{ id: 144, ident: 'unit', font: 31 }, { id: 145, ident: 'icon', font: 31 }] });
  assert.deepStrictEqual(t.get(), [SEL1, ICON]);
  assert.strictEqual(t.rows()[0].getAttribute('data-shape'), null);
  assert.strictEqual(t.rows()[1].getAttribute('data-shape'), 'i');
  const opt = t.rows()[1].querySelector('.pl-prop option:checked');
  assert.strictEqual(opt.value, 'condition|i');
  assert.strictEqual(opt.textContent, 'Condition (icon)');
  // numeric widget keeps its unit / none button, the icon one has just a placeholder
  const wrows = Array.from(t.wroot.querySelectorAll('.wl-row'));
  assert.strictEqual(wrows[0].querySelector('.wl-main .wl-hide').tagName, 'BUTTON');
  // the icon property shows the inactive icon-only label
  const fixed = wrows[1].querySelector('.wl-main .wl-hide');
  assert.strictEqual(fixed.textContent, '\u25c9');
  assert.strictEqual(fixed.disabled, true);
  assert.strictEqual(wrows[1].querySelector('.wl-main .wl-hide-ph'), null);
  // switching the property back to a numeric one drops the shape
  t.choose(t.rows()[1], 'pl-prop', 'temperature');
  assert.deepStrictEqual(t.get()[1], { plugin: WEATHER, category: 'weather', item: 'location',
    instanceId: 'u-jkl', property: 'temperature' });
  // ... and an icon choice sets it
  t.choose(t.rows()[0], 'pl-prop', 'condition|i');
  assert.strictEqual(t.get()[0].shape, 'icon');
  assert.strictEqual(t.get()[0].property, 'condition');
});

test('identModes: plugin icon widgets have no identifier switch', () => {
  assert.deepStrictEqual(identModes(150, true), []);
  assert.deepStrictEqual(identModes(150, false), ['unit', 'none']);
  assert.deepStrictEqual(identModes(7, true), ['icon', 'unit', 'none', 'iconly']);   // only plugin ids are affected
});

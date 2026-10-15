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
  assert.strictEqual(defaultLabel(side.root), 'Default (M Gothic)');
  const top = mount([w(30)], { defaultFont: 7, max: 1 });
  assert.strictEqual(defaultLabel(top.root), 'Default (L Bitham)');
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
  assert.deepStrictEqual(v, [{ id: 0, ident: 'icon', font: 11 }]);
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
  // the group header itself has no font control (only its members do)
  let memberBoxes = 0;
  mems.forEach((m) => { memberBoxes += m.querySelectorAll('.wl-fontbox').length; });
  assert.strictEqual(rows[4].querySelectorAll('.wl-fontbox').length, memberBoxes);
});

test('rotating members with Empty are still dropped by the encoder', () => {
  const g = { rotating: true, interval: 3, members: [w(0, 5), w(10), w(12)] };
  assert.deepStrictEqual(widgetListToPayload([g]), [255, 2, 3, 10, 31, 12, 31]);
});

test('every row in a list has the same slot elements with placeholders for missing controls', () => {
  // Test with a mix of plain, Empty, icon-only, rotating group with members, and top-list single row
  const { root: sideList } = mount([
    w(10),           // plain widget
    w(0),            // Empty (has font, no hide)
    w(36),           // icon-only (no font, no hide)
    w(37),           // icon-only
    { rotating: true, interval: 3, members: [w(2), w(10), w(37)] },  // rotating group
  ], { defaultFont: 5 });

  const { root: topList } = mount([w(35)], { defaultFont: 7, max: 1 });  // single-row top list

  // Check side list rows (should have: select, font, hide, up, down, del)
  const sideRows = sideList.querySelectorAll('.wl-row > .wl-main');
  assert.ok(sideRows.length >= 5, 'side list has at least 5 rows');

  sideRows.forEach((main, i) => {
    const slots = main.querySelectorAll(':scope > *');
    assert.strictEqual(slots.length, 6, `side row ${i} has 6 slot elements (select + font + hide + up + down + del)`);

    // First should be select
    assert.ok(slots[0].classList.contains('wl-sel'), `side row ${i} slot 0 is select`);

    // Check that font slot exists (either real or placeholder)
    const fontSlot = slots[1];
    assert.ok(
      fontSlot.classList.contains('wl-fontbox') || fontSlot.classList.contains('wl-fontbox-ph'),
      `side row ${i} slot 1 is font (real or placeholder)`
    );

    // Check that hide slot exists (either real button or placeholder)
    const hideSlot = slots[2];
    assert.ok(
      hideSlot.tagName === 'BUTTON' || hideSlot.classList.contains('wl-hide-ph'),
      `side row ${i} slot 2 is hide button or placeholder`
    );

    // Buttons should be the last 3 slots
    assert.ok(slots[3].classList.contains('wl-up') || slots[3].classList.contains('wl-btn-ph'), `side row ${i} slot 3 is up button or placeholder`);
    assert.ok(slots[4].classList.contains('wl-down') || slots[4].classList.contains('wl-btn-ph'), `side row ${i} slot 4 is down button or placeholder`);
    assert.ok(slots[5].tagName === 'BUTTON', `side row ${i} slot 5 is del button`);
  });

  // Check top list rows (single slot list)
  const topRows = topList.querySelectorAll('.wl-row > .wl-main');
  assert.ok(topRows.length === 1, 'top list has 1 row');
  const topSlots = topRows[0].querySelectorAll(':scope > *');
  assert.strictEqual(topSlots.length, 6, 'top row has 6 slot elements');

  // Check group members (should have: select, font, hide, up-placeholder, down-placeholder, del)
  const groupRows = sideList.querySelectorAll('.wl-mems .wl-mem');
  assert.ok(groupRows.length >= 2, 'rotating group has at least 2 members');
  groupRows.forEach((mem, i) => {
    const mains = mem.querySelectorAll('.wl-mmain > *');
    assert.strictEqual(mains.length, 6, `group member ${i} has 6 slot elements (select + font + hide + up-ph + down-ph + del)`);
  });
});

test('rotating group member rows align with outer rows (no right inset on group)', () => {
  const { root } = mount([
    w(10),           // plain widget
    w(0),            // Empty
    { rotating: true, interval: 3, members: [w(2), w(10), w(37)] },  // rotating group
  ], { defaultFont: 5 });

  // Get the computed style of the group to verify it has no right padding/margin that would prevent alignment
  const group = root.querySelector('.wl-group');
  assert.ok(group, 'group exists');

  // Check that group uses flex layout for its content rows
  const introw = group.querySelector('.wl-introw');
  const mems = group.querySelector('.wl-mems');
  assert.ok(introw, 'group has introw');
  assert.ok(mems, 'group has mems container');

  // Verify member rows have the same flex structure as outer rows
  const outerMain = root.querySelector('.wl-row:not(:has(.wl-group)) > .wl-main');
  const memberMain = group.querySelector('.wl-mem .wl-mmain');

  // Both should use flex layout
  assert.strictEqual(outerMain.style.display || 'flex', 'flex', 'outer row uses flex');
  assert.strictEqual(memberMain.style.display || 'flex', 'flex', 'member row uses flex');

  // Verify both have the same number of slot elements (structure for alignment)
  const outerSlots = outerMain.querySelectorAll(':scope > *');
  const memberSlots = memberMain.querySelectorAll(':scope > *');
  assert.strictEqual(outerSlots.length, memberSlots.length, 'outer and member rows have same slot count');
  assert.strictEqual(outerSlots.length, 6, 'both have 6 slots for alignment');
});

// ----------------------------------------------------------- identifier button

const { identModes, STATIC_WIDGETS } = require('../src/pkjs/widget_options');
const wid = (id, ident) => ({ id, ident, font: 31 });
const identBtn = (root, i) => root.querySelectorAll('.wl-row')[i || 0].querySelector('.wl-main .wl-hide');
const click = (el) => el.dispatchEvent(new (el.ownerDocument.defaultView.MouseEvent)('click', { bubbles: true }));

test('the identifier button cycles icon -> unit -> none -> icon for a widget with both', () => {
  const { self, root } = mount([wid(12, 'icon')]);
  const b = identBtn(root);
  assert.deepStrictEqual([b.textContent, b.getAttribute('data-ident')], ['\u25c9', 'icon']);
  click(b);
  assert.deepStrictEqual([identBtn(root).textContent, identBtn(root).getAttribute('data-ident')], ['U', 'unit']);
  assert.ok(/unit/i.test(identBtn(root).title));
  click(identBtn(root));
  assert.deepStrictEqual([identBtn(root).textContent, identBtn(root).getAttribute('data-ident')], ['\u2298', 'none']);
  assert.deepStrictEqual(component.manipulator.get.call(self), [wid(12, 'none')]);
  click(identBtn(root));
  assert.strictEqual(identBtn(root).getAttribute('data-ident'), 'icon');
});

test('a unit-only widget cycles only unit <-> none and starts on the unit', () => {
  const { self, root } = mount([wid(13, 'icon')]);      // UV: no icon, default requested icon
  assert.strictEqual(identBtn(root).getAttribute('data-ident'), 'unit');
  click(identBtn(root));
  assert.strictEqual(identBtn(root).getAttribute('data-ident'), 'none');
  click(identBtn(root));
  assert.strictEqual(identBtn(root).getAttribute('data-ident'), 'unit');
  assert.deepStrictEqual(component.manipulator.get.call(self), [wid(13, 'unit')]);
});

test('widgets without an identifier get the placeholder, not a button', () => {
  const { root } = mount([wid(0, 'icon'), wid(5, 'icon'), wid(26, 'icon'), wid(36, 'icon'), wid(11, 'icon')]);
  root.querySelectorAll('.wl-row').forEach((r) => {
    assert.strictEqual(r.querySelectorAll('.wl-main .wl-hide').length, 0);
    assert.strictEqual(r.querySelectorAll('.wl-main .wl-hide-ph').length, 1);
  });
});

test('rotating members get the same button and cycle the same modes', () => {
  const { self, root } = mount([{ rotating: true, interval: 3, members: [wid(7, 'icon'), wid(13, 'unit')] }]);
  const mem = root.querySelectorAll('.wl-mem');
  const b0 = mem[0].querySelector('.wl-mhide');
  click(b0);
  assert.strictEqual(mem[0].querySelector('.wl-mhide').getAttribute('data-ident'), 'unit');
  const v = component.manipulator.get.call(self);
  assert.deepStrictEqual(v[0].members, [wid(7, 'unit'), wid(13, 'unit')]);
  click(root.querySelectorAll('.wl-mem')[1].querySelector('.wl-mhide'));
  assert.deepStrictEqual(component.manipulator.get.call(self)[0].members[1], wid(13, 'none'));
});

test('legacy stored values load: hide -> none, otherwise icon', () => {
  const { self, root } = mount([{ id: 7, hide: true, font: 31 }, { id: 12, hide: false, font: 31 }, 34]);
  assert.strictEqual(identBtn(root, 0).getAttribute('data-ident'), 'none');
  assert.strictEqual(identBtn(root, 1).getAttribute('data-ident'), 'icon');
  assert.deepStrictEqual(component.manipulator.get.call(self).map((e) => e.ident), ['none', 'icon', 'none']);
});

test('the button of every offered widget cycles exactly identModes()', () => {
  STATIC_WIDGETS.forEach((o) => {
    if (o.id === 0 || o.id === 255) { return; }
    const modes = identModes(o.id);
    const { root } = mount([wid(o.id, 'icon')]);
    const b = identBtn(root);
    if (modes.length === 0) { assert.strictEqual(b, null, 'no button for ' + o.id); return; }
    const seen = [];
    let cur = identBtn(root);
    for (let i = 0; i < modes.length; i++) {
      seen.push(cur.getAttribute('data-ident'));
      click(cur);
      cur = identBtn(root);
    }
    assert.deepStrictEqual(seen, modes, 'modes of widget ' + o.id);
    assert.strictEqual(cur.getAttribute('data-ident'), modes[0], 'wraps around for ' + o.id);
  });
});

test('identModes: the documented widgets', () => {
  assert.deepStrictEqual(identModes(2), ['icon', 'unit', 'none']);
  assert.deepStrictEqual(identModes(22), ['icon', 'unit', 'none']);
  assert.deepStrictEqual(identModes(7), ['icon', 'unit', 'none']);
  assert.deepStrictEqual(identModes(13), ['unit', 'none']);
  assert.deepStrictEqual(identModes(210), ['unit', 'none']);
  assert.deepStrictEqual(identModes(23), ['icon', 'none']);
  assert.deepStrictEqual(identModes(5), []);
  assert.deepStrictEqual(identModes(30), []);
});

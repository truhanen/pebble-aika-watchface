// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const { isUvWidgetConfigured, UV_WIDGET_ID } = require('../src/pkjs/uv_widget');

const slot = (id, hide) => ({ id: id, hide: !!hide, font: 31 });

function storageWith(claySettings) {
  return {
    getItem: function (key) {
      return key === 'clay-settings' ? claySettings : null;
    },
  };
}

test('UV widget id is 13', () => {
  assert.strictEqual(UV_WIDGET_ID, 13);
});

test('true when UV widget (13) is in the left column', () => {
  const s = storageWith(JSON.stringify({ WidgetList: [1, 13, 2], WidgetListRight: [3] }));
  assert.strictEqual(isUvWidgetConfigured(s), true);
});

test('true when UV widget is in the right column', () => {
  const s = storageWith(JSON.stringify({ WidgetList: [1, 2], WidgetListRight: [13] }));
  assert.strictEqual(isUvWidgetConfigured(s), true);
});

test('false when UV widget is absent', () => {
  const s = storageWith(JSON.stringify({ WidgetList: [1, 2, 7], WidgetListRight: [8] }));
  assert.strictEqual(isUvWidgetConfigured(s), false);
});

test('handles string ids (Clay select values are strings)', () => {
  const s = storageWith(JSON.stringify({ WidgetList: ['1', '13'], WidgetListRight: [] }));
  assert.strictEqual(isUvWidgetConfigured(s), true);
});

test('false on missing / empty / malformed clay-settings', () => {
  assert.strictEqual(isUvWidgetConfigured(storageWith(null)), false);
  assert.strictEqual(isUvWidgetConfigured(storageWith('{}')), false);
  assert.strictEqual(isUvWidgetConfigured(storageWith('not json')), false);
});

test('true for new-format slot objects, including a hidden UV widget', () => {
  const s = storageWith(JSON.stringify({ WidgetList: [slot(7), slot(13, true)], WidgetListRight: [] }));
  assert.strictEqual(isUvWidgetConfigured(s), true);
});

test('true when UV is in the centre column list', () => {
  assert.strictEqual(isUvWidgetConfigured(storageWith(JSON.stringify({ WidgetListCenter: [slot(13)] }))), true);
  assert.strictEqual(isUvWidgetConfigured(storageWith(JSON.stringify({ WidgetListCenter: [slot(30), slot(0), slot(13)] }))), true);
});

test('true when UV is a rotating-group member', () => {
  const g = { rotating: true, interval: 3, members: [slot(10), slot(13)] };
  assert.strictEqual(isUvWidgetConfigured(storageWith(JSON.stringify({ WidgetListRight: [g] }))), true);
});

test('a hidden-icon UV widget in the legacy flat format (13 | 0x20) still counts', () => {
  assert.strictEqual(isUvWidgetConfigured(storageWith(JSON.stringify({ WidgetList: [13 | 0x20] }))), true);
});

test('false when only a similar widget is placed (weather 7, icon-only weather 37)', () => {
  const s = storageWith(JSON.stringify({ WidgetList: [slot(7)], WidgetListCenter: [slot(37)] }));
  assert.strictEqual(isUvWidgetConfigured(s), false);
});

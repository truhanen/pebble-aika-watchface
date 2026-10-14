// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const { isUvWidgetConfigured, UV_WIDGET_ID } = require('../src/pkjs/uv_widget');

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

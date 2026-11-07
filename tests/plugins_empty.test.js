// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// With plugins enabled but no selections, updatePlugins still sends an all '--'
// PluginData (once, deduped) so the watch's cold-poll loop ends and stale values clear.
const test = require('node:test');
const assert = require('node:assert');

function setup(sel) {
  const store = { disable_plugins: 'no' };
  if (sel !== undefined) { store.plugin_selections = sel; }
  const sent = [];
  global.window = { localStorage: {
    getItem: (k) => (k in store ? store[k] : null),
    setItem: (k, v) => { store[k] = String(v); },
    removeItem: (k) => { delete store[k]; },
  } };
  global.Pebble = {
    subscribeToSource: () => { throw new Error('no subscription expected'); },
    sendAppMessage: (m, ok) => { sent.push(m); ok(); },
  };
  delete require.cache[require.resolve('../src/pkjs/plugins')];
  return { sent, update: require('../src/pkjs/plugins').updatePlugins };
}

test('no selections: sends empty PluginData once, again on force', () => {
  const t = setup(JSON.stringify([]));
  t.update(false);
  assert.strictEqual(t.sent.length, 1);
  assert.ok(String(Object.values(t.sent[0])[0]).indexOf('--') !== -1);
  t.update(false);
  assert.strictEqual(t.sent.length, 1);   // deduped
  t.update(true);
  assert.strictEqual(t.sent.length, 2);   // forced / cold
});

test('selections never set: still sends', () => {
  const t = setup();
  t.update(false);
  assert.strictEqual(t.sent.length, 1);
});

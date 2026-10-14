// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// Sync guard for the duplicated widget option list. Two physical copies exist:
//   - src/ts/widget_options.ts STATIC_WIDGETS   (exported, used by tests + the
//     PKJS value mapping)
//   - src/ts/config_widget_list.ts STATIC_OPTIONS (inlined inside the Clay custom
//     component, which Clay serializes via toSource() and re-evals in the config
//     webview, so it cannot import a module-scope list — hence the duplicate)
// They MUST list the same {id,label} set. A drift here is exactly what dropped the
// Deep Sleep widget (id 20) from the config dropdown until it was re-synced. This
// guard fails on any divergence so the next addition can't silently desync them.

const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { STATIC_WIDGETS } = require('../src/pkjs/widget_options');

// Extract a `[{ id: N, label: '...' }, ...]` literal block by const name from a TS
// source file. Handles single- and double-quoted labels (e.g. "Today's Date").
function parseOptions(relFile, constName) {
  const src = fs.readFileSync(path.join(__dirname, '..', relFile), 'utf8');
  const at = src.indexOf(constName);
  assert.ok(at !== -1, constName + ' not found in ' + relFile);
  const open = src.indexOf('[', at);
  const close = src.indexOf('];', open);
  assert.ok(open !== -1 && close !== -1, 'array literal for ' + constName + ' not found');
  const block = src.slice(open, close);
  const re = /\{\s*id:\s*(\d+),\s*label:\s*(['"])((?:\\.|(?!\2).)*)\2\s*\}/g;
  const out = [];
  let m;
  while ((m = re.exec(block)) !== null) {
    out.push({ id: parseInt(m[1], 10), label: m[3] });
  }
  return out;
}

function sortById(arr) {
  return arr
    .map((o) => ({ id: o.id, label: o.label }))
    .sort((a, b) => a.id - b.id);
}

test('config_widget_list STATIC_OPTIONS stays in sync with widget_options STATIC_WIDGETS', () => {
  const fromOptions = sortById(STATIC_WIDGETS);
  const fromConfig = sortById(parseOptions('src/ts/config_widget_list.ts', 'STATIC_OPTIONS'));
  assert.ok(fromConfig.length > 0, 'parsed STATIC_OPTIONS is non-empty (parser still works)');
  assert.deepStrictEqual(fromConfig, fromOptions);
});

// The font choice list is duplicated the same way (widget_options.ts FONT_OPTIONS vs
// the array inlined in the component) and must match, in order: the array index IS
// the font index sent to the watch.
function parseStrings(relFile, constName) {
  const src = fs.readFileSync(path.join(__dirname, '..', relFile), 'utf8');
  const at = src.indexOf('const ' + constName);
  assert.ok(at !== -1, constName + ' not found in ' + relFile);
  const open = src.indexOf('= [', at) + 2;
  const close = src.indexOf('];', open);
  return src.slice(open, close).split('\n')
    .map((l) => l.trim())
    .filter((l) => /^'.*',?$/.test(l))
    .map((l) => l.replace(/^'|',?$/g, ''));
}

test('config_widget_list FONT_OPTIONS stays in sync with widget_options FONT_OPTIONS', () => {
  const { FONT_OPTIONS } = require('../src/pkjs/widget_options');
  const inlined = parseStrings('src/ts/config_widget_list.ts', 'FONT_OPTIONS');
  assert.strictEqual(inlined.length, 17, 'parsed 17 inlined font labels (parser still works)');
  assert.deepStrictEqual(inlined, FONT_OPTIONS);
});

// Clay serializes the component via toSource() and re-evals it in the webview, so the
// compiled initialize/manipulator must not reference module-scope helpers or tsc
// downlevel helpers.
test('widgetList component functions are self-contained (toSource-safe)', () => {
  const comp = require('../src/pkjs/config_widget_list');
  [comp.initialize, comp.manipulator.get, comp.manipulator.set].forEach((fn) => {
    const src = fn.toString();
    assert.ok(!/__spread|__read|__assign|_this|require\(/.test(src), 'no helper/module references');
  });
  assert.ok(/FONT_OPTIONS/.test(comp.initialize.toString()), 'font list is inlined');
});

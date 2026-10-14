// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// Guard for the relaunch power fix: the PKJS 'ready' handler must NOT fetch. A
// watchface is killed and restarted whenever any watchapp runs, so a fetch here ran
// a full network round on every return to the watchface. The watch asks for a
// forced refresh instead (cold poll request) when its persisted data is missing.

const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');

function indexSource() {
  return fs.readFileSync(path.join(__dirname, '..', 'src', 'pkjs', 'index.js'), 'utf8');
}

function readySource() {
  const js = indexSource();
  const start = js.indexOf("addEventListener('ready'");
  assert.ok(start !== -1, "the 'ready' listener registration must exist");
  const next = js.indexOf('addEventListener(', start + 10);
  return js.slice(start, next === -1 ? js.length : next);
}

test("the 'ready' handler performs no data fetches", () => {
  const src = readySource();
  const calls = ['updateWeather', 'updateElectricity', 'updateCrypto', 'updateCurrency', 'updateTuya'];
  const found = calls.filter((c) => src.indexOf(c) !== -1);
  assert.deepStrictEqual(found, [],
    "'ready' must not call any updateX (it runs on every watchface relaunch): " + found.join(', '));
});

test("the 'ready' handler still seeds the disable_* defaults", () => {
  const src = readySource();
  ['disable_weather', 'disable_electricity', 'disable_crypto', 'disable_currency', 'disable_tuya']
    .forEach((k) => assert.ok(src.indexOf(k) !== -1, k + ' default must still be seeded on ready'));
});

test('the appmessage poll handler routes the cold flag into every source', () => {
  const js = indexSource();
  const start = js.indexOf("addEventListener('appmessage'");
  assert.ok(start !== -1);
  const src = js.slice(start, start + 1500);
  assert.ok(src.indexOf('isColdPollRequest') !== -1, 'the poll handler must detect a cold request');
  ['updateWeather(', 'updateElectricity(', 'updateCrypto(', 'updateCurrency(', 'updateTuya(']
    .forEach((c) => {
      const at = src.indexOf(c);
      assert.ok(at !== -1, c + ' must be called from the poll handler');
      assert.ok(/^\s*cold\s*\)/.test(src.slice(at + c.length)),
        c + ' must be passed the cold flag');
    });
});

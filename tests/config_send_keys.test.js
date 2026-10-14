// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const { STRAIGHT_THROUGH_KEYS } = require('../src/pkjs/config_send_keys');
const configArr = require('../src/pkjs/config_clay');

// Mirror the section/item shape of config_clay (sections carry an items[] array).
function flattenConfig(cfg) {
  const out = [];
  cfg.forEach((entry) => {
    if (entry.type === 'section' && Array.isArray(entry.items)) {
      entry.items.forEach((it) => out.push(it));
    } else {
      out.push(entry);
    }
  });
  return out;
}

// Every boolean toggle the watch reads is a straight-through scalar (booleans, not
// colours/lists), so all of them MUST be in STRAIGHT_THROUGH_KEYS. A toggle absent
// from the send path renders/gates fine in the config webview but its value never
// reaches the watch — it silently stays at the C default. This is exactly how
// SettingStatusClockDigital shipped broken; this test fails if any toggle is
// forgotten again.
test('every config toggle setting is wired into the straight-through send path', () => {
  const set = new Set(STRAIGHT_THROUGH_KEYS);
  const toggles = flattenConfig(configArr)
    .filter((it) => it && it.type === 'toggle' && typeof it.messageKey === 'string'
      && it.messageKey.indexOf('Setting') === 0)
    .map((it) => it.messageKey);
  assert.ok(toggles.length > 0, 'expected at least one Setting* toggle in the config');
  const missing = toggles.filter((k) => !set.has(k));
  assert.deepStrictEqual(missing, [],
    'toggle settings missing from STRAIGHT_THROUGH_KEYS (their config value never reaches the watch): '
    + missing.join(', '));
});

// The toggle-only test above left a real gap: a `select` / `radiogroup` / numeric
// `input` is just as much a straight-through scalar, and just as silently broken when
// missing from the send path — it renders and gates perfectly in the webview while the
// watch keeps its C default. Verified at the time of writing that no existing Setting*
// item of these types is absent, so this starts green and only catches new mistakes.
// (Colours, widget lists and the crypto/currency/tuya custom components are deliberately
// NOT covered: they are not scalars and index.ts handles them on their own paths.)
test('every scalar config setting is wired into the straight-through send path', () => {
  const set = new Set(STRAIGHT_THROUGH_KEYS);
  const SCALAR_TYPES = ['toggle', 'select', 'radiogroup', 'input'];
  const scalars = flattenConfig(configArr)
    .filter((it) => it && SCALAR_TYPES.indexOf(it.type) !== -1
      && typeof it.messageKey === 'string' && it.messageKey.indexOf('Setting') === 0)
    .map((it) => it.messageKey);
  assert.ok(scalars.length > 0, 'expected at least one scalar Setting* item in the config');
  const missing = scalars.filter((k) => !set.has(k));
  assert.deepStrictEqual(missing, [],
    'scalar settings missing from STRAIGHT_THROUGH_KEYS (their config value never reaches the watch): '
    + missing.join(', '));
});

test('the night-rotation settings reach the watch', () => {
  ['SettingNightRotationMode', 'SettingNightRotationStart', 'SettingNightRotationEnd']
    .forEach((k) => {
      assert.ok(STRAIGHT_THROUGH_KEYS.indexOf(k) !== -1,
        k + ' must be sent, or night rotation silently stays at the C default (off)');
    });
});

test('SettingStatusClockDigital reaches the watch', () => {
  assert.ok(STRAIGHT_THROUGH_KEYS.indexOf('SettingStatusClockDigital') !== -1,
    'SettingStatusClockDigital must be sent so the status-strip digital-clock swap works on a real watch');
});

// Colours were exempt from the coverage tests above because index.ts sends them on its
// own path (24-bit RGB ints, not scalars) with the key list inlined in index.ts. That
// inline list is exactly as forgettable as STRAIGHT_THROUGH_KEYS was: a colour missing
// from it renders and saves in the webview and never reaches the watch. COLOR_KEYS moves
// the list into this module so it can be asserted here too.
test('every config colour setting is wired into the colour send path', () => {
  const { COLOR_KEYS } = require('../src/pkjs/config_send_keys');
  const set = new Set(COLOR_KEYS);
  const colors = flattenConfig(configArr)
    .filter((it) => it && it.type === 'color' && typeof it.messageKey === 'string'
      && it.messageKey.indexOf('Setting') === 0)
    .map((it) => it.messageKey);
  assert.ok(colors.length > 0, 'expected at least one Setting* colour item in the config');
  const missing = colors.filter((k) => !set.has(k));
  assert.deepStrictEqual(missing, [],
    'colour settings missing from COLOR_KEYS (their config value never reaches the watch): '
    + missing.join(', '));
});

test('the warning-frame settings reach the watch', () => {
  const { COLOR_KEYS } = require('../src/pkjs/config_send_keys');
  ['SettingBatteryWarnPct', 'SettingBatteryWarnDays', 'SettingBtWarnBorder'].forEach((k) => {
    assert.ok(STRAIGHT_THROUGH_KEYS.indexOf(k) !== -1,
      k + ' must be sent, or the warning frame silently stays at the C default (off)');
  });
  ['SettingBatteryWarnColor', 'SettingBtWarnColor'].forEach((k) => {
    assert.ok(COLOR_KEYS.indexOf(k) !== -1, k + ' must be sent as a colour');
  });
});

test('the night palette settings reach the watch', () => {
  const { COLOR_KEYS } = require('../src/pkjs/config_send_keys');
  ['SettingNightSlowRotation', 'SettingNightColors'].forEach((k) => {
    assert.ok(STRAIGHT_THROUGH_KEYS.indexOf(k) !== -1,
      k + ' must be sent, or the night feature silently stays at the C default');
  });
  ['SettingNightBgColor', 'SettingNightFgColor'].forEach((k) => {
    assert.ok(COLOR_KEYS.indexOf(k) !== -1, k + ' must be sent as a colour');
  });
});

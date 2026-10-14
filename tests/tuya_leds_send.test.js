// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// updateTuya() end-to-end with stubbed globals: verifies the LED rows join the
// per-device fetch, that TuyaLeds is packed and sent, that an all-unknown result is
// suppressed, and that a device shared with a sensor row is fetched only once.

const test = require('node:test');
const assert = require('node:assert');

function setup(settings, propsByDevice) {
  const store = {
    'clay-settings': JSON.stringify(settings),
    'disable_tuya': 'no',
    'tuya-catalog': JSON.stringify({ v: 1, devices: [] }),
  };
  const requested = [];
  const sent = [];
  global.window = {
    localStorage: {
      getItem: (k) => (k in store ? store[k] : null),
      setItem: (k, v) => { store[k] = String(v); },
    },
  };
  global.Pebble = {
    sendAppMessage: (dict, ok) => { sent.push(dict); if (ok) { ok(); } },
  };
  // Stub XMLHttpRequest: every Tuya request resolves from propsByDevice.
  global.XMLHttpRequest = function () {
    this.open = (method, url) => { this._url = url; };
    this.setRequestHeader = () => {};
    this.send = () => {
      requested.push(this._url);
      const m = /thing\/([^/]+)\/shadow/.exec(this._url);
      let body;
      if (m) {
        const props = propsByDevice[m[1]];
        body = props
          ? { success: true, result: { properties: props } }
          : { success: false };
      } else {
        // token / device-list / specification calls
        body = { success: true, result: { access_token: 't', expire_time: 7200, devices: [] } };
      }
      this.responseText = JSON.stringify(body);
      if (this.onload) { this.onload(); }
    };
  };
  return { store, requested, sent };
}

const CREDS = { TuyaAccessId: 'id', TuyaAccessSecret: 'secret', TuyaRegion: 'eu' };

// Each test needs a fresh module instance: tuya.ts memoises its Tuya client.
function freshTuya() {
  delete require.cache[require.resolve('../src/pkjs/tuya')];
  delete require.cache[require.resolve('../src/pkjs/tuya_client')];
  return require('../src/pkjs/tuya');
}

test('LED states are packed and sent under TuyaLeds', async () => {
  const ctx = setup(
    Object.assign({}, CREDS, {
      TuyaList: [],
      TuyaLedList: [{ deviceId: 'dev1', code: 'switch_1' }, { deviceId: 'dev1', code: 'switch_2' }],
    }),
    { dev1: [{ code: 'switch_1', value: true }, { code: 'switch_2', value: false }] });
  freshTuya().updateTuya(true);
  await new Promise((r) => setTimeout(r, 50));
  assert.strictEqual(ctx.sent.length, 1);
  assert.strictEqual(ctx.sent[0].TuyaLeds, '10');
  assert.strictEqual(ctx.store['tuya_leds_last_sent'], '10');
});

test('a device used by both a sensor row and an LED row is fetched once', async () => {
  const ctx = setup(
    Object.assign({}, CREDS, {
      TuyaList: [{ wid: 128, deviceId: 'dev1', code: 'va_temperature', p: 1, t: 0, label: 'T' }],
      TuyaLedList: [{ deviceId: 'dev1', code: 'switch_1' }],
    }),
    { dev1: [{ code: 'va_temperature', value: 235 }, { code: 'switch_1', value: true }] });
  freshTuya().updateTuya(true);
  await new Promise((r) => setTimeout(r, 50));
  const shadowCalls = ctx.requested.filter((u) => u.indexOf('/shadow/properties') !== -1);
  assert.strictEqual(shadowCalls.length, 1);
  assert.strictEqual(ctx.sent[0].TuyaLeds, '1');
  assert.ok(typeof ctx.sent[0].TuyaData === 'string');
});

test('an all-unknown LED result is not sent', async () => {
  const ctx = setup(
    Object.assign({}, CREDS, {
      TuyaList: [],
      TuyaLedList: [{ deviceId: 'gone', code: 'switch_1' }],
    }),
    {});
  freshTuya().updateTuya(true);
  await new Promise((r) => setTimeout(r, 50));
  assert.strictEqual(ctx.sent.length, 0);
});

test('a partial LED answer is sent with ? for the unreachable device', async () => {
  const ctx = setup(
    Object.assign({}, CREDS, {
      TuyaList: [],
      TuyaLedList: [{ deviceId: 'dev1', code: 'switch_1' }, { deviceId: 'gone', code: 'switch_1' }],
    }),
    { dev1: [{ code: 'switch_1', value: false }] });
  freshTuya().updateTuya(true);
  await new Promise((r) => setTimeout(r, 50));
  assert.strictEqual(ctx.sent.length, 1);
  assert.strictEqual(ctx.sent[0].TuyaLeds, '0?');
});

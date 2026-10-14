// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
const test = require('node:test');
const assert = require('node:assert');
const {
  buildStringToSign, buildSignString, sign, createClient, EMPTY_BODY_SHA256,
} = require('../src/pkjs/tuya_client');

test('buildStringToSign: GET empty body', () => {
  assert.strictEqual(
    buildStringToSign('GET', '/v1.0/token?grant_type=1', ''),
    'GET\n' + EMPTY_BODY_SHA256 + '\n\n/v1.0/token?grant_type=1');
});

test('buildStringToSign: POST hashes the body', () => {
  const lines = buildStringToSign('POST', '/x', '{"a":1}').split('\n');
  assert.strictEqual(lines[0], 'POST');
  assert.match(lines[1], /^[0-9a-f]{64}$/);
  assert.notStrictEqual(lines[1], EMPTY_BODY_SHA256);
  assert.strictEqual(lines[2], '');
  assert.strictEqual(lines[3], '/x');
});

test('buildSignString: token req omits access_token, business req inserts it', () => {
  const t = '1588925778000', nonce = 'n1', cid = 'CID', sts = 'GET\nx\n\n/p';
  assert.strictEqual(buildSignString({ clientId: cid, t, nonce, stringToSign: sts }), cid + t + nonce + sts);
  assert.strictEqual(buildSignString({ clientId: cid, accessToken: 'TOK', t, nonce, stringToSign: sts }),
    cid + 'TOK' + t + nonce + sts);
});

test('sign: uppercase 64-hex, deterministic, secret-dependent', () => {
  assert.match(sign('abc', 'k1'), /^[0-9A-F]{64}$/);
  assert.strictEqual(sign('abc', 'k1'), sign('abc', 'k1'));
  assert.notStrictEqual(sign('abc', 'k1'), sign('abc', 'k2'));
});

function fakeHttp(responses) {
  const calls = [];
  function http(opts) { calls.push(opts); return Promise.resolve(responses.shift()); }
  http.calls = calls;
  return http;
}
const cfg = { clientId: 'CID', secret: 'SEC', host: 'https://openapi.tuyaeu.com' };
const deps = { now: () => 1588925778000, nonce: () => 'NONCE' };

test('getToken signs a token request without access_token', async () => {
  const http = fakeHttp([{ success: true, result: { access_token: 'TOK', expire_time: 7200 } }]);
  const c = createClient(cfg, http, deps);
  assert.strictEqual(await c.getToken(), 'TOK');
  assert.strictEqual(http.calls[0].url, 'https://openapi.tuyaeu.com/v1.0/token?grant_type=1');
  assert.strictEqual(http.calls[0].headers.client_id, 'CID');
  assert.strictEqual(http.calls[0].headers.access_token, undefined);
  assert.match(http.calls[0].headers.sign, /^[0-9A-F]{64}$/);
});

test('request fetches token then signs with access_token', async () => {
  const http = fakeHttp([
    { success: true, result: { access_token: 'TOK', expire_time: 7200 } },
    { success: true, result: { devices: [] } }]);
  const c = createClient(cfg, http, deps);
  const res = await c.request('GET', '/v1.0/iot-01/associated-users/devices');
  assert.deepStrictEqual(res.result.devices, []);
  assert.strictEqual(http.calls[1].headers.access_token, 'TOK');
});

test('request throws on non-auth error (no retry)', async () => {
  const http = fakeHttp([
    { success: true, result: { access_token: 'T', expire_time: 7200 } },
    { success: false, code: 2007, msg: 'offline' }]);
  const c = createClient(cfg, http, deps);
  await assert.rejects(() => c.request('GET', '/x'), /2007/);
  assert.strictEqual(http.calls.length, 2);
});

test('request clears token and retries once on 1010', async () => {
  const http = fakeHttp([
    { success: true, result: { access_token: 'T1', expire_time: 7200 } },
    { success: false, code: 1010, msg: 'bad' },
    { success: true, result: { access_token: 'T2', expire_time: 7200 } },
    { success: true, result: { ok: 1 } }]);
  const c = createClient(cfg, http, deps);
  assert.strictEqual((await c.request('GET', '/x')).result.ok, 1);
  assert.strictEqual(http.calls.length, 4);
  assert.strictEqual(http.calls[3].headers.access_token, 'T2');
});

test('a persisted valid token is reused', async () => {
  const http = fakeHttp([{ success: true, result: { ok: 1 } }]);
  const d = { now: () => 1000, nonce: () => 'N',
    loadToken: (cid) => (cid === 'CID' ? { token: 'CACHED', expiresAt: 9e15 } : null) };
  const c = createClient(cfg, http, d);
  await c.request('GET', '/x');
  assert.strictEqual(http.calls.length, 1);
  assert.strictEqual(http.calls[0].headers.access_token, 'CACHED');
});

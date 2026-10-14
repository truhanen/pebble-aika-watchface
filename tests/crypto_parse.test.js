// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const {
  buildPriceUrl, normalizeRows, packCryptoData, countValidPrices, DELIM,
} = require('../src/pkjs/crypto_parse');

const ROWS = [
  { wid: 15, coin: 'bitcoin', vs: 'usd', p: -3, label: 'BTC' },
  { wid: 200, coin: 'ethereum', vs: 'eur', p: 0, label: 'ETH' },
];

test('buildPriceUrl requests unique ids and the union of vs-currencies', () => {
  const url = buildPriceUrl(ROWS);
  assert.ok(url.indexOf('ids=bitcoin,ethereum') !== -1, url);
  assert.ok(/vs_currencies=(usd,eur|eur,usd)/.test(url), url);
  assert.ok(url.indexOf('precision=8') !== -1, url);
});

test('buildPriceUrl dedupes repeated coin ids', () => {
  const url = buildPriceUrl([
    { wid: 15, coin: 'bitcoin', vs: 'usd', p: -3, label: 'BTC' },
    { wid: 200, coin: 'bitcoin', vs: 'eur', p: -3, label: 'BTCE' },
  ]);
  assert.ok(url.indexOf('ids=bitcoin&') !== -1 || url.indexOf('ids=bitcoin&vs') !== -1, url);
});

test('normalizeRows drops malformed rows and coerces types', () => {
  const out = normalizeRows([
    { wid: '15', coin: 'bitcoin', vs: 'usd', p: '-3', label: 'BTC' },
    { wid: 200, coin: '', vs: 'usd', p: 0, label: 'X' },
    { coin: 'monero', vs: 'usd', p: 0, label: 'XMR' },
    'garbage',
  ]);
  assert.strictEqual(out.length, 1);
  assert.deepStrictEqual(out[0], { wid: 15, coin: 'bitcoin', vs: 'usd', p: -3, t: 0, label: 'BTC' });
});

test('packCryptoData formats each row and joins wid/label/value triplets', () => {
  const json = { bitcoin: { usd: 104235 }, ethereum: { eur: 3520.4 } };
  const packed = packCryptoData(ROWS, json);
  assert.strictEqual(packed, ['15', 'BTC', '104', '200', 'ETH', '3520'].join(DELIM));
});

test('packCryptoData emits -- for a missing price but keeps the slot', () => {
  const json = { bitcoin: { usd: 104235 } };
  const packed = packCryptoData(ROWS, json);
  assert.strictEqual(packed, ['15', 'BTC', '104', '200', 'ETH', '--'].join(DELIM));
});

test('normalizeRows clamps precision to [-8, 8]', () => {
  const out = normalizeRows([
    { wid: 200, coin: 'shiba-inu', vs: 'usd', p: 200, label: 'SHIB' },
    { wid: 201, coin: 'bitcoin', vs: 'usd', p: -50, label: 'BTC' },
  ]);
  assert.strictEqual(out[0].p, 8);
  assert.strictEqual(out[1].p, -8);
});

test('label falls back to the uppercased coin id when empty', () => {
  const packed = packCryptoData(
    [{ wid: 200, coin: 'dogecoin', vs: 'usd', p: 4, label: '' }],
    { dogecoin: { usd: 0.1234 } });
  assert.strictEqual(packed, ['200', 'DOGECOIN', '0.1234'].join(DELIM));
});

test('countValidPrices counts coins with a finite price', () => {
  const json = { bitcoin: { usd: 104235 }, ethereum: { eur: 3520 } };
  assert.strictEqual(countValidPrices(ROWS, json), 2);
});

test('countValidPrices ignores missing / non-finite prices', () => {
  // ethereum price absent -> only bitcoin counts
  assert.strictEqual(countValidPrices(ROWS, { bitcoin: { usd: 104235 } }), 1);
});

test('countValidPrices is 0 for a CoinGecko rate-limit (429) error body', () => {
  // a 429 JSON body has none of the requested coin keys -> nothing valid.
  // This is the signal updateCrypto uses to AVOID overwriting good prices with "--".
  const errBody = { status: { error_code: 429, error_message: 'rate limited' } };
  assert.strictEqual(countValidPrices(ROWS, errBody), 0);
});

test('countValidPrices is 0 for an empty / non-object response', () => {
  assert.strictEqual(countValidPrices(ROWS, {}), 0);
  assert.strictEqual(countValidPrices(ROWS, null), 0);
});

test('normalizeRows parses t, defaults missing/NaN to 0', () => {
  const out = normalizeRows([
    { wid: 15, coin: 'bitcoin', vs: 'usd', p: -3, t: 2, label: 'BTC' },
    { wid: 200, coin: 'euro-coin', vs: 'eur', p: 3, label: 'EUR' },
  ]);
  assert.strictEqual(out[0].t, 2);
  assert.strictEqual(out[1].t, 0);
});

test('normalizeRows clamps t to [0, 15]', () => {
  const out = normalizeRows([
    { wid: 200, coin: 'bitcoin', vs: 'usd', p: 0, t: 99, label: 'BTC' },
    { wid: 201, coin: 'ethereum', vs: 'usd', p: 0, t: -5, label: 'ETH' },
  ]);
  assert.strictEqual(out[0].t, 15);
  assert.strictEqual(out[1].t, 0);
});

test('packCryptoData applies the per-row trim', () => {
  const packed = packCryptoData(
    [{ wid: 17, coin: 'euro-coin', vs: 'eur', p: 3, t: 2, label: 'EUR' }],
    { 'euro-coin': { eur: 1.160 } });
  assert.strictEqual(packed, '17' + DELIM + 'EUR' + DELIM + '60');
});

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const {
  normalizeRows, distinctBases, buildRateUrl, packCurrencyData,
  countValidRates, parseLastSent, DELIM,
} = require('../src/pkjs/currency_parse');

const ROWS = [
  { wid: 216, base: 'EUR', quote: 'USD', p: 4, label: '' },
  { wid: 217, base: 'USD', quote: 'JPY', p: 2, label: 'Yen' },
];
const RATES = { EUR: { rates: { USD: 1.0823 } }, USD: { rates: { JPY: 162.4 } } };
// packCurrencyData takes ratesByBase = { base: <rates object> }, so unwrap:
const BY_BASE = { EUR: { USD: 1.0823 }, USD: { JPY: 162.4 } };

test('normalizeRows uppercases codes, clamps precision, drops malformed', () => {
  const out = normalizeRows([
    { wid: '216', base: 'eur', quote: 'usd', p: '4', label: '' },
    { wid: 217, base: 'USD', quote: '', p: 2, label: 'X' },   // empty quote -> dropped
    { base: 'GBP', quote: 'USD', p: 2 },                       // no wid -> dropped
    { wid: 218, base: 'gbp', quote: 'jpy', p: 99, label: '' }, // p clamps to 8
    'garbage',
  ]);
  assert.strictEqual(out.length, 2);
  assert.deepStrictEqual(out[0], { wid: 216, base: 'EUR', quote: 'USD', p: 4, t: 0, label: '' });
  assert.deepStrictEqual(out[1], { wid: 218, base: 'GBP', quote: 'JPY', p: 8, t: 0, label: '' });
});

test('normalizeRows: precision defaults to 4, allows negatives (round), clamps to [-8,8]', () => {
  const out = normalizeRows([
    { wid: 216, base: 'EUR', quote: 'USD', label: '' },         // no p -> 4
    { wid: 217, base: 'USD', quote: 'JPY', p: -3, label: '' },  // negative preserved (rounding)
    { wid: 218, base: 'GBP', quote: 'JPY', p: 99, label: '' },  // clamps to 8
    { wid: 219, base: 'CHF', quote: 'JPY', p: -50, label: '' }, // clamps to -8
  ]);
  assert.strictEqual(out[0].p, 4);
  assert.strictEqual(out[1].p, -3);
  assert.strictEqual(out[2].p, 8);
  assert.strictEqual(out[3].p, -8);
});

test('normalizeRows: trim t defaults to 0, floors negatives, clamps to 15', () => {
  const out = normalizeRows([
    { wid: 216, base: 'EUR', quote: 'USD', p: 4, t: 2, label: '' },
    { wid: 217, base: 'USD', quote: 'JPY', p: 2, label: '' },        // no t -> 0
    { wid: 218, base: 'GBP', quote: 'JPY', p: 2, t: -5, label: '' }, // negative -> 0
    { wid: 219, base: 'CHF', quote: 'JPY', p: 2, t: 99, label: '' }, // clamps to 15
  ]);
  assert.strictEqual(out[0].t, 2);
  assert.strictEqual(out[1].t, 0);
  assert.strictEqual(out[2].t, 0);
  assert.strictEqual(out[3].t, 15);
});

test('distinctBases returns unique bases in order', () => {
  assert.deepStrictEqual(distinctBases(ROWS), ['EUR', 'USD']);
  assert.deepStrictEqual(distinctBases([
    { wid: 216, base: 'EUR', quote: 'USD', p: 4, label: '' },
    { wid: 217, base: 'EUR', quote: 'GBP', p: 4, label: '' },
  ]), ['EUR']);
});

test('buildRateUrl targets the open.er-api latest endpoint for the base', () => {
  assert.strictEqual(buildRateUrl('EUR'), 'https://open.er-api.com/v6/latest/EUR');
});

test('packCurrencyData formats each row; label falls back to the quote currency', () => {
  const packed = packCurrencyData(ROWS, BY_BASE, {});
  assert.strictEqual(packed, ['216', 'USD', '1.0823', '217', 'Yen', '162.40'].join(DELIM));
});

test('packCurrencyData uses prevValues when a rate is missing, else --', () => {
  const partial = { EUR: { USD: 1.0823 } };   // USD base absent this round
  const withPrev = packCurrencyData(ROWS, partial, { 217: '161.00' });
  assert.strictEqual(withPrev, ['216', 'USD', '1.0823', '217', 'Yen', '161.00'].join(DELIM));
  const noPrev = packCurrencyData(ROWS, partial, {});
  assert.strictEqual(noPrev, ['216', 'USD', '1.0823', '217', 'Yen', '--'].join(DELIM));
});

test('packCurrencyData: negative precision rounds (crypto-style)', () => {
  const packed = packCurrencyData(
    [{ wid: 216, base: 'USD', quote: 'JPY', p: -1, t: 0, label: 'JPY' }],
    { USD: { JPY: 162.4 } }, {});
  assert.strictEqual(packed, ['216', 'JPY', '16'].join(DELIM));   // round(162.4/10) = 16
});

test('packCurrencyData: trim cuts leading digits (crypto-style)', () => {
  const packed = packCurrencyData(
    [{ wid: 216, base: 'EUR', quote: 'USD', p: 3, t: 2, label: 'X' }],
    { EUR: { USD: 1.160 } }, {});
  assert.strictEqual(packed, ['216', 'X', '60'].join(DELIM));     // "1.160" trim 2 -> "60"
});

test('countValidRates counts rows with a finite fresh rate', () => {
  assert.strictEqual(countValidRates(ROWS, BY_BASE), 2);
  assert.strictEqual(countValidRates(ROWS, { EUR: { USD: 1.0823 } }), 1);
  assert.strictEqual(countValidRates(ROWS, {}), 0);
  assert.strictEqual(countValidRates(ROWS, null), 0);
});

test('parseLastSent maps wid -> value from a packed string', () => {
  const packed = ['216', 'EUR/USD', '1.0823', '217', 'Yen', '162.40'].join(DELIM);
  assert.deepStrictEqual(parseLastSent(packed), { '216': '1.0823', '217': '162.40' });
  assert.deepStrictEqual(parseLastSent(''), {});
});

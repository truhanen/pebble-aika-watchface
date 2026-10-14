// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const { seedCryptoList } = require('../src/pkjs/crypto_migrate');

test('seeds only legacy coins whose disable flag was not "yes"', () => {
  const flags = { disable_btc: 'no', disable_xmr: 'yes', disable_eurusd: 'no' };
  const list = seedCryptoList(flags);
  assert.deepStrictEqual(list, [
    { wid: 15, coin: 'bitcoin', vs: 'usd', p: -3, t: 0, label: 'BTC' },
    { wid: 17, coin: 'euro-coin', vs: 'usd', p: 3, t: 0, label: 'EUR' },
  ]);
});

test('returns an empty list when no legacy coin was enabled', () => {
  assert.deepStrictEqual(seedCryptoList({}), []);
  assert.deepStrictEqual(
    seedCryptoList({ disable_btc: 'yes', disable_xmr: 'yes', disable_eurusd: 'yes' }), []);
});

test('fresh install (null flags) seeds nothing', () => {
  assert.deepStrictEqual(
    seedCryptoList({ disable_btc: null, disable_xmr: null, disable_eurusd: null }), []);
});

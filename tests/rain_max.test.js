// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const {
  hourlyPrecip, rainMaxHundredths, packRainMax, rainMaxBytes, openMeteoPrecipSamples,
  RAIN_UNKNOWN,
} = require('../src/pkjs/rain_max');

const N = 12;
const nan12 = () => Array(N).fill(NaN);

test('rainMaxHundredths: maxima over the first 1/3/6/9/12 hours', () => {
  const p = [0.1, 0, 0.5, 0, 0, 0, 1.25, 0, 0, 0, 0, 3];
  assert.deepStrictEqual(rainMaxHundredths(p), [10, 50, 50, 125, 300]);
});

test('rainMaxHundredths: all dry is 0, not unknown', () => {
  assert.deepStrictEqual(rainMaxHundredths(Array(N).fill(0)), [0, 0, 0, 0, 0]);
});

test('rainMaxHundredths: no data at all is unknown for every window', () => {
  assert.deepStrictEqual(rainMaxHundredths(nan12()), Array(5).fill(RAIN_UNKNOWN));
  assert.deepStrictEqual(rainMaxHundredths([]), Array(5).fill(RAIN_UNKNOWN));
});

test('rainMaxHundredths: unknown hours are skipped; a window with none known is unknown', () => {
  const p = nan12();
  p[1] = 0.2; p[7] = 0.9;
  // N=1 -> p[0] unknown; N=3 -> 0.2; N=6 -> 0.2; N=9 -> 0.9; N=12 -> 0.9
  assert.deepStrictEqual(rainMaxHundredths(p), [RAIN_UNKNOWN, 20, 20, 90, 90]);
});

test('rainMaxHundredths: rounding and clamping below the sentinel', () => {
  assert.strictEqual(rainMaxHundredths([0.126].concat(Array(11).fill(0)))[0], 13);
  assert.strictEqual(rainMaxHundredths([9999].concat(Array(11).fill(0)))[0], 0xFFFE);
});

test('packRainMax: five little-endian uint16 values, 10 bytes', () => {
  assert.deepStrictEqual(packRainMax([0, 1, 0x1234, RAIN_UNKNOWN, 300]),
    [0, 0, 1, 0, 0x34, 0x12, 0xFF, 0xFF, 0x2C, 0x01]);
});

// now = 12:34 UTC; H = 12:00. p[0] is the sample stamped 13:00.
const NOW = Date.parse('2026-06-10T12:34:00Z') / 1000;
const at = (hh) => Date.parse('2026-06-10T' + hh + ':00:00Z') / 1000;

test('hourlyPrecip: p[0] is the sample stamped at the END of the current hour', () => {
  const p = hourlyPrecip([
    { epoch: at('12'), value: 9 },      // the previous hour: excluded
    { epoch: at('13'), value: 0.4 },    // p[0]
    { epoch: at('14'), value: 0.0 },    // p[1]
    { epoch: at('23'), value: 1 },      // p[10]
    { epoch: at('23') + 3600, value: 2 },   // 00:00 next day = p[11]
    { epoch: at('23') + 7200, value: 7 },   // beyond the window
  ], NOW);
  assert.strictEqual(p.length, N);
  assert.strictEqual(p[0], 0.4);
  assert.strictEqual(p[1], 0);
  assert.ok(isNaN(p[2]));
  assert.strictEqual(p[10], 1);
  assert.strictEqual(p[11], 2);
});

test('hourlyPrecip: NaN, negative and off-hour samples', () => {
  const p = hourlyPrecip([
    { epoch: at('13'), value: NaN },
    { epoch: at('14'), value: -0.3 },
    { epoch: at('15') + 1800, value: 5 },
  ], NOW);
  assert.ok(isNaN(p[0]));
  assert.strictEqual(p[1], 0);
  assert.ok(isNaN(p[2]));
});

test('rainMaxBytes end to end', () => {
  const s = [{ epoch: at('13'), value: 0.5 }, { epoch: at('16'), value: 2 }];
  // p[0]=.5 p[3]=2 -> N=1:50, 3:50, 6:200, 9:200, 12:200 (unknown hours skipped)
  assert.deepStrictEqual(rainMaxBytes(s, NOW),
    [50, 0, 50, 0, 200, 0, 200, 0, 200, 0]);
  assert.deepStrictEqual(rainMaxBytes([], NOW), Array(10).fill(0xFF));
});

test('openMeteoPrecipSamples: local times are converted with utc_offset_seconds', () => {
  // Helsinki summer = UTC+3: local 16:00 == 13:00Z
  const s = openMeteoPrecipSamples(
    ['2026-06-10T16:00', '2026-06-10T17:00', '2026-06-10T18:00'], [0.4, null, 1.5], 10800);
  assert.deepStrictEqual(s, [
    { epoch: at('13'), value: 0.4 },
    { epoch: at('15'), value: 1.5 },
  ]);
  const p = hourlyPrecip(s, NOW);
  assert.strictEqual(p[0], 0.4);
  assert.strictEqual(p[2], 1.5);
});

test('openMeteoPrecipSamples: missing/garbage input yields no samples', () => {
  assert.deepStrictEqual(openMeteoPrecipSamples(undefined, undefined, 0), []);
  assert.deepStrictEqual(openMeteoPrecipSamples(['x'], [1], 0), []);
});

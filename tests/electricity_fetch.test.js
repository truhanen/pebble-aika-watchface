// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const {
  elecFetchDue,
  elecExpectedTableEndEpoch,
  ELEC_FULL_INTERVAL_S,
  ELEC_STALE_INTERVAL_S,
  ELEC_PUBLISH_HOUR_LOCAL,
} = require('../src/pkjs/electricity_parse');

const H = 3600;
const DAY = 86400;
const NOW = 1781787680;
const MIDNIGHT = 1781733600; // an arbitrary local midnight (epoch s)

// --- elecExpectedTableEndEpoch (publication-aware expected coverage) ---------

test('before the publish hour, only today is expected (midnight + 1 day)', () => {
  assert.strictEqual(
    elecExpectedTableEndEpoch(MIDNIGHT, ELEC_PUBLISH_HOUR_LOCAL - 1),
    MIDNIGHT + 1 * DAY);
});

test('at the publish hour, tomorrow is expected (midnight + 2 days)', () => {
  assert.strictEqual(
    elecExpectedTableEndEpoch(MIDNIGHT, ELEC_PUBLISH_HOUR_LOCAL),
    MIDNIGHT + 2 * DAY);
});

test('after the publish hour, tomorrow is expected (midnight + 2 days)', () => {
  assert.strictEqual(
    elecExpectedTableEndEpoch(MIDNIGHT, 23),
    MIDNIGHT + 2 * DAY);
});

// --- elecFetchDue (throttle keyed on whether the table reaches expectedEnd) --

test('forceUpdate always fetches, even right after a fetch', () => {
  assert.strictEqual(
    elecFetchDue(true, NOW, NOW - 60, NOW + 48 * H, NOW + 48 * H), true);
});

test('table reaches expectedEnd, within the normal interval -> not due', () => {
  assert.strictEqual(
    elecFetchDue(false, NOW, NOW - 1 * H, NOW + 30 * H, NOW + 24 * H), false);
});

test('table reaches expectedEnd, past the normal interval -> due (safety refresh)', () => {
  assert.strictEqual(
    elecFetchDue(false, NOW, NOW - (ELEC_FULL_INTERVAL_S + 60), NOW + 30 * H, NOW + 24 * H), true);
});

test('table short of expectedEnd, within the stale interval -> not due', () => {
  // tableEnd (NOW+8h) < expectedEnd (NOW+24h): tomorrow expected but missing.
  // last fetch within the stale interval -> wait.
  assert.strictEqual(
    elecFetchDue(false, NOW, NOW - (ELEC_STALE_INTERVAL_S - 60), NOW + 8 * H, NOW + 24 * H), false);
});

test('table short of expectedEnd, past the stale interval -> due (the bug fix)', () => {
  // Tomorrow's prices are published but the watch still holds today only.
  // Under the old 11h-only throttle this returned false and the widget stayed "--".
  assert.strictEqual(
    elecFetchDue(false, NOW, NOW - (ELEC_STALE_INTERVAL_S + 60), NOW + 8 * H, NOW + 24 * H), true);
});

test('unknown table end with no prior fetch is due', () => {
  assert.strictEqual(elecFetchDue(false, NOW, 0, 0, NOW + 24 * H), true);
});

test('constants are ordered sensibly', () => {
  assert.ok(ELEC_STALE_INTERVAL_S < ELEC_FULL_INTERVAL_S);
  assert.ok(ELEC_PUBLISH_HOUR_LOCAL >= 0 && ELEC_PUBLISH_HOUR_LOCAL <= 23);
});

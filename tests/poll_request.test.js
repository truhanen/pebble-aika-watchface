// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const { isWatchPollRequest } = require('../src/pkjs/poll_request');

test('the watch poll-request (dummy key 0) is a poll request', () => {
  // messaging_requestNewWeatherData() sends dict_write_uint32(iter, 0, 0)
  assert.strictEqual(isWatchPollRequest({ '0': 0 }), true);
});

test('an empty payload is treated as a poll request', () => {
  assert.strictEqual(isWatchPollRequest({}), true);
});

test('a message carrying data keys is NOT a poll request', () => {
  assert.strictEqual(isWatchPollRequest({ WeatherTemperature: 12 }), false);
});

test('a mixed message (poll key + data key) is NOT a poll request', () => {
  assert.strictEqual(isWatchPollRequest({ '0': 0, WeatherTemperature: 12 }), false);
});

test('null / undefined payloads are not poll requests', () => {
  assert.strictEqual(isWatchPollRequest(null), false);
  assert.strictEqual(isWatchPollRequest(undefined), false);
});

const { isColdPollRequest } = require('../src/pkjs/poll_request');

test('a cold poll request is recognised', () => {
  // messaging_requestNewWeatherData(true) sends dict_write_uint32(iter, 0, 1)
  assert.strictEqual(isColdPollRequest({ '0': 1 }), true);
});

test('a normal poll request is not cold', () => {
  assert.strictEqual(isColdPollRequest({ '0': 0 }), false);
  assert.strictEqual(isColdPollRequest({}), false);
});

test('a data message is never a cold poll request', () => {
  assert.strictEqual(isColdPollRequest({ WeatherTemperature: 12 }), false);
  assert.strictEqual(isColdPollRequest({ '0': 1, WeatherTemperature: 12 }), false);
});

test('a string-valued cold flag still counts (AppMessage type coercion)', () => {
  assert.strictEqual(isColdPollRequest({ '0': '1' }), true);
});

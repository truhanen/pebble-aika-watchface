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

test('a twt_status push relayed from the Android app is NOT a poll request', () => {
  // a tracking toggle carries TWT_* keys and must not trigger a phone fetch
  assert.strictEqual(isWatchPollRequest({ TWT_IS_TRACKING: 1, TWT_TASK_ID: 4 }), false);
});

test('a midi_status push is NOT a poll request', () => {
  assert.strictEqual(isWatchPollRequest({ MIDI_IS_RECORDING: 1 }), false);
});

test('a mixed message (poll key + data key) is NOT a poll request', () => {
  assert.strictEqual(isWatchPollRequest({ '0': 0, TWT_IS_TRACKING: 1 }), false);
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

test('a companion-app push is never a cold poll request', () => {
  assert.strictEqual(isColdPollRequest({ TWT_IS_TRACKING: 1 }), false);
  assert.strictEqual(isColdPollRequest({ '0': 1, TWT_IS_TRACKING: 1 }), false);
});

test('a string-valued cold flag still counts (AppMessage type coercion)', () => {
  assert.strictEqual(isColdPollRequest({ '0': '1' }), true);
});

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Recognizes the watch's data-poll request: an otherwise-empty message carrying
   only the dummy key "0" (messaging_requestNewWeatherData -> dict_write_uint32
   key 0). Any message carrying other keys is not a poll request. Pure + no
   globals, so unit-testable with `node --test`. */

export function isWatchPollRequest(payload: any): boolean {
  if (!payload || typeof payload !== 'object') { return false; }
  const keys = Object.keys(payload);
  for (let i = 0; i < keys.length; i++) {
    // the watch's poll carries only the dummy key "0"
    if (keys[i] !== '0') { return false; }
  }
  return true;
}

// The watch flags a request as "cold" (value 1 in the dummy key) when a placed
// phone-data widget has no persisted data on the watch -- e.g. after a watchface
// reinstall, which wipes the watch's persist while the phone's *_last_sent stamps
// survive and would otherwise suppress a re-send as "unchanged". A cold request is
// the only one that bypasses the per-source throttles.
export function isColdPollRequest(payload: any): boolean {
  if (!isWatchPollRequest(payload)) { return false; }
  return parseInt(payload['0'], 10) === 1;
}

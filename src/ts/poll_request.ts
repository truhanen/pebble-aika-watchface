// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Tells a genuine watch data-poll request apart from a status push that merely
   shares this watchface's AppMessage channel.

   The watch asks the phone for fresh weather/electricity/crypto data with an
   otherwise-empty message (messaging_requestNewWeatherData -> dict_write_uint32
   key 0). But a companion Android app (trackworktime, MIDI Recorder) sends its
   status to THIS watchface's UUID too, so those pushes are delivered to the same
   `appmessage` listener. Treating them as poll requests made every tracking
   toggle hit the rate-limited crypto API, which blanked the widgets to "--".

   So: only the empty poll message (or a payload whose only key is the dummy "0")
   counts as a poll request; anything carrying named data keys (TWT_ / MIDI_ ...)
   does not. Pure + no globals, so unit-testable with `node --test`. */

export function isWatchPollRequest(payload: any): boolean {
  if (!payload || typeof payload !== 'object') { return false; }
  const keys = Object.keys(payload);
  for (let i = 0; i < keys.length; i++) {
    // the watch's poll carries only the dummy key "0"; any other key means this
    // is a status push relayed from a companion app, not a data request.
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

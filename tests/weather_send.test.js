// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// sendWeatherToPebble() unchanged-suppression. Weather used to be the only source with
// no such check, so every poll woke the watch over Bluetooth even when nothing had
// changed. The failure mode of getting this wrong is silent and severe -- weather stops
// updating entirely -- so both directions are pinned here.

const test = require('node:test');
const assert = require('node:assert');

function setup() {
  // A stored location keeps updateWeather() on the provider path; without it the call
  // falls through to navigator.geolocation, which Node 24 exposes as a read-only global
  // that a test cannot replace. The provider's fetch is a no-op XHR stub below, so
  // nothing is sent -- we only care about the force flag updateWeather sets on the way.
  const store = {
    'disable_weather': 'no',
    'weather_loc': 'Helsinki',
    'weather_loc_lat': '60.17',
    'weather_loc_lng': '24.94',
  };
  const sent = [];
  global.window = {
    localStorage: {
      getItem: (k) => (k in store ? store[k] : null),
      setItem: (k, v) => { store[k] = String(v); },
      removeItem: (k) => { delete store[k]; },
    },
  };
  // ok() = the watch acknowledged; fail() = delivery failed.
  let deliver = true;
  global.Pebble = {
    sendAppMessage: (dict, ok, fail) => {
      sent.push(dict);
      if (deliver) { if (ok) { ok(); } } else if (fail) { fail(); }
    },
  };
  global.XMLHttpRequest = function () {
    this.open = () => {}; this.setRequestHeader = () => {}; this.send = () => {};
  };
  delete require.cache[require.resolve('../src/pkjs/weather.js')];
  const weather = require('../src/pkjs/weather.js');
  return { weather, sent, store, setDeliver: (v) => { deliver = v; } };
}

const DICT = { WeatherTemperature: 12, WeatherCondition: 3 };

test('an identical payload is not re-sent', () => {
  const { weather, sent } = setup();
  weather.sendWeatherToPebble(DICT);
  weather.sendWeatherToPebble({ WeatherTemperature: 12, WeatherCondition: 3 });
  assert.strictEqual(sent.length, 1, 'the second, identical payload must be suppressed');
});

test('a changed payload is sent', () => {
  const { weather, sent } = setup();
  weather.sendWeatherToPebble(DICT);
  weather.sendWeatherToPebble({ WeatherTemperature: 13, WeatherCondition: 3 });
  assert.strictEqual(sent.length, 2);
  assert.strictEqual(sent[1].WeatherTemperature, 13);
});

test('a forced update sends even an identical payload', () => {
  const { weather, sent } = setup();
  weather.sendWeatherToPebble(DICT);
  // a cold request means the watch has NO data, so "unchanged since we last sent" says
  // nothing about what the watch actually holds
  weather.updateWeather(true);
  weather.sendWeatherToPebble(DICT);
  assert.strictEqual(sent.length, 2, 'a forced update must bypass the suppression');
});

test('the force flag is consumed by one send, not sticky', () => {
  const { weather, sent } = setup();
  weather.updateWeather(true);
  weather.sendWeatherToPebble(DICT);
  weather.sendWeatherToPebble(DICT);
  assert.strictEqual(sent.length, 1, 'the second send must fall back to the normal check');
});

test('a failed delivery is retried rather than remembered', () => {
  const { weather, sent, setDeliver } = setup();
  setDeliver(false);
  weather.sendWeatherToPebble(DICT);
  assert.strictEqual(sent.length, 1);
  // the watch never acknowledged, so the same payload must be allowed through again
  setDeliver(true);
  weather.sendWeatherToPebble(DICT);
  assert.strictEqual(sent.length, 2, 'an unacknowledged payload must not be treated as sent');
});

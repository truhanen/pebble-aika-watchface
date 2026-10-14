// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

import * as weather from './weather';
import * as electricity from './electricity';
import * as crypto from './crypto';
import * as currency from './currency';
import * as tuya from './tuya';
import cryptoListComponent from './config_crypto_list';
import currencyListComponent from './config_currency_list';
import tuyaListComponent from './config_tuya_list';
import tuyaCatalogComponent from './config_tuya_catalog';
import tuyaLedListComponent from './config_tuya_led_list';
import { migrateCryptoList } from './crypto_migrate';

import Clay from 'pebble-clay';
import clayConfig from './config_clay';
import clayConfigCustom from './config_clay_custom';
import widgetListComponent from './config_widget_list';
import { widgetListToPayload } from './widget_list_payload';
import { slotsToList, splitListByPosition } from './widget_slots';
import { isWatchPollRequest, isColdPollRequest } from './poll_request';
import { toAppMessageValue } from './app_message_value';
import { STRAIGHT_THROUGH_KEYS, COLOR_KEYS } from './config_send_keys';

const clay = new Clay(clayConfig, clayConfigCustom, { autoHandleEvents: false });
clay.registerComponent(widgetListComponent);
clay.registerComponent(cryptoListComponent);
clay.registerComponent(currencyListComponent);
clay.registerComponent(tuyaCatalogComponent);
clay.registerComponent(tuyaListComponent);
clay.registerComponent(tuyaLedListComponent);

// Listen for when the watchface is opened.
//
// NOTE: this fires on EVERY watchface relaunch -- PebbleOS runs one app at a time, so
// opening any watchapp kills the watchface and exiting it starts this JS again. It
// therefore must NOT fetch: the watch persists every source's data and restores it at
// launch, so a relaunch needs no phone traffic at all. When the watch's persist really
// is empty (reinstall / wipe), the watch itself asks for a forced round via a COLD
// poll request -- see poll_request.ts and main.c's cold_scan_cb.
Pebble.addEventListener('ready', () => {
  console.log('JS component is now READY');

  // Seed the per-source enable flags on first ever run; webviewclosed re-derives them
  // from the placed widget ids after that.
  if (window.localStorage.getItem('disable_weather') === null) {
    window.localStorage.setItem('disable_weather', 'no');
  }
  if (window.localStorage.getItem('disable_electricity') === null) {
    window.localStorage.setItem('disable_electricity', 'yes');
  }
  if (window.localStorage.getItem('disable_crypto') === null) {
    window.localStorage.setItem('disable_crypto', 'yes');
  }
  if (window.localStorage.getItem('disable_currency') === null) {
    window.localStorage.setItem('disable_currency', 'yes');
  }
  if (window.localStorage.getItem('disable_tuya') === null) {
    window.localStorage.setItem('disable_tuya', 'yes');
  }
});

// Listen for incoming messages
// when one is received, we treat it as the watch's request for fresh phone data
// (weather + electricity + crypto) -- this is the single shared, watch-driven poll.
Pebble.addEventListener('appmessage', (msg) => {
  console.log('Recieved message: ' + JSON.stringify(msg.payload));

  // Companion Android apps (trackworktime, MIDI Recorder) push their status to
  // THIS watchface's UUID, so those messages are delivered here too. Only the
  // watch's own (empty) data-poll request should trigger a phone fetch -- else a
  // tracking toggle would hit the rate-limited crypto API and blank the widgets.
  if (!isWatchPollRequest(msg.payload)) {
    return;
  }

  // A cold request means the watch has no data for a placed widget (its persist was
  // wiped); it bypasses every per-source throttle AND the "unchanged" suppression,
  // which is the only way to repopulate a wiped watch from the phone's cache.
  const cold = isColdPollRequest(msg.payload);

  // NOTE: do NOT force disable_weather = 'no' here. It used to be set on every poll, which
  // permanently overwrote the value webviewclosed derives from the placed widget ids -- so
  // a watchface with no weather widget still fetched weather and sent a 6-7 key message
  // every single poll. The weather module already self-heals an empty/garbage flag.
  weather.updateWeather(cold);
  electricity.updateElectricity(cold);
  crypto.updateCrypto(cold);
  currency.updateCurrency(cold);
  tuya.updateTuya(cold);
});

// One-time migration: the old config saved 6 separate SettingWidget*ID keys to
// Clay's `clay-settings` store; the new single `widgetList` component reads
// `WidgetList`. Without this, opening the new config shows the default 3 widgets
// (ignoring the user's real slots) and saving would clobber slots 4-6. Seed
// `WidgetList` from the legacy slots once, before Clay bakes settings into the URL.
function migrateWidgetListSettings() {
  let stored: Record<string, any>;
  try {
    stored = JSON.parse(window.localStorage.getItem('clay-settings') || '{}') || {};
  } catch (e) {
    return;
  }
  // Step 1: legacy 6 SettingWidget*ID slots -> single WidgetList (if not present).
  if (stored.WidgetList === undefined) {
    const list = slotsToList(stored);
    if (list) { stored.WidgetList = list; }
  }
  // Step 2: single WidgetList -> left/right per legacy SettingSidebarOnLeft, once.
  if (stored.WidgetListRight === undefined) {
    const single = Array.isArray(stored.WidgetList) ? stored.WidgetList : [];
    const split = splitListByPosition(single, stored.SettingSidebarOnLeft);
    stored.WidgetList = split.left;
    stored.WidgetListRight = split.right;
  }
  window.localStorage.setItem('clay-settings', JSON.stringify(stored));
}

// Bake the discovered Tuya catalog + a default (empty) sensor list into clay-settings
// so the config page's custom components can read them (the config webview has no
// localStorage). Mirrors PebbleTuyaControl's seedConfigData.
function seedTuyaCatalog() {
  let stored: Record<string, any>;
  try { stored = JSON.parse(window.localStorage.getItem('clay-settings') || '{}') || {}; }
  catch (e) { return; }
  stored.TuyaCatalog = window.localStorage.getItem('tuya-catalog') || '{"v":1,"devices":[]}';
  if (stored.TuyaList === undefined) { stored.TuyaList = []; }
  if (stored.TuyaLedList === undefined) { stored.TuyaLedList = []; }
  window.localStorage.setItem('clay-settings', JSON.stringify(stored));
}

Pebble.addEventListener('showConfiguration', () => {
  migrateWidgetListSettings();
  migrateCryptoList();
  let opened = false;
  const openConfig = () => {
    if (opened) { return; }
    opened = true;
    seedTuyaCatalog();
    Pebble.openURL(clay.generateUrl());
  };
  if (tuya.hasCachedCatalog()) {
    // Devices were discovered before: open instantly from the cached catalog and
    // refresh it in the background for the next open. A newly-added Tuya device
    // appears one config-open later — worth it to avoid a multi-second wait every
    // time. seedTuyaCatalog() (in openConfig) already ran, so the background
    // saveCatalog only affects the next open.
    openConfig();
    tuya.discoverCatalog(() => {});
  } else {
    // First open with credentials (no cache yet): block on discovery so the sensor
    // list is populated, with a fallback so a hung Tuya request never wedges the
    // config page. No creds -> discoverCatalog calls back immediately (opens fast).
    const fallback = setTimeout(openConfig, 12000);
    tuya.discoverCatalog(() => { clearTimeout(fallback); openConfig(); });
  }
});

Pebble.addEventListener('webviewclosed', (e) => {
  if (!e || !e.response) { console.log('No settings changed!'); return; }

  // getSettings(response, false) returns the raw parsed config keyed by
  // messageKey, with each value wrapped as { value: X[, precision] } (Clay's
  // serialize() shape). Flatten those wrappers into `s` so each `s[k]` is the
  // bare value the rest of this handler expects.
  const raw = clay.getSettings(e.response, false);
  const s: Record<string, any> = {};
  Object.keys(raw).forEach((k) => {
    const v = raw[k];
    s[k] = (v && typeof v === 'object' && 'value' in v) ? v.value : v;
  });

  const dict: Record<string, any> = {};

  // colors: Clay returns a 24-bit RGB decimal int; C decodes via GColorFromHEX
  function colorInt(v: string | number): number {
    return (typeof v === 'string') ? (parseInt(v.replace(/^0x/, ''), 16) & 0xFFFFFF) : (v & 0xFFFFFF);
  }
  COLOR_KEYS.forEach((k) => {
    if (s[k] !== undefined && s[k] !== null) { dict[k] = colorInt(s[k]); }
  });

  // Straight-through settings. Clay returns radiogroup/select/input values as
  // strings (DOM values), so coerce every numeric watch key to int; only genuine
  // string settings (alt-clock name, decimal separator char) stay as-is.
  const STRING_KEYS: Record<string, boolean> = { SettingAltClockName: true, SettingDecimalSep: true };
  STRAIGHT_THROUGH_KEYS.forEach((k) => {
    if (s[k] === undefined || s[k] === null || s[k] === '') { return; }
    dict[k] = toAppMessageValue(s[k], !!STRING_KEYS[k]);
  });
  if (dict.SettingPollIntervalMin === undefined || isNaN(dict.SettingPollIntervalMin)) {
    dict.SettingPollIntervalMin = 30;
  }

  // decimal electricity fields: snt/kWh -> centi (x100)
  if (s.elec_cheap_floor !== undefined && s.elec_cheap_floor !== '') {
    dict.SettingElecCheapFloorCenti = Math.round(parseFloat(s.elec_cheap_floor) * 100);
  }
  if (s.elec_cheap_ceiling !== undefined && s.elec_cheap_ceiling !== '') {
    dict.SettingElecCheapCeilingCenti = Math.round(parseFloat(s.elec_cheap_ceiling) * 100);
  }

  // phone-only: weather location + data source -> localStorage (weather.js reads these)
  if (s.weather_loc_mode === 'manual') {
    window.localStorage.setItem('weather_loc', s.weather_loc || '');
    window.localStorage.setItem('weather_loc_lat', s.weather_loc_lat || '');
    window.localStorage.setItem('weather_loc_lng', s.weather_loc_lng || '');
  } else {
    window.localStorage.removeItem('weather_loc');
    window.localStorage.removeItem('weather_loc_lat');
    window.localStorage.removeItem('weather_loc_lng');
  }
  if (s.weather_datasource) {
    window.localStorage.setItem('weather_datasource', s.weather_datasource);
    window.localStorage.setItem('weather_api_key', '');
  }

  // widget lists -> two byte-array keys the watch draws per column (variable length).
  const widgetPayload = widgetListToPayload(s.WidgetList);
  const widgetPayloadRight = widgetListToPayload(s.WidgetListRight);
  dict.SettingWidgetList = widgetPayload;
  dict.SettingRightWidgetList = widgetPayloadRight;

  // derive disable_* flags from the selected widget IDs across BOTH lists
  const widgetIDs = widgetPayload.concat(widgetPayloadRight);
  window.localStorage.setItem('disable_weather',
    (widgetIDs.indexOf(7) !== -1 || widgetIDs.indexOf(8) !== -1 || widgetIDs.indexOf(13) !== -1) ? 'no' : 'yes');
  window.localStorage.setItem('disable_electricity',
    (widgetIDs.indexOf(14) !== -1 || widgetIDs.indexOf(18) !== -1 || widgetIDs.indexOf(19) !== -1) ? 'no' : 'yes');
  // crypto: enabled iff any placed widget id is in the crypto range (15/16/17 or 200+)
  const anyCrypto = widgetIDs.some((id) =>
    id === 15 || id === 16 || id === 17 || (id >= 200 && id < 216));
  window.localStorage.setItem('disable_crypto', anyCrypto ? 'no' : 'yes');
  // currency: enabled iff any placed widget id is in the currency range [216, 223)
  const anyCurrency = widgetIDs.some((id) => id >= 216 && id < 223);
  window.localStorage.setItem('disable_currency', anyCurrency ? 'no' : 'yes');
  // tuya: enabled iff a placed widget id is in the tuya sensor range [128, 144)
  // or is the LED-row widget (23)
  const anyTuya = widgetIDs.some((id) => (id >= 128 && id < 144) || id === 23);
  window.localStorage.setItem('disable_tuya', anyTuya ? 'no' : 'yes');

  console.log('Preparing message: ' + JSON.stringify(dict));
  Pebble.sendAppMessage(dict, () => {
    weather.updateWeather(true);
    electricity.updateElectricity(true);
    crypto.updateCrypto(true);
    currency.updateCurrency(true);
    tuya.updateTuya(true);
  }, () => { console.log('Failed to send config data!'); });
});

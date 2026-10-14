// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Phone-side Tuya sensor driver. Reads the user's sensor rows (Clay key TuyaList in
   clay-settings), authenticates to the Tuya cloud (creds in TuyaAccessId/Secret/Region),
   and on each shared poll fetches device /status and pushes packed readings to the watch
   under the TuyaData key. Scale/unit come from the catalog discovered at config-open and
   cached in localStorage['tuya-catalog'] (no per-poll /specification call). Throttled far
   longer than crypto since sensor readings change slowly. Pure helpers live in
   tuya_client.ts / tuya_spec.ts / tuya_parse.ts (unit-tested). */

import { createClient } from './tuya_client';
import { parseSpecCodes, parseProperties, mergeCodes, buildCatalog, catalogScaleMap, catalogCodesById } from './tuya_spec';
import {
  normalizeRows, distinctDevices, buildPropertiesPath, packTuyaData,
  countValidValues, parseLastSent, TuyaRow,
} from './tuya_parse';
import {
  normalizeLedRows, ledDeviceIds, packLedStates, countKnownStates, TuyaLedRow,
} from './tuya_leds';

// All six Tuya data centers. Argentina/Latin America (and anything not in Tuya's
// country table) lives in Western America — 'us'; see the config page's tip text.
const REGION_HOST: Record<string, string> = {
  eu: 'https://openapi.tuyaeu.com', us: 'https://openapi.tuyaus.com',
  cn: 'https://openapi.tuyacn.com', in: 'https://openapi.tuyain.com',
  'eu-w': 'https://openapi-weaz.tuyaeu.com', 'us-e': 'https://openapi-ueaz.tuyaus.com',
};

const LAST_SENT_KEY = 'tuya_last_sent';
const LEDS_LAST_SENT_KEY = 'tuya_leds_last_sent';
const LAST_FETCH_KEY = 'tuya_last_fetch';
const CATALOG_KEY = 'tuya-catalog';
const TOKEN_KEY = 'tuya-token';
const MIN_FETCH_INTERVAL_S = 10 * 60;   // sensor readings change slowly

function readSettings(): any {
  try { return JSON.parse(window.localStorage.getItem('clay-settings') || '{}') || {}; }
  catch (e) { return {}; }
}

function readCfg(): any {
  const s = readSettings();
  const id = s.TuyaAccessId, secret = s.TuyaAccessSecret, region = s.TuyaRegion || 'eu';
  if (!id || !secret) { return null; }
  return { clientId: id, secret: secret, host: REGION_HOST[region] || REGION_HOST.eu };
}

function readTuyaRows(): TuyaRow[] {
  return normalizeRows(readSettings().TuyaList);
}

// Switch rows drawn by the Tuya LED-row widget (id 23). Separate Clay list from the
// sensor rows: no label/precision, and their state is sent as a compact string.
function readLedRows(): TuyaLedRow[] {
  return normalizeLedRows(readSettings().TuyaLedList);
}

function loadCatalog(): any {
  try { return JSON.parse(window.localStorage.getItem(CATALOG_KEY) || 'null'); }
  catch (e) { return null; }
}
function saveCatalog(cat: any): void {
  try { window.localStorage.setItem(CATALOG_KEY, JSON.stringify(cat)); } catch (e) {}
}

// True once a previous config-open discovered at least one device. Lets the config
// page open instantly from cache (refreshing in the background) instead of blocking
// on a full re-discovery every time — see index.ts showConfiguration.
export function hasCachedCatalog(): boolean {
  const cat = loadCatalog();
  return !!(cat && cat.devices && cat.devices.length > 0);
}

// XMLHttpRequest -> parsed Tuya envelope (same wrapper as PebbleTuyaControl).
function http(opts: any): Promise<any> {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open(opts.method, opts.url, true);
    const h = opts.headers || {};
    Object.keys(h).forEach((k) => { xhr.setRequestHeader(k, h[k]); });
    xhr.onload = () => { try { resolve(JSON.parse(xhr.responseText)); } catch (e) { reject(new Error('Bad JSON from Tuya')); } };
    xhr.onerror = () => { reject(new Error('Network error')); };
    xhr.timeout = 15000;
    xhr.ontimeout = function () { reject(new Error('Timeout')); };
    xhr.send(opts.body || null);
  });
}

const deps = {
  now: () => Date.now(),
  nonce: () => 'xxxxxxxx'.replace(/x/g, () => Math.floor(Math.random() * 16).toString(16)),
  loadToken: (clientId: string) => {
    try { const o = JSON.parse(window.localStorage.getItem(TOKEN_KEY) || 'null'); return (o && o.clientId === clientId) ? o : null; }
    catch (e) { return null; }
  },
  saveToken: (clientId: string, v: any) => {
    try { window.localStorage.setItem(TOKEN_KEY, JSON.stringify({ clientId: clientId, token: v.token, expiresAt: v.expiresAt })); }
    catch (e) {}
  },
};

let _client: any = null, _clientKey: string | null = null;
function getClient(): any {
  const cfg = readCfg();
  if (!cfg) { return null; }
  const key = cfg.clientId + '|' + cfg.host;
  if (!_client || _clientKey !== key) { _client = createClient(cfg, http, deps); _clientKey = key; }
  return _client;
}

// Config-open: list devices + fetch each /specification, build the catalog, cache it,
// then run cb (best-effort: any failure / no creds still calls cb so config opens).
export function discoverCatalog(cb: () => void): void {
  const c = getClient();
  if (!c) { cb(); return; }
  // Last-known per-device codes (scale/unit): fall back to these when a device's
  // /specification fetch fails, so a transient error doesn't degrade good scales.
  const prevByDevice = catalogCodesById(loadCatalog());
  let devices: any[] = [];
  const codesById: Record<string, any> = {};
  c.request('GET', '/v1.0/iot-01/associated-users/devices').then((resp: any) => {
    devices = (resp.result && resp.result.devices) || [];
    let chain: Promise<any> = Promise.resolve();
    devices.forEach((d: any) => {
      chain = chain.then(() => {
        // /specification gives scale+unit (standard codes only); shadow/properties gives
        // the FULL code list + current values (incl. custom DPs). Merge so every reported
        // datapoint is selectable, with a sample value shown in the config dropdown.
        let specCodes: any[] = [];
        return c.request('GET', '/v1.0/iot-03/devices/' + d.id + '/specification')
          .then((spec: any) => { specCodes = parseSpecCodes(spec.result); })
          .catch(() => { specCodes = []; })
          .then(() => c.request('GET', buildPropertiesPath(d.id)))
          .then((props: any) => { codesById[d.id] = mergeCodes(specCodes, parseProperties(props.result), prevByDevice[d.id]); })
          .catch(() => { codesById[d.id] = specCodes.length ? specCodes : (prevByDevice[d.id] || []); });
      });
    });
    return chain;
  }).then(() => {
    // Don't let a degraded result (device list came back empty — lapsed app-account
    // link, transient token/region error) wipe a good cached catalog: the poll's
    // scale map and the config picker both depend on it. Keep the old cache instead.
    const fresh = buildCatalog(devices, codesById);
    const old = loadCatalog();
    if (fresh.devices.length === 0 && old && Array.isArray(old.devices) && old.devices.length > 0) {
      console.log('tuya: discovery returned no devices, keeping cached catalog');
    } else {
      saveCatalog(fresh);
    }
    cb();
  }).catch(() => { cb(); });
}

export function updateTuya(forceUpdate?: boolean): void {
  if (window.localStorage.getItem('disable_tuya') === 'yes') { return; }
  const rows = readTuyaRows();
  const ledRows = readLedRows();
  if (rows.length === 0 && ledRows.length === 0) { return; }

  const last = parseInt(window.localStorage.getItem(LAST_FETCH_KEY) || '0', 10);
  const now = Math.floor(Date.now() / 1000);
  if (!forceUpdate && (now - last) < MIN_FETCH_INTERVAL_S) {
    console.log('tuya: skipping fetch, last was ' + (now - last) + 's ago');
    return;
  }
  window.localStorage.setItem(LAST_FETCH_KEY, String(now));

  const c = getClient();
  if (!c) { console.log('tuya: no credentials'); return; }
  const scaleMap = catalogScaleMap(loadCatalog());
  const prev = parseLastSent(window.localStorage.getItem(LAST_SENT_KEY) || '');
  // Union of the sensor devices and the LED devices: a device used by both is
  // fetched exactly once per poll.
  const devices = distinctDevices(rows);
  const ledDevices = ledDeviceIds(ledRows);
  for (let i = 0; i < ledDevices.length; i++) {
    if (devices.indexOf(ledDevices[i]) === -1) { devices.push(ledDevices[i]); }
  }
  const valuesById: Record<string, any> = {};

  let chain: Promise<any> = Promise.resolve();
  devices.forEach((id) => {
    chain = chain.then(() =>
      c.request('GET', buildPropertiesPath(id)).then((stat: any) => {
        const arr = (stat && stat.result && stat.result.properties) || [];
        const m: Record<string, any> = {};
        for (let i = 0; i < arr.length; i++) { m[arr[i].code] = arr[i].value; }
        valuesById[id] = m;
      }).catch(() => { /* leave this device out -> prev/-- fallback */ }));
  });
  chain.then(() => {
    const dict: Record<string, string> = {};

    // Sensors: unchanged semantics -- skip entirely when nothing valid came back.
    if (rows.length > 0) {
      if (countValidValues(rows, valuesById) === 0) {
        console.log('tuya: no valid readings, keeping last');
      } else {
        const packed = packTuyaData(rows, valuesById, scaleMap, prev);
        if (forceUpdate || window.localStorage.getItem(LAST_SENT_KEY) !== packed) {
          dict.TuyaData = packed;
        } else {
          console.log('tuya: sensors unchanged');
        }
      }
    }

    // LEDs: a PARTIAL answer IS sent (missing rows show as '?'), but an answer with
    // no known state at all (network down / every device offline) is suppressed so
    // the watch keeps its persisted last-good states.
    let ledPacked: string | null = null;
    if (ledRows.length > 0) {
      if (countKnownStates(ledRows, valuesById) === 0) {
        console.log('tuya: no known led states, keeping last');
      } else {
        const packedLeds = packLedStates(ledRows, valuesById);
        if (forceUpdate || window.localStorage.getItem(LEDS_LAST_SENT_KEY) !== packedLeds) {
          dict.TuyaLeds = packedLeds;
          ledPacked = packedLeds;
        } else {
          console.log('tuya: leds unchanged');
        }
      }
    }

    if (Object.keys(dict).length === 0) { return; }
    Pebble.sendAppMessage(dict,
      () => {
        if (dict.TuyaData !== undefined) { window.localStorage.setItem(LAST_SENT_KEY, dict.TuyaData); }
        if (ledPacked !== null) { window.localStorage.setItem(LEDS_LAST_SENT_KEY, ledPacked); }
        console.log('tuya: sent ' + JSON.stringify(dict));
      },
      () => { console.log('tuya: failed to send to Pebble'); });
  });
}

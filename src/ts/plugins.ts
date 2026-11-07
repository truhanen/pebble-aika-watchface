// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Plugin-sourced widgets (ids 144..159): numeric properties of phone-side plugins
   (Pebble SDK 4.44+ Pebble.subscribeToSource), e.g. the built-in weather plugin.
   Selections come from localStorage `plugin_selections`, a JSON array whose index n
   feeds widget id 144+n (max 16); the config page that writes it comes later. For
   emulator testing set it by hand from the pkjs context:
     localStorage.setItem('plugin_selections', JSON.stringify([
       {plugin:'00000000-0000-0000-0001-000000000002', category:'weather',
        item:'location', instanceId:'1', property:'temperature'}]));
     localStorage.setItem('disable_plugins', 'no');
   One one-shot subscription per (plugin, category, item) group; the combined result
   is sent as one PluginData string, only when it changed (or on a cold request).
   Pure logic lives in plugins_parse.ts (unit-tested). */

import { parseSelections, groupSelections, resultFor, noData, packPluginData,
  buildPluginMessage, allNoData, PluginResult, SelectionGroup, PluginSelection } from './plugins_parse';

const LAST_SENT_KEY = 'plugins_last_sent';    // last PluginData string we pushed
const LAST_FETCH_KEY = 'plugins_last_fetch';  // unix-seconds of the last fetch attempt
// Plugin data is local to the phone, but each subscription can wake a plugin that hits
// the network; coalesce bursts of watch polls.
const MIN_FETCH_INTERVAL_S = 60;
const GROUP_TIMEOUT_MS = 15000;               // a plugin that never answers reads "--"

export function readSelections(): (PluginSelection | null)[] {
  return parseSelections(window.localStorage.getItem('plugin_selections'));
}

function fetchGroup(g: SelectionGroup, sels: (PluginSelection | null)[],
                    done: (results: PluginResult[]) => void): void {
  let finished = false;
  let sub: PebbleSourceSubscription | undefined;
  let timer: any;
  const finish = (results: PluginResult[]) => {
    if (finished) { return; }
    finished = true;
    clearTimeout(timer);
    if (sub) { try { sub.unsubscribe(); } catch (e) { /* already gone */ } }
    done(results);
  };
  const failAll = () => finish(g.slots.map((slot) => noData(slot)));
  timer = setTimeout(failAll, GROUP_TIMEOUT_MS);
  try {
    sub = Pebble.subscribeToSource!({
      plugin: g.plugin, category: g.category, item: g.item, properties: g.properties,
      onData: (env) => {
        finish(g.slots.map((slot) => resultFor(slot, sels[slot] as PluginSelection, env)));
      },
      onError: (err) => { console.log('plugins: source error ' + JSON.stringify(err)); failAll(); },
    });
    if (finished) { try { sub.unsubscribe(); } catch (e) { /* ignore */ } }   // onData ran synchronously
  } catch (e) {
    console.log('plugins: subscribe failed ' + e);
    failAll();
  }
}

export function updatePlugins(forceUpdate?: boolean): void {
  if (window.localStorage.getItem('disable_plugins') !== 'no') { return; }
  if (typeof Pebble.subscribeToSource !== 'function') { return; }   // old phone app
  const sels = readSelections();
  const groups = groupSelections(sels);
  if (groups.length === 0) { return; }

  const last = parseInt(window.localStorage.getItem(LAST_FETCH_KEY) || '0', 10);
  const now = Math.floor(Date.now() / 1000);
  if (!forceUpdate && (now - last) < MIN_FETCH_INTERVAL_S) {
    console.log('plugins: skipping fetch, last was ' + (now - last) + 's ago');
    return;
  }
  window.localStorage.setItem(LAST_FETCH_KEY, String(now));

  let results: PluginResult[] = [];
  let pending = groups.length;
  groups.forEach((g) => {
    fetchGroup(g, sels, (r) => {
      results = results.concat(r);
      if (--pending > 0) { return; }
      const packed = packPluginData(results);
      // A failed fetch must not hold off the retry for MIN_FETCH_INTERVAL_S.
      if (allNoData(results)) { window.localStorage.removeItem(LAST_FETCH_KEY); }
      if (!forceUpdate && window.localStorage.getItem(LAST_SENT_KEY) === packed) {
        console.log('plugins: nothing changed, not sending');
        return;
      }
      Pebble.sendAppMessage(buildPluginMessage(results), () => {
        window.localStorage.setItem(LAST_SENT_KEY, packed);
        console.log('plugins: sent ' + packed);
      }, () => { console.log('plugins: failed to send to Pebble'); });
    });
  });
}

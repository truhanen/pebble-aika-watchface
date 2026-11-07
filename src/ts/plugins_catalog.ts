// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* The plugin catalogue shown on the config page: what Pebble.enumeratePlugins() offers,
   reduced to what Aika can display and kept small, because it is baked into the Clay
   URL (already ~336 KB). Pure (no Pebble globals) so it is unit-tested with `node --test`
   (tests/plugins_catalog.test.js); index.ts feeds it the live enumeratePlugins() result.

   Shape (short keys on purpose):
     { v: 1, c: [ { c: category, p: [ { u: pluginUuid, n: pluginName, i: [ item... ] } ] } ] }
     item = { i: itemId, m: 1 (supports multiple instances; omitted otherwise),
              s: [ [propertyId, 'n' | 'i' | 'ni'] ... ],
              n: [ { i: instanceId, n: name ('' = unnamed) } ... ] (multi-instance items only) }
   Shape letters: n = numericValue (selectable now), i = icon/image (recorded for the
   icon widgets of a later phase). Everything else (shortText, series.*, ...) is skipped. */

export interface CatInstance { i: string; n: string; }
export interface CatItem {
  i: string;
  m?: 1;
  s: [string, string][];
  n?: CatInstance[];
}
export interface CatPlugin { u: string; n: string; i: CatItem[]; }
export interface CatCategory { c: string; p: CatPlugin[]; }
export interface PluginCatalog { v: 1; c: CatCategory[]; }

const INSTANCE_NAME_MAX = 24;   // keep the injected payload small

function shapeLetters(shapes: any): string {
  let n = false, i = false;
  if (Array.isArray(shapes)) {
    for (let k = 0; k < shapes.length; k++) {
      if (shapes[k] === 'numericValue') { n = true; }
      else if (shapes[k] === 'icon' || shapes[k] === 'image') { i = true; }
    }
  }
  return (n ? 'n' : '') + (i ? 'i' : '');
}

// enumeratePlugins() result -> compact catalogue. Items without any supported property
// are dropped; so are empty plugins and categories.
export function buildCatalogue(enumerated: any): PluginCatalog {
  const cat: PluginCatalog = { v: 1, c: [] };
  const plugins: any[] = Array.isArray(enumerated) ? enumerated : [];
  for (let a = 0; a < plugins.length; a++) {
    const pl = plugins[a];
    if (!pl || typeof pl.uuid !== 'string' || !Array.isArray(pl.sources)) { continue; }
    for (let b = 0; b < pl.sources.length; b++) {
      const src = pl.sources[b];
      if (!src || typeof src.category !== 'string' || !Array.isArray(src.items)) { continue; }
      const props: [string, string][] = [];
      const declared = src.properties && typeof src.properties === 'object' ? src.properties : {};
      Object.keys(declared).forEach((name) => {
        const letters = shapeLetters(declared[name]);
        if (letters !== '') { props.push([name, letters]); }
      });
      if (props.length === 0) { continue; }
      const items: CatItem[] = [];
      for (let k = 0; k < src.items.length; k++) {
        if (typeof src.items[k] !== 'string') { continue; }
        const it: CatItem = { i: src.items[k], s: props };
        if (src.supportsMultiple) { it.m = 1; }
        items.push(it);
      }
      if (items.length === 0) { continue; }
      let c: CatCategory | undefined;
      for (let k = 0; k < cat.c.length; k++) { if (cat.c[k].c === src.category) { c = cat.c[k]; } }
      if (!c) { c = { c: src.category, p: [] }; cat.c.push(c); }
      let p: CatPlugin | undefined;
      for (let k = 0; k < c.p.length; k++) { if (c.p[k].u === pl.uuid) { p = c.p[k]; } }
      if (!p) { p = { u: pl.uuid, n: typeof pl.name === 'string' ? pl.name : '', i: [] }; c.p.push(p); }
      p.i = p.i.concat(items);
    }
  }
  return cat;
}

// Every multi-instance item of the catalogue, as the subscription request that lists its
// instances.
export function multiInstanceItems(cat: PluginCatalog): { plugin: string; category: string; item: CatItem }[] {
  const out: { plugin: string; category: string; item: CatItem }[] = [];
  cat.c.forEach((c) => c.p.forEach((p) => p.i.forEach((it) => {
    if (it.m) { out.push({ plugin: p.u, category: c.c, item: it }); }
  })));
  return out;
}

// instances of an envelope -> [{i, n}] (name = properties.name.shortText.text, '' if none).
export function instancesFromEnvelope(env: any): CatInstance[] {
  const list: any[] = (env && Array.isArray(env.instances)) ? env.instances : [];
  const out: CatInstance[] = [];
  for (let k = 0; k < list.length; k++) {
    const inst = list[k];
    if (!inst || (typeof inst.instanceId !== 'string' && typeof inst.instanceId !== 'number')) { continue; }
    const nameProp = inst.properties ? inst.properties.name : undefined;
    const text = (nameProp && nameProp.shortText && typeof nameProp.shortText.text === 'string')
      ? nameProp.shortText.text : '';
    out.push({ i: String(inst.instanceId), n: text.substring(0, INSTANCE_NAME_MAX) });
  }
  return out;
}

export type SubscribeFn = (req: any) => { unsubscribe(): void };

// One-shot subscription per multi-instance item to learn its instances (ids + names),
// all in parallel and capped at `timeoutMs` overall so opening the config page never
// waits long. Items that did not answer keep no instance list (the page then offers
// "first instance" only). `done` is called exactly once, with the same catalogue object.
export function gatherInstances(cat: PluginCatalog, subscribe: SubscribeFn | undefined,
                                timeoutMs: number, done: (cat: PluginCatalog) => void,
                                setTimer?: (fn: () => void, ms: number) => any,
                                clearTimer?: (t: any) => void): void {
  const multi = multiInstanceItems(cat);
  if (multi.length === 0 || typeof subscribe !== 'function') { done(cat); return; }
  const st = setTimer || ((fn: () => void, ms: number) => setTimeout(fn, ms));
  const ct = clearTimer || ((t: any) => clearTimeout(t));
  let pending = multi.length;
  let finished = false;
  const subs: ({ unsubscribe(): void } | undefined)[] = [];
  const unsubAll = () => {
    subs.forEach((s) => { if (s) { try { s.unsubscribe(); } catch (e) { /* already gone */ } } });
  };
  const timer = st(() => finish(), timeoutMs);
  const finish = () => {
    if (finished) { return; }
    finished = true;
    ct(timer);
    unsubAll();
    done(cat);
  };
  multi.forEach((m, idx) => {
    let answered = false;
    const settle = () => { if (answered) { return; } answered = true; if (--pending === 0) { finish(); } };
    try {
      subs[idx] = subscribe({
        plugin: m.plugin, category: m.category, item: m.item.i, properties: ['name'],
        onData: (env: any) => {
          if (finished || answered) { return; }
          const inst = instancesFromEnvelope(env);
          if (inst.length > 0) { m.item.n = inst; }
          settle();
        },
        onError: () => { settle(); },
      });
      if (finished && subs[idx]) { try { subs[idx]!.unsubscribe(); } catch (e) { /* ignore */ } }   // onData ran synchronously
    } catch (e) { settle(); }
  });
}

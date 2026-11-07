// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Clay custom component "pluginList": the plugin properties to show as widgets. Value =
   an array of up to 16 selections { plugin, category, item, instanceId?, property } or
   null (an empty slot) -- exactly the localStorage `plugin_selections` format read by
   plugins.ts. Row n is widget id 144+n (data-wid). Each row has cascading selects
   Category -> Plugin -> Item -> [Instance] -> Property, fed by the PluginCatalog baked
   into clay-settings by PKJS (plugins_catalog.ts); the Instance select exists only for
   items with several instances and shows the instance NAME when it has one, else its
   number; the stable instanceId is what is stored. Numeric properties are offered
   under their name and icon properties as "name (icon)" (stored with shape 'icon'); a
   property with both is listed twice.

   SLOT STABILITY: removing a row that is not the last one EMPTIES the slot (it stays as
   an "unused slot" row that can be reused) instead of shifting the later rows, so every
   other widget id keeps its meaning; trailing empty rows are dropped. A widget list that
   still references an emptied slot shows "--" on the watch (packPluginData sends "--" for
   slots without a selection) and "PLUGIN #n" in the picker.

   Each row also carries data-name: the widget's picker name (CATEGORY Plugin Item
   Instance Prop, see plugin_names.ts), unique within the list; config_widget_list.ts reads
   .pl-row[data-wid][data-name] live from the DOM. After every change the component fires
   a DOM 'plugin-rows-changed' event on document so the picker refreshes its labels.

   Serialized via toSource() + re-eval'd in the webview -> fully self-contained: no runtime
   imports, no module-scope helpers, no spread/destructuring, native DOM only. */

function pluginListInitialize(this: any, _minified: any, clayConfig: any): void {
  const self = this;
  const root: HTMLElement = self.$element[0];
  const PLUGIN_BASE = 144;
  const MAX_PLUGINS = 16;   // matches PLUGIN_SLOT_COUNT in src/c/plugin_slots.h
  let catalogue: any = { v: 1, c: [] };

  // BEGIN naming (KEEP IN SYNC with src/ts/plugin_names.ts; tests/plugin_names.test.js)
  const NAME_START_LEN = 3;
  const FOLD_MAP: any = {
    'ø': 'o', 'Ø': 'O', 'æ': 'ae', 'Æ': 'AE', 'œ': 'oe', 'Œ': 'OE', 'ß': 'ss',
    'đ': 'd', 'Đ': 'D', 'ł': 'l', 'Ł': 'L', 'ð': 'd', 'Ð': 'D', 'þ': 'th', 'Þ': 'TH', 'ı': 'i',
  };
  function foldAscii(s: any): string {
    let t = (s === undefined || s === null) ? '' : String(s);
    if (typeof t.normalize === 'function') { t = t.normalize('NFD'); }
    let out = '';
    for (let i = 0; i < t.length; i++) {
      const ch = t.charAt(i);
      const m = FOLD_MAP[ch];
      out += (m !== undefined) ? m : ch;
    }
    return out.replace(/[^A-Za-z0-9]/g, '');
  }
  function shortPart(s: any, len: number): string {
    const f = foldAscii(s).substring(0, len);
    return f.charAt(0).toUpperCase() + f.substring(1);
  }
  function shortenName(p: any, len?: number): string {
    const n = (len === undefined || len < 1) ? NAME_START_LEN : len;
    const out: string[] = [foldAscii(p.category).toUpperCase()];
    out.push(shortPart(p.plugin, n), shortPart(p.item, n));
    if (p.instance !== '' && p.instance !== undefined && p.instance !== null) { out.push(shortPart(p.instance, n)); }
    out.push(shortPart(p.property, n));
    return out.filter(function(x) { return x !== ''; }).join(' ');
  }
  function uniqueNames(list: any[]): string[] {
    const lens: number[] = [];
    const names: string[] = [];
    let maxLen = NAME_START_LEN;
    for (let i = 0; i < list.length; i++) {
      const p = list[i];
      lens.push(NAME_START_LEN);
      if (p) {
        const longest = Math.max(foldAscii(p.plugin).length, foldAscii(p.item).length,
          foldAscii(p.instance).length, foldAscii(p.property).length);
        if (longest > maxLen) { maxLen = longest; }
      }
    }
    for (let round = 0; round <= maxLen; round++) {
      let changed = false;
      for (let i = 0; i < list.length; i++) {
        const p = list[i];
        names[i] = p ? shortenName(p, lens[i]) : '';
      }
      for (let i = 0; i < list.length; i++) {
        if (!list[i]) { continue; }
        let dup = false;
        for (let j = 0; j < list.length; j++) { if (j !== i && names[j] === names[i]) { dup = true; break; } }
        if (dup && lens[i] < maxLen) { lens[i] += 1; changed = true; }
      }
      if (!changed) { break; }
    }
    for (let i = 0; i < list.length; i++) {
      const p = list[i];
      names[i] = p ? shortenName(p, lens[i]) : '';
    }
    const seen: any = {};
    for (let i = 0; i < list.length; i++) {
      if (!list[i]) { continue; }
      const base = names[i];
      seen[base] = (seen[base] || 0) + 1;
      if (seen[base] > 1) { names[i] = base + ' ' + seen[base]; }
    }
    return names;
  }
  // END naming

  function escAttr(s: string): string {
    return String(s).replace(/&/g, '&amp;').replace(/"/g, '&quot;')
      .replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }
  function str(v: any): string { return (typeof v === 'string') ? v : (typeof v === 'number' ? String(v) : ''); }

  // ---- catalogue lookups (shape letters: 'n' numeric, 'i' icon bitmap). A property offering
  // both is listed twice; an icon choice is the option value "name|i" (see propValue).
  function itemHasNumeric(it: any): boolean {   // = has any supported property
    const s = it.s || [];
    for (let i = 0; i < s.length; i++) { if (String(s[i][1]) !== '') { return true; } }
    return false;
  }
  function categories(): any[] {
    const out: any[] = [];
    const cs = catalogue.c || [];
    for (let a = 0; a < cs.length; a++) {
      let ok = false;
      for (let b = 0; b < cs[a].p.length && !ok; b++) {
        for (let c = 0; c < cs[a].p[b].i.length; c++) { if (itemHasNumeric(cs[a].p[b].i[c])) { ok = true; break; } }
      }
      if (ok) { out.push(cs[a]); }
    }
    return out;
  }
  function findCategory(cat: string): any {
    const cs = catalogue.c || [];
    for (let i = 0; i < cs.length; i++) { if (cs[i].c === cat) { return cs[i]; } }
    return null;
  }
  function pluginsOf(cat: string): any[] {
    const c = findCategory(cat);
    const out: any[] = [];
    if (!c) { return out; }
    for (let b = 0; b < c.p.length; b++) {
      for (let k = 0; k < c.p[b].i.length; k++) { if (itemHasNumeric(c.p[b].i[k])) { out.push(c.p[b]); break; } }
    }
    return out;
  }
  function findPlugin(cat: string, uuid: string): any {
    const c = findCategory(cat);
    if (!c) { return null; }
    for (let b = 0; b < c.p.length; b++) { if (c.p[b].u === uuid) { return c.p[b]; } }
    return null;
  }
  function itemsOf(cat: string, uuid: string): any[] {
    const p = findPlugin(cat, uuid);
    const out: any[] = [];
    if (!p) { return out; }
    for (let k = 0; k < p.i.length; k++) { if (itemHasNumeric(p.i[k])) { out.push(p.i[k]); } }
    return out;
  }
  function findItem(cat: string, uuid: string, item: string): any {
    const p = findPlugin(cat, uuid);
    if (!p) { return null; }
    for (let k = 0; k < p.i.length; k++) { if (p.i[k].i === item) { return p.i[k]; } }
    return null;
  }
  function propsOf(cat: string, uuid: string, item: string): string[] {
    const it = findItem(cat, uuid, item);
    const out: string[] = [];
    if (!it) { return out; }
    for (let i = 0; i < it.s.length; i++) {
      if (String(it.s[i][1]).indexOf('n') !== -1) { out.push(it.s[i][0]); }
      if (String(it.s[i][1]).indexOf('i') !== -1) { out.push(it.s[i][0] + '|i'); }
    }
    return out;
  }
  // The property select's value for a selection / the selection fields for a select value.
  function propValue(sel: any): string { return sel.property + (sel.shape === 'icon' ? '|i' : ''); }
  function propLabel(v: string): string {
    return v.length > 2 && v.substring(v.length - 2) === '|i' ? v.substring(0, v.length - 2) + ' (icon)' : v;
  }
  function setProp(sel: any, v: string): void {
    if (v.length > 2 && v.substring(v.length - 2) === '|i') { sel.property = v.substring(0, v.length - 2); sel.shape = 'icon'; }
    else { sel.property = v; delete sel.shape; }
  }
  function instancesOf(cat: string, uuid: string, item: string): any[] {
    const it = findItem(cat, uuid, item);
    return (it && it.m && it.n) ? it.n : [];
  }
  function isMulti(cat: string, uuid: string, item: string): boolean {
    const it = findItem(cat, uuid, item);
    return !!(it && it.m);
  }

  // Option label of an instance: its name, else its 1-based number.
  function instanceLabel(inst: any, idx: number): string {
    return (inst && inst.n) ? inst.n : String(idx + 1);
  }
  // The instance part of the widget name for a selection (see plugin_names.ts): name or
  // number; '' when the item does not have several instances.
  function instancePart(sel: any): string {
    if (!isMulti(sel.category, sel.plugin, sel.item) && !sel.instanceId) { return ''; }
    const list = instancesOf(sel.category, sel.plugin, sel.item);
    for (let i = 0; i < list.length; i++) {
      if (list[i].i === sel.instanceId) { return instanceLabel(list[i], i); }
    }
    if (/^\d+$/.test(sel.instanceId || '')) { return sel.instanceId; }
    return '1';
  }

  // ---- rows. A selection is {plugin, category, item, instanceId, property} (strings) or null.
  function emptyRowHtml(slot: number): string {
    return '<div class="pl-row pl-empty" data-wid="' + (PLUGIN_BASE + slot) + '" data-name="">' +
      '<div class="pl-line">' +
        '<select class="pl-cat">' + catOptionsHtml('') + '</select>' +
        '<button type="button" class="pl-del" style="visibility:hidden" title="Remove">&#10005;</button>' +
      '</div><div class="pl-name">Unused slot ' + (slot + 1) + ' (choose a category to use it)</div></div>';
  }
  // Display-only casing of option text: categories ALL-CAPS, the rest first letter upper-cased.
  function capFirst(t: string): string { return t === '' ? t : t.charAt(0).toUpperCase() + t.slice(1); }
  function optionsHtml(values: string[], labels: string[], sel: string, blank: boolean, upper?: boolean): string {
    const disp = function (t: string): string { return upper ? t.toUpperCase() : capFirst(t); };
    let html = blank ? '<option value=""' + (sel === '' ? ' selected' : '') + '>-</option>' : '';
    let found = false;
    for (let i = 0; i < values.length; i++) {
      if (values[i] === sel) { found = true; }
      html += '<option value="' + escAttr(values[i]) + '"' + (values[i] === sel ? ' selected' : '') + '>' + escAttr(disp(labels[i])) + '</option>';
    }
    if (!found && sel !== '') {
      html = '<option value="' + escAttr(sel) + '" selected>' + escAttr(disp(sel)) + ' (unavailable)</option>' + html;
    }
    return html;
  }
  function catOptionsHtml(sel: string): string {
    const cs = categories();
    const v: string[] = [], l: string[] = [];
    for (let i = 0; i < cs.length; i++) { v.push(cs[i].c); l.push(cs[i].c); }
    return optionsHtml(v, l, sel, true, true);
  }
  // Fill every empty level of a selection with the first available choice.
  function fillDefaults(sel: any): any {
    const out: any = { plugin: str(sel.plugin), category: str(sel.category), item: str(sel.item),
      instanceId: str(sel.instanceId), property: str(sel.property) };
    if (sel.shape === 'icon') { out.shape = 'icon'; }
    if (out.category === '') { return null; }
    if (out.plugin === '') { const ps = pluginsOf(out.category); out.plugin = ps.length ? ps[0].u : ''; }
    if (out.item === '') { const its = itemsOf(out.category, out.plugin); out.item = its.length ? its[0].i : ''; }
    if (out.instanceId === '' && isMulti(out.category, out.plugin, out.item)) {
      const insts = instancesOf(out.category, out.plugin, out.item);
      out.instanceId = insts.length ? insts[0].i : '';
    }
    if (out.property === '') { const pr = propsOf(out.category, out.plugin, out.item); setProp(out, pr.length ? pr[0] : ''); }
    return out;
  }
  function selName(sel: any): any {
    const p = findPlugin(sel.category, sel.plugin);
    const pname = p && p.n ? p.n : (sel.plugin || '').replace(/[^A-Za-z0-9]/g, '');
    return { category: sel.category, plugin: pname, item: sel.item, instance: instancePart(sel), property: sel.property };
  }
  function rowHtml(sel: any, slot: number, name: string): string {
    if (!sel) { return emptyRowHtml(slot); }
    const plugs = pluginsOf(sel.category);
    const pv: string[] = [], pl: string[] = [];
    for (let i = 0; i < plugs.length; i++) { pv.push(plugs[i].u); pl.push(plugs[i].n || plugs[i].u); }
    const items = itemsOf(sel.category, sel.plugin);
    const iv: string[] = [], il: string[] = [];
    for (let i = 0; i < items.length; i++) { iv.push(items[i].i); il.push(items[i].i); }
    const insts = instancesOf(sel.category, sel.plugin, sel.item);
    const nv: string[] = [], nl: string[] = [];
    for (let i = 0; i < insts.length; i++) { nv.push(insts[i].i); nl.push(instanceLabel(insts[i], i)); }
    const showInst = insts.length > 0 || sel.instanceId !== '';
    const props = propsOf(sel.category, sel.plugin, sel.item);
    const pLabels: string[] = [];
    for (let i = 0; i < props.length; i++) { pLabels.push(propLabel(props[i])); }
    return '<div class="pl-row" data-wid="' + (PLUGIN_BASE + slot) + '" data-name="' + escAttr(name) + '"' +
      (sel.shape === 'icon' ? ' data-shape="i"' : '') + '>' +
      '<div class="pl-line">' +
        '<select class="pl-cat" title="Category">' + catOptionsHtml(sel.category) + '</select>' +
        '<select class="pl-plugin" title="Plugin">' + optionsHtml(pv, pl, sel.plugin, false) + '</select>' +
        '<select class="pl-item" title="Item">' + optionsHtml(iv, il, sel.item, false) + '</select>' +
        '<select class="pl-inst" title="Instance"' + (showInst ? '' : ' style="display:none"') + '>' +
          optionsHtml(nv, nl, sel.instanceId, false) + '</select>' +
        '<select class="pl-prop" title="Property">' + optionsHtml(props, pLabels, propValue(sel), false) + '</select>' +
        '<button type="button" class="pl-del" title="Remove">&#10005;</button>' +
      '</div><div class="pl-name">Widget: ' + escAttr(name) + '</div></div>';
  }

  // The rows as selections (null = empty slot), read from the DOM.
  function currentRows(): any[] {
    const rows: any[] = [];
    const rowEls = root.querySelectorAll('.pl-row');
    for (let i = 0; i < rowEls.length; i++) {
      const el = rowEls[i] as HTMLElement;
      const catSel = el.querySelector('.pl-cat') as HTMLSelectElement;
      const category = catSel ? catSel.value : '';
      if (category === '') { rows.push(null); continue; }
      const pl = el.querySelector('.pl-plugin') as HTMLSelectElement;
      const it = el.querySelector('.pl-item') as HTMLSelectElement;
      const inst = el.querySelector('.pl-inst') as HTMLSelectElement;
      const pr = el.querySelector('.pl-prop') as HTMLSelectElement;
      const sel: any = { plugin: pl ? pl.value : '', category: category, item: it ? it.value : '',
        property: '' };
      if (pr) { setProp(sel, pr.value); }
      const instVal = (inst && inst.style.display !== 'none') ? inst.value : '';
      if (instVal !== '') { sel.instanceId = instVal; }
      rows.push(sel);
    }
    return rows;
  }

  function trimTrailing(rows: any[]): any[] {
    const out = rows.slice(0, MAX_PLUGINS);
    while (out.length > 0 && !out[out.length - 1]) { out.pop(); }
    return out;
  }
  function updateAddButton(count: number, hasEmpty: boolean): void {
    const add = root.querySelector('.pl-add') as HTMLButtonElement;
    if (add) { add.style.display = (categories().length === 0 || (count >= MAX_PLUGINS && !hasEmpty)) ? 'none' : ''; }
  }
  function updateStatus(): void {
    const el = root.querySelector('.pl-status') as HTMLElement;
    if (!el) { return; }
    el.textContent = (categories().length === 0)
      ? 'No plugins available: this phone app has no plugin support, or no plugin offers numeric values or icons. Update the Pebble app (plugin support needs SDK 4.44+) and reopen this page.'
      : 'Add a plugin property, then place it in one of the widget lists above. Removing a slot in the middle leaves it empty so the other widgets keep their numbers.';
  }
  function notify(): void {
    if (typeof document === 'undefined') { return; }
    const ev = document.createEvent('Event');
    ev.initEvent('plugin-rows-changed', true, true);
    document.dispatchEvent(ev);
  }

  // Re-render all rows from selections: defaults filled in, names made unique.
  function rebuild(rowsIn: any[]): void {
    const rows: any[] = [];
    for (let i = 0; i < rowsIn.length && i < MAX_PLUGINS; i++) {
      rows.push(rowsIn[i] && typeof rowsIn[i] === 'object' ? fillDefaults(rowsIn[i]) : null);
    }
    const parts: any[] = [];
    for (let i = 0; i < rows.length; i++) { parts.push(rows[i] ? selName(rows[i]) : null); }
    const names = uniqueNames(parts);
    let html = '';
    let hasEmpty = false;
    for (let i = 0; i < rows.length; i++) {
      if (!rows[i]) { hasEmpty = true; }
      html += rowHtml(rows[i], i, names[i]);
    }
    (root.querySelector('.pl-list') as HTMLElement).innerHTML = html;
    updateAddButton(rows.length, hasEmpty);
    updateStatus();
    notify();
  }

  self._plCurrentRows = function(): any[] { return trimTrailing(currentRows()); };
  self._plRebuild = rebuild;

  function rowIndexOf(node: Node | null): number {
    const rowEls = root.querySelectorAll('.pl-row');
    for (let i = 0; i < rowEls.length; i++) { if (rowEls[i] === node) { return i; } }
    return -1;
  }

  root.addEventListener('click', function(ev: Event) {
    let target = ev.target as HTMLElement;
    if (!target) { return; }
    if (target.tagName !== 'BUTTON') { target = target.closest ? (target.closest('button') as HTMLElement) : null as any; }
    if (!target) { return; }
    if (target.classList.contains('pl-add')) {
      const rows = currentRows();
      const cs = categories();
      if (cs.length === 0) { return; }
      let slot = -1;
      for (let i = 0; i < rows.length; i++) { if (!rows[i]) { slot = i; break; } }
      if (slot === -1) { if (rows.length >= MAX_PLUGINS) { return; } slot = rows.length; rows.push(null); }
      rows[slot] = { category: cs[0].c, plugin: '', item: '', instanceId: '', property: '' };
      rebuild(rows);
      self.trigger('change');
      return;
    }
    if (target.classList.contains('pl-del')) {
      const rowEl = (target.closest ? target.closest('.pl-row') : null) as HTMLElement;
      const idx = rowIndexOf(rowEl);
      if (idx === -1) { return; }
      const rows = currentRows();
      rows[idx] = null;           // empty the slot: later slots keep their widget ids
      rebuild(trimTrailing(rows));
      self.trigger('change');
    }
  });

  root.addEventListener('change', function(ev: Event) {
    const t = ev.target as HTMLElement;
    if (!t || !t.classList) { return; }
    const rowEl = (t.closest ? t.closest('.pl-row') : null) as HTMLElement;
    const idx = rowIndexOf(rowEl);
    if (idx === -1) { return; }
    const rows = currentRows();
    const sel = rows[idx];
    if (t.classList.contains('pl-cat')) {
      // new category -> everything below falls back to the first choice (or the slot empties)
      rows[idx] = ((t as HTMLSelectElement).value === '') ? null
        : { plugin: '', category: (t as HTMLSelectElement).value, item: '', instanceId: '', property: '' };
    } else if (sel && t.classList.contains('pl-plugin')) {
      sel.item = ''; sel.instanceId = ''; sel.property = ''; delete sel.shape;
    } else if (sel && t.classList.contains('pl-item')) {
      sel.instanceId = ''; sel.property = ''; delete sel.shape;
    }
    rebuild(trimTrailing(rows));
    self.trigger('change');
  });

  // The catalogue's store DOM exists only after the whole page builds; read it, then
  // re-render the rows with real options and names.
  if (clayConfig && clayConfig.on && clayConfig.EVENTS) {
    clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
      const store = clayConfig.getItemByMessageKey('PluginCatalog');
      if (store) {
        const raw = store.get();
        let parsed: any = null;
        try { parsed = (typeof raw === 'string') ? JSON.parse(raw) : raw; } catch (e) { parsed = null; }
        if (parsed && parsed.c) { catalogue = parsed; }
      }
      rebuild(currentRows());
    });
  }
}

const pluginListComponent = {
  name: 'pluginList',
  template:
    '<div class="pl-root">' +
    '<div class="pl-status"></div>' +
    '<div class="pl-list"></div>' +
    '<button type="button" class="pl-add">+ Add plugin widget</button>' +
    '</div>',
  style:
    // The small texts get Clay's own side padding (0.75rem), as this root is not a .component.
    '.pl-status{margin:0 0 8px 0;padding:0 0.75rem;font-size:0.85rem;color:#bbb}' +
    '.pl-row{border-bottom:1px solid #555;padding:6px 0;margin:0 0 6px 0}' +
    '.pl-line{display:flex;flex-wrap:wrap;align-items:center;margin:0}' +
    '.pl-row select{min-width:0;height:2.6rem;margin:0 4px 4px 0;flex:1 1 6rem;' +
      'background-color:#767676;color:#fff;border:none;border-radius:0.3rem;padding:0 0.4rem;color-scheme:dark}' +
    '.pl-row button{flex:0 0 auto;min-width:0;width:2.6rem;height:2.6rem;margin:0 0 4px 0;padding:0}' +
    '.pl-name{padding:0 0.75rem;font-size:0.8rem;color:#bbb}' +
    '.pl-empty .pl-name{font-style:italic}' +
    '.pl-add{margin:8px 0 10px 0}',
  manipulator: {
    get: function(this: any): any[] { return this._plCurrentRows ? this._plCurrentRows() : []; },
    set: function(this: any, value: any) {
      let rows: any[] = [];
      let v: any = value;
      if (v && typeof v === 'object' && !Array.isArray(v) && v.value !== undefined) { v = v.value; }
      if (Array.isArray(v)) { rows = v; }
      else if (typeof v === 'string' && v !== '') { try { const p = JSON.parse(v); if (Array.isArray(p)) { rows = p; } } catch (e) { rows = []; } }
      if (this._plRebuild) { this._plRebuild(rows); }
      return this;
    },
  },
  defaults: { label: '' },
  initialize: pluginListInitialize,
};

export = pluginListComponent;

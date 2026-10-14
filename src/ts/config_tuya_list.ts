// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Clay custom component "tuyaList": the SET of Tuya sensor datapoints to track. Value
   = array of { wid, deviceId, code, p, t, label }. Each row: a device <select> and a
   datapoint <select> (both from the TuyaCatalog baked into clay-settings by PKJS), a
   precision input p, a trim input t, and an optional label (empty -> the code). Each
   row carries a STABLE wid (data-wid) in [128, 144); config_widget_list.ts reads these
   rows live from the DOM to build the sidebar picker's Tuya options.

   Serialized via toSource() + re-eval'd in the webview -> fully self-contained: no
   runtime imports, no module-scope helpers, no spread/destructuring, native DOM only.
   The catalog is read at AFTER_BUILD (its DOM exists only after the page builds). */

function tuyaListInitialize(this: any, _minified: any, clayConfig: any): void {
  const self = this;
  const root: HTMLElement = self.$element[0];
  const TUYA_BASE = 128;
  const MAX_TUYA = 16;   // matches MAX_TUYA in src/c/tuya.h
  let catalogDevices: any[] = [];   // [{id, name, codes:[{code, unit, ...}]}]

  function escAttr(s: string): string {
    return String(s).replace(/&/g, '&amp;').replace(/"/g, '&quot;')
      .replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }
  function deviceById(id: string): any {
    for (let i = 0; i < catalogDevices.length; i++) { if (catalogDevices[i].id === id) { return catalogDevices[i]; } }
    return null;
  }
  // Human-readable sample for the dropdown: apply the datapoint's scale like the
  // watch does, so a temperature reported raw as 128 (scale 1) shows "12.8", not
  // "128". Booleans -> On/Off; non-numeric (strings) pass through.
  function formatSample(value: any, scale: any): string {
    if (typeof value === 'boolean') { return value ? 'On' : 'Off'; }
    if (typeof value !== 'number') { return String(value); }
    const sc = (typeof scale === 'number' && scale > 0) ? scale : 0;
    if (sc === 0) { return String(value); }
    let divisor = 1;
    for (let i = 0; i < sc; i++) { divisor *= 10; }
    return (value / divisor).toFixed(sc);
  }
  function deviceOptionsHtml(selId: string): string {
    let html = '';
    let found = false;
    for (let i = 0; i < catalogDevices.length; i++) {
      const d = catalogDevices[i];
      if (d.id === selId) { found = true; }
      html += '<option value="' + escAttr(d.id) + '"' + (d.id === selId ? ' selected' : '') + '>' + escAttr(d.name) + '</option>';
    }
    if (!found && selId) { html = '<option value="' + escAttr(selId) + '" selected>' + escAttr(selId) + ' (unavailable)</option>' + html; }
    return html;
  }
  function codeOptionsHtml(devId: string, selCode: string): string {
    const d = deviceById(devId);
    const codes = (d && d.codes) ? d.codes : [];
    let html = '';
    let found = false;
    for (let i = 0; i < codes.length; i++) {
      const c = codes[i];
      const unit = (c.unit ? ' ' + c.unit : '');
      // Show the current reading so the right datapoint is obvious (e.g. a soil
      // sensor's real moisture 'humidity1 = 96' vs a stale 'humidity = 0').
      const sample = (c.sample !== undefined && c.sample !== null) ? (' = ' + formatSample(c.sample, c.scale) + unit) : '';
      if (c.code === selCode) { found = true; }
      html += '<option value="' + escAttr(c.code) + '"' + (c.code === selCode ? ' selected' : '') + '>' + escAttr(c.code + sample) + '</option>';
    }
    if (!found && selCode) { html = '<option value="' + escAttr(selCode) + '" selected>' + escAttr(selCode) + '</option>' + html; }
    return html;
  }

  function rowHtml(row: any): string {
    const deviceId = (row && typeof row.deviceId === 'string') ? row.deviceId : '';
    const code = (row && typeof row.code === 'string') ? row.code : '';
    const p = (row && row.p !== undefined && row.p !== null) ? row.p : 0;
    const t = (row && row.t !== undefined && row.t !== null) ? row.t : 0;
    const label = (row && typeof row.label === 'string') ? row.label : '';
    const wid = (row && row.wid !== undefined) ? parseInt(row.wid, 10) : 0;
    return '<div class="tul-row" data-wid="' + wid + '">' +
      '<div class="tul-line">' +
        '<select class="tul-device">' + deviceOptionsHtml(deviceId) + '</select>' +
        '<select class="tul-code">' + codeOptionsHtml(deviceId, code) + '</select>' +
        '<input class="tul-p" type="number" step="1" title="precision (decimals; negative rounds)" value="' + escAttr(String(p)) + '">' +
        '<input class="tul-t" type="number" step="1" min="0" title="trim leading digits" value="' + escAttr(String(t)) + '">' +
        '<button type="button" class="tul-del" title="Remove">&#10005;</button>' +
      '</div>' +
      '<input class="tul-label" type="text" placeholder="' + escAttr(code) + '" maxlength="7" value="' + escAttr(label) + '">' +
      '</div>';
  }

  function currentRows(): any[] {
    const rows: any[] = [];
    const rowEls = root.querySelectorAll('.tul-row');
    for (let i = 0; i < rowEls.length; i++) {
      const el = rowEls[i] as HTMLElement;
      const devSel = el.querySelector('.tul-device') as HTMLSelectElement;
      const codeSel = el.querySelector('.tul-code') as HTMLSelectElement;
      const pIn = el.querySelector('.tul-p') as HTMLInputElement;
      const tIn = el.querySelector('.tul-t') as HTMLInputElement;
      const labelIn = el.querySelector('.tul-label') as HTMLInputElement;
      let p = parseInt(pIn ? pIn.value : '0', 10); if (isNaN(p)) { p = 0; }
      let t = parseInt(tIn ? tIn.value : '0', 10); if (isNaN(t) || t < 0) { t = 0; }
      const wid = parseInt(el.getAttribute('data-wid') || '0', 10) || 0;
      rows.push({ wid: wid, deviceId: devSel ? devSel.value : '', code: codeSel ? codeSel.value : '',
        p: p, t: t, label: labelIn ? labelIn.value : '' });
    }
    return rows;
  }

  function usedWids(): number[] {
    const out: number[] = [];
    const rowEls = root.querySelectorAll('.tul-row');
    for (let i = 0; i < rowEls.length; i++) { out.push(parseInt((rowEls[i] as HTMLElement).getAttribute('data-wid') || '0', 10) || 0); }
    return out;
  }
  function nextFreeWid(): number {
    const used = usedWids();
    for (let w = TUYA_BASE; w < TUYA_BASE + MAX_TUYA; w++) { if (used.indexOf(w) === -1) { return w; } }
    return TUYA_BASE;
  }
  function updateAddButton(): void {
    const rowEls = root.querySelectorAll('.tul-row');
    const add = root.querySelector('.tul-add') as HTMLButtonElement;
    if (add) { add.style.display = (catalogDevices.length === 0 || rowEls.length >= MAX_TUYA) ? 'none' : ''; }
  }
  function updateStatus(): void {
    const el = root.querySelector('.tul-status') as HTMLElement;
    if (!el) { return; }
    el.textContent = (catalogDevices.length === 0)
      ? 'No devices loaded yet — enter your Tuya credentials, then close and reopen this page.'
      : ('Loaded ' + catalogDevices.length + ' device' + (catalogDevices.length === 1 ? '' : 's') + '.');
  }
  function rebuild(rows: any[]): void {
    const list = root.querySelector('.tul-list') as HTMLElement;
    let html = '';
    for (let i = 0; i < rows.length && i < MAX_TUYA; i++) { html += rowHtml(rows[i]); }
    list.innerHTML = html;
    updateAddButton();
    updateStatus();
  }

  self._tulCurrentRows = currentRows;
  self._tulRebuild = rebuild;

  function rowIndexOf(node: Node | null): number {
    const rowEls = root.querySelectorAll('.tul-row');
    for (let i = 0; i < rowEls.length; i++) { if (rowEls[i] === node) { return i; } }
    return -1;
  }

  root.addEventListener('click', function(ev: Event) {
    let target = ev.target as HTMLElement;
    if (!target) { return; }
    if (target.tagName !== 'BUTTON') { target = target.closest ? (target.closest('button') as HTMLElement) : null as any; }
    if (!target) { return; }
    if (target.classList.contains('tul-add')) {
      const rows = currentRows();
      if (rows.length < MAX_TUYA && catalogDevices.length > 0) {
        const d0 = catalogDevices[0];
        const c0 = (d0.codes && d0.codes[0]) ? d0.codes[0].code : '';
        rows.push({ wid: nextFreeWid(), deviceId: d0.id, code: c0, p: 0, t: 0, label: '' });
        rebuild(rows);
        self.trigger('change');
      }
      return;
    }
    if (target.classList.contains('tul-del')) {
      const rowEl = (target.closest ? target.closest('.tul-row') : null) as HTMLElement;
      const idx = rowIndexOf(rowEl);
      if (idx === -1) { return; }
      const rows = currentRows();
      rows.splice(idx, 1);
      rebuild(rows);
      self.trigger('change');
    }
  });

  root.addEventListener('change', function(ev: Event) {
    const t = ev.target as HTMLElement;
    if (!t) { return; }
    // When the device changes, repopulate that row's code <select> from the catalog.
    if (t.classList.contains('tul-device')) {
      const rowEl = (t.closest ? t.closest('.tul-row') : null) as HTMLElement;
      if (rowEl) {
        const codeSel = rowEl.querySelector('.tul-code') as HTMLSelectElement;
        if (codeSel) { codeSel.innerHTML = codeOptionsHtml((t as HTMLSelectElement).value, ''); }
      }
    }
    self.trigger('change');
  });

  // Read the catalog (its store DOM exists only after the whole page builds) and
  // re-render rows with real device/code options.
  if (clayConfig && clayConfig.on && clayConfig.EVENTS) {
    clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
      const store = clayConfig.getItemByMessageKey('TuyaCatalog');
      if (store) {
        const raw = store.get();
        let parsed: any = null;
        try { parsed = (typeof raw === 'string') ? JSON.parse(raw) : raw; } catch (e) { parsed = null; }
        if (parsed && parsed.devices) { catalogDevices = parsed.devices; }
      }
      rebuild(currentRows());
    });
  }
}

const tuyaListComponent = {
  name: 'tuyaList',
  template:
    '<div class="tul-root">' +
    '<div class="tul-status"></div>' +
    '<div class="tul-list"></div>' +
    '<button type="button" class="tul-add">+ Add sensor</button>' +
    '</div>',
  style:
    '.tul-status{margin:0 0 8px 0;font-size:0.85rem;color:#bbb}' +
    '.tul-row{display:flex;flex-wrap:wrap;align-items:center;border-bottom:1px solid #555;padding:6px 0;margin:0 0 6px 0}' +
    '.tul-line{flex:1 1 100%;display:flex;flex-wrap:wrap;align-items:center;margin:0}' +
    '.tul-row select,.tul-row input{min-width:0;height:2.6rem;margin:0 4px 0 0;' +
      'background-color:#767676;color:#fff;border:none;border-radius:0.3rem;padding:0 0.4rem;color-scheme:dark}' +
    '.tul-row .tul-device{flex:1 1 6rem}' +
    '.tul-row .tul-code{flex:1 1 6rem}' +
    '.tul-row .tul-p{flex:0 0 3rem}' +
    '.tul-row .tul-t{flex:0 0 3rem}' +
    '.tul-row .tul-label{flex:1 1 6rem;margin-top:4px}' +
    '.tul-row button{flex:0 0 auto;min-width:0;width:2.6rem;height:2.6rem;margin:0;padding:0}' +
    '.tul-add{margin:8px 0 10px 0}',
  manipulator: {
    get: function(this: any): any[] { return this._tulCurrentRows ? this._tulCurrentRows() : []; },
    set: function(this: any, value: any) {
      let rows: any[] = [];
      let v: any = value;
      if (v && typeof v === 'object' && !Array.isArray(v) && v.value !== undefined) { v = v.value; }
      if (Array.isArray(v)) { rows = v; }
      else if (typeof v === 'string' && v !== '') { try { const p = JSON.parse(v); if (Array.isArray(p)) { rows = p; } } catch (e) { rows = []; } }
      if (this._tulRebuild) { this._tulRebuild(rows); }
      return this;
    },
  },
  defaults: { label: '' },
  initialize: tuyaListInitialize,
};

export = tuyaListComponent;

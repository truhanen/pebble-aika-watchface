// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Clay custom component "tuyaLedList": the ordered SET of Tuya switches drawn by the
   Tuya LED-row sidebar widget (widget id 23). Value = array of { deviceId, code },
   max 6, order = LED position (left to right, top to bottom). Only BOOLEAN
   datapoints are offered: a code qualifies if the catalog says type 'Boolean' or its
   sampled value is a boolean (manufacturer-custom DPs often carry no spec type).

   Serialized via toSource() + re-eval'd in the webview -> fully self-contained: no
   runtime imports, no module-scope helpers, no spread/destructuring, native DOM only.
   The catalog is read at AFTER_BUILD (its DOM exists only after the page builds). */

function tuyaLedListInitialize(this: any, _minified: any, clayConfig: any): void {
  const self = this;
  const root: HTMLElement = self.$element[0];
  const MAX_LEDS = 6;   // matches MAX_TUYA_LEDS in src/ts/tuya_leds.ts
  let catalogDevices: any[] = [];   // [{id, name, codes:[{code, type, sample, ...}]}]

  function escAttr(s: string): string {
    return String(s).replace(/&/g, '&amp;').replace(/"/g, '&quot;')
      .replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }
  function deviceById(id: string): any {
    for (let i = 0; i < catalogDevices.length; i++) { if (catalogDevices[i].id === id) { return catalogDevices[i]; } }
    return null;
  }
  // A switch datapoint: declared Boolean, or (custom DP with no spec type) whose
  // current reading is a boolean.
  function isSwitchCode(c: any): boolean {
    if (!c || typeof c.code !== 'string') { return false; }
    if (c.type === 'Boolean') { return true; }
    return typeof c.sample === 'boolean';
  }
  function switchCodes(devId: string): any[] {
    const d = deviceById(devId);
    const codes = (d && d.codes) ? d.codes : [];
    const out: any[] = [];
    for (let i = 0; i < codes.length; i++) { if (isSwitchCode(codes[i])) { out.push(codes[i]); } }
    return out;
  }
  function firstSwitchDevice(): any {
    for (let i = 0; i < catalogDevices.length; i++) {
      if (switchCodes(catalogDevices[i].id).length > 0) { return catalogDevices[i]; }
    }
    return null;
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
    const codes = switchCodes(devId);
    let html = '';
    let found = false;
    for (let i = 0; i < codes.length; i++) {
      const c = codes[i];
      // Show the current state so the right switch is obvious.
      const sample = (typeof c.sample === 'boolean') ? (' = ' + (c.sample ? 'On' : 'Off')) : '';
      if (c.code === selCode) { found = true; }
      html += '<option value="' + escAttr(c.code) + '"' + (c.code === selCode ? ' selected' : '') + '>' + escAttr(c.code + sample) + '</option>';
    }
    if (!found && selCode) { html = '<option value="' + escAttr(selCode) + '" selected>' + escAttr(selCode) + '</option>' + html; }
    return html;
  }

  function rowHtml(row: any): string {
    const deviceId = (row && typeof row.deviceId === 'string') ? row.deviceId : '';
    const code = (row && typeof row.code === 'string') ? row.code : '';
    return '<div class="tll-row">' +
      '<select class="tll-device">' + deviceOptionsHtml(deviceId) + '</select>' +
      '<select class="tll-code">' + codeOptionsHtml(deviceId, code) + '</select>' +
      '<button type="button" class="tll-up" title="Move up">&#9650;</button>' +
      '<button type="button" class="tll-down" title="Move down">&#9660;</button>' +
      '<button type="button" class="tll-del" title="Remove">&#10005;</button>' +
      '</div>';
  }

  function currentRows(): any[] {
    const rows: any[] = [];
    const rowEls = root.querySelectorAll('.tll-row');
    for (let i = 0; i < rowEls.length; i++) {
      const el = rowEls[i] as HTMLElement;
      const devSel = el.querySelector('.tll-device') as HTMLSelectElement;
      const codeSel = el.querySelector('.tll-code') as HTMLSelectElement;
      rows.push({ deviceId: devSel ? devSel.value : '', code: codeSel ? codeSel.value : '' });
    }
    return rows;
  }

  function updateAddButton(): void {
    const rowEls = root.querySelectorAll('.tll-row');
    const add = root.querySelector('.tll-add') as HTMLButtonElement;
    if (add) { add.style.display = (firstSwitchDevice() === null || rowEls.length >= MAX_LEDS) ? 'none' : ''; }
  }
  function updateStatus(): void {
    const el = root.querySelector('.tll-status') as HTMLElement;
    if (!el) { return; }
    if (catalogDevices.length === 0) {
      el.textContent = 'No devices loaded yet — enter your Tuya credentials, then close and reopen this page.';
    } else if (firstSwitchDevice() === null) {
      el.textContent = 'None of your Tuya devices report a switch (Boolean) datapoint.';
    } else {
      el.textContent = 'Green = on, red = off, hollow ring = state unknown. Order sets the LED position.';
    }
  }
  function rebuild(rows: any[]): void {
    const list = root.querySelector('.tll-list') as HTMLElement;
    let html = '';
    for (let i = 0; i < rows.length && i < MAX_LEDS; i++) { html += rowHtml(rows[i]); }
    list.innerHTML = html;
    updateAddButton();
    updateStatus();
  }

  self._tllCurrentRows = currentRows;
  self._tllRebuild = rebuild;

  function rowIndexOf(node: Node | null): number {
    const rowEls = root.querySelectorAll('.tll-row');
    for (let i = 0; i < rowEls.length; i++) { if (rowEls[i] === node) { return i; } }
    return -1;
  }

  root.addEventListener('click', function(ev: Event) {
    let target = ev.target as HTMLElement;
    if (!target) { return; }
    if (target.tagName !== 'BUTTON') { target = target.closest ? (target.closest('button') as HTMLElement) : null as any; }
    if (!target) { return; }
    if (target.classList.contains('tll-add')) {
      const addRows = currentRows();
      const d0 = firstSwitchDevice();
      if (addRows.length < MAX_LEDS && d0 !== null) {
        const codes = switchCodes(d0.id);
        addRows.push({ deviceId: d0.id, code: codes[0].code });
        rebuild(addRows);
        self.trigger('change');
      }
      return;
    }
    const rowEl = (target.closest ? target.closest('.tll-row') : null) as HTMLElement;
    const idx = rowIndexOf(rowEl);
    if (idx === -1) { return; }
    const rows = currentRows();
    if (target.classList.contains('tll-del')) {
      rows.splice(idx, 1);
    } else if (target.classList.contains('tll-up')) {
      if (idx === 0) { return; }
      const up = rows[idx - 1]; rows[idx - 1] = rows[idx]; rows[idx] = up;
    } else if (target.classList.contains('tll-down')) {
      if (idx >= rows.length - 1) { return; }
      const down = rows[idx + 1]; rows[idx + 1] = rows[idx]; rows[idx] = down;
    } else {
      return;
    }
    rebuild(rows);
    self.trigger('change');
  });

  root.addEventListener('change', function(ev: Event) {
    const t = ev.target as HTMLElement;
    if (!t) { return; }
    // When the device changes, repopulate that row's switch <select> from the catalog.
    if (t.classList.contains('tll-device')) {
      const rowEl = (t.closest ? t.closest('.tll-row') : null) as HTMLElement;
      if (rowEl) {
        const codeSel = rowEl.querySelector('.tll-code') as HTMLSelectElement;
        if (codeSel) { codeSel.innerHTML = codeOptionsHtml((t as HTMLSelectElement).value, ''); }
      }
    }
    self.trigger('change');
  });

  // Read the catalog (its store DOM exists only after the whole page builds) and
  // re-render rows with real device/switch options.
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

const tuyaLedListComponent = {
  name: 'tuyaLedList',
  template:
    '<div class="tll-root">' +
    '<div class="tll-status"></div>' +
    '<div class="tll-list"></div>' +
    '<button type="button" class="tll-add">+ Add switch</button>' +
    '</div>',
  style:
    '.tll-status{margin:0 0 8px 0;font-size:0.85rem;color:#bbb}' +
    '.tll-row{display:flex;flex-wrap:wrap;align-items:center;border-bottom:1px solid #555;padding:6px 0;margin:0 0 6px 0}' +
    '.tll-row select{min-width:0;height:2.6rem;margin:0 4px 0 0;' +
      'background-color:#767676;color:#fff;border:none;border-radius:0.3rem;padding:0 0.4rem;color-scheme:dark}' +
    '.tll-row .tll-device{flex:1 1 6rem}' +
    '.tll-row .tll-code{flex:1 1 6rem}' +
    '.tll-row button{flex:0 0 auto;min-width:0;width:2.6rem;height:2.6rem;margin:0;padding:0}' +
    '.tll-add{margin:8px 0 10px 0}',
  manipulator: {
    get: function(this: any): any[] { return this._tllCurrentRows ? this._tllCurrentRows() : []; },
    set: function(this: any, value: any) {
      let rows: any[] = [];
      let v: any = value;
      if (v && typeof v === 'object' && !Array.isArray(v) && v.value !== undefined) { v = v.value; }
      if (Array.isArray(v)) { rows = v; }
      else if (typeof v === 'string' && v !== '') { try { const p = JSON.parse(v); if (Array.isArray(p)) { rows = p; } } catch (e) { rows = []; } }
      if (this._tllRebuild) { this._tllRebuild(rows); }
      return this;
    },
  },
  defaults: { label: '' },
  initialize: tuyaLedListInitialize,
};

export = tuyaLedListComponent;

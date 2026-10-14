// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Clay custom component "widgetList": a reorderable list of widget slots. Each row
   is either a plain widget <select>, or a "Rotating" group (main <select> == 255)
   that expands into an interval <select> + a member sub-list. The component value is
   a flat marker-encoded int array: a plain slot is one id; a rotating slot is
   255, count, interval_code, member_1.. member_count (see widget_list_payload.ts +
   src/c/widget_list.c). Clay serializes this object via toSource() and re-evals it in
   the config webview, so EVERY function here MUST be self-contained: no module-scope
   helper, no import at runtime, no TS downlevel helper (__spreadArray/_this), no
   spread/destructuring. Native DOM + native array methods only.
   See docs/superpowers/specs/2026-06-24-timestyle-rotating-widget-design.md */

function widgetListInitialize(this: any, _minified: any, clayConfig: any): void {
  const self = this;
  const root: HTMLElement = self.$element[0];
  const MAX = 16;                 // max top-level rows (matches MAX_WIDGET_LIST bytes-ish)
  const ROTATING = 255;
  const MAX_MEMBERS = 6;
  const DEFAULT_INTERVAL = 3;     // 1 min
  const HIDE_FLAG = 0x20;       // bit set on an id => identifier hidden (see widget_list.h)
  // Toggleable = has an icon or a title label to hide. No-op for Empty(0),
  // Bluetooth(1), Today's Date(4), Seconds(5), and the Rotating head(255).
  function isToggleableId(base: number): boolean {
    return base !== 0 && base !== 1 && base !== 4 && base !== 5 && base !== ROTATING;
  }
  function hideBtnHtml(encoded: number, cls: string): string {
    const base = encoded & 0xdf;
    if (!isToggleableId(base)) { return ''; }
    const hidden = (encoded & HIDE_FLAG) !== 0;
    return '<button type="button" class="' + cls + (hidden ? ' wl-hidden' : '') +
      '" title="Show/hide icon or title">' + (hidden ? '⊘' : '◉') + '</button>';
  }

  // Static widget options. KEEP IN SYNC with src/ts/widget_options.ts STATIC_WIDGETS
  // and src/c/sidebar_widgets.c. Includes Empty(0) + Rotating(255) for the MAIN
  // select; member selects filter those two out (memberOptionsHtml).
  const STATIC_OPTIONS: { id: number; label: string }[] = [
    { id: 0, label: 'Empty' },
    { id: 255, label: '🔁 Rotating' },
    { id: 3, label: 'Alternate Time Zone' },
    { id: 5, label: 'Seconds' },
    { id: 11, label: 'Swatch Internet Time' },
    { id: 4, label: "Today's Date" },
    { id: 6, label: 'Week Number' },
    { id: 7, label: 'Current Weather' },
    { id: 8, label: "Today's Forecast" },
    { id: 13, label: 'UV Index' },
    { id: 14, label: 'Electricity price' },
    { id: 18, label: 'Next cheap electricity' },
    { id: 19, label: 'Cheapest electricity hour' },
    { id: 9, label: 'Sleep' },
    { id: 20, label: 'Deep Sleep' },
    { id: 24, label: 'Sleep + Deep Sleep' },
    { id: 25, label: 'Sleep + Deep Bar' },
    { id: 10, label: 'Steps' },
    { id: 21, label: 'Distance' },
    { id: 12, label: 'Heart Rate' },
    { id: 2, label: 'Battery' },
    { id: 22, label: 'Battery (days left)' },
    { id: 23, label: 'Tuya LEDs' },
  ];

  const INTERVAL_OPTIONS: { code: number; label: string }[] = [
    { code: 0, label: '5 s' },
    { code: 1, label: '10 s' },
    { code: 2, label: '30 s' },
    { code: 3, label: '1 min' },
    { code: 4, label: '2 min' },
    { code: 5, label: '5 min' },
  ];

  function readCryptoRows(): { wid: number; coin: string; label: string }[] {
    const out: { wid: number; coin: string; label: string }[] = [];
    const rowEls = document.querySelectorAll('.cl-root .cl-row');
    for (let i = 0; i < rowEls.length; i++) {
      const el = rowEls[i] as HTMLElement;
      const wid = parseInt(el.getAttribute('data-wid') || '0', 10) || 0;
      const coinSel = el.querySelector('.cl-coin') as HTMLSelectElement;
      let coin = coinSel ? coinSel.value : '';
      if (coin === 'custom') {
        const cust = el.querySelector('.cl-custom') as HTMLInputElement;
        coin = cust ? cust.value : '';
      }
      const labelIn = el.querySelector('.cl-label') as HTMLInputElement;
      out.push({ wid: wid, coin: coin, label: labelIn ? labelIn.value : '' });
    }
    return out;
  }
  function readCurrencyRows(): { wid: number; base: string; quote: string; label: string }[] {
    const out: { wid: number; base: string; quote: string; label: string }[] = [];
    const rowEls = document.querySelectorAll('.cul-root .cul-row');
    for (let i = 0; i < rowEls.length; i++) {
      const el = rowEls[i] as HTMLElement;
      const wid = parseInt(el.getAttribute('data-wid') || '0', 10) || 0;
      const baseSel = el.querySelector('.cul-base') as HTMLSelectElement;
      let base = baseSel ? baseSel.value : '';
      if (base === 'custom') {
        const c = el.querySelector('.cul-base-custom') as HTMLInputElement;
        base = c ? c.value.toUpperCase() : '';
      }
      const quoteSel = el.querySelector('.cul-quote') as HTMLSelectElement;
      let quote = quoteSel ? quoteSel.value : '';
      if (quote === 'custom') {
        const c = el.querySelector('.cul-quote-custom') as HTMLInputElement;
        quote = c ? c.value.toUpperCase() : '';
      }
      const labelIn = el.querySelector('.cul-label') as HTMLInputElement;
      out.push({ wid: wid, base: base, quote: quote, label: labelIn ? labelIn.value : '' });
    }
    return out;
  }
  function readTuyaRows(): { wid: number; deviceId: string; code: string; label: string }[] {
    const out: { wid: number; deviceId: string; code: string; label: string }[] = [];
    const rowEls = document.querySelectorAll('.tul-root .tul-row');
    for (let i = 0; i < rowEls.length; i++) {
      const el = rowEls[i] as HTMLElement;
      const wid = parseInt(el.getAttribute('data-wid') || '0', 10) || 0;
      const devSel = el.querySelector('.tul-device') as HTMLSelectElement;
      const codeSel = el.querySelector('.tul-code') as HTMLSelectElement;
      const labelIn = el.querySelector('.tul-label') as HTMLInputElement;
      out.push({ wid: wid, deviceId: devSel ? devSel.value : '', code: codeSel ? codeSel.value : '',
        label: labelIn ? labelIn.value : '' });
    }
    return out;
  }
  function currentOptions(): { id: number; label: string }[] {
    const out: { id: number; label: string }[] = STATIC_OPTIONS.slice();
    const arr = readCryptoRows();
    for (let i = 0; i < arr.length; i++) {
      const r = arr[i];
      if (!r) { continue; }
      const wid = r.wid;
      if (isNaN(wid) || wid === 0) { continue; }
      const coin = (typeof r.coin === 'string') ? r.coin : '';
      const label = (typeof r.label === 'string' && r.label !== '') ? r.label : coin.toUpperCase();
      out.push({ id: wid, label: label });
    }
    const carr = readCurrencyRows();
    for (let i = 0; i < carr.length; i++) {
      const r = carr[i];
      if (!r) { continue; }
      const cwid = r.wid;
      if (isNaN(cwid) || cwid === 0) { continue; }
      const base = (typeof r.base === 'string') ? r.base : '';
      const quote = (typeof r.quote === 'string') ? r.quote : '';
      const label = (typeof r.label === 'string' && r.label !== '') ? r.label : (base + '/' + quote);
      out.push({ id: cwid, label: label });
    }
    const tarr = readTuyaRows();
    for (let i = 0; i < tarr.length; i++) {
      const r = tarr[i];
      if (!r) { continue; }
      const twid = r.wid;
      if (isNaN(twid) || twid === 0) { continue; }
      const label = (typeof r.label === 'string' && r.label !== '') ? r.label : (r.code || 'Tuya');
      out.push({ id: twid, label: label });
    }
    return out;
  }

  // Options for the MAIN select (includes Empty + Rotating). `selected` preserved
  // even if its option isn't built yet (crypto rows build later).
  function mainOptionsHtml(selected: number): string {
    const opts = currentOptions();
    let html = '';
    let found = false;
    for (let i = 0; i < opts.length; i++) {
      const o = opts[i];
      if (o.id === selected) { found = true; }
      html += '<option value="' + o.id + '"' + (o.id === selected ? ' selected' : '') + '>' + o.label + '</option>';
    }
    if (!found && selected !== 0) {
      html += '<option value="' + selected + '" selected>Crypto #' + selected + '</option>';
    }
    return html;
  }

  // Options for a MEMBER select: drawable widgets only (drop Empty(0) + Rotating(255)).
  function memberOptionsHtml(selected: number): string {
    const opts = currentOptions();
    let html = '';
    let found = false;
    for (let i = 0; i < opts.length; i++) {
      const o = opts[i];
      if (o.id === 0 || o.id === ROTATING) { continue; }
      if (o.id === selected) { found = true; }
      html += '<option value="' + o.id + '"' + (o.id === selected ? ' selected' : '') + '>' + o.label + '</option>';
    }
    if (!found && selected !== 0 && selected !== ROTATING) {
      html += '<option value="' + selected + '" selected>Crypto #' + selected + '</option>';
    }
    return html;
  }

  function firstMemberId(): number {
    const opts = currentOptions();
    for (let i = 0; i < opts.length; i++) {
      if (opts[i].id !== 0 && opts[i].id !== ROTATING) { return opts[i].id; }
    }
    return 2;   // Battery fallback
  }

  function intervalOptionsHtml(selected: number): string {
    let html = '';
    for (let i = 0; i < INTERVAL_OPTIONS.length; i++) {
      const o = INTERVAL_OPTIONS[i];
      html += '<option value="' + o.code + '"' + (o.code === selected ? ' selected' : '') + '>' + o.label + '</option>';
    }
    return html;
  }

  // ---- slot model: {id} (plain) | {rotating:true, interval, members:[]} ----

  function memberHtml(encoded: number): string {
    const base = encoded & 0xdf;
    return '<div class="wl-mem">' +
      '<select class="wl-msel">' + memberOptionsHtml(base) + '</select>' +
      hideBtnHtml(encoded, 'wl-mhide') +
      '<button type="button" class="wl-mdel" title="Remove member">&#10005;</button>' +
      '</div>';
  }

  function groupHtml(interval: number, members: number[]): string {
    let mem = '';
    for (let i = 0; i < members.length; i++) { mem += memberHtml(members[i]); }
    return '<div class="wl-group">' +
      '<div class="wl-introw"><span class="wl-intlbl">Every</span>' +
      '<select class="wl-int">' + intervalOptionsHtml(interval) + '</select></div>' +
      '<div class="wl-mems">' + mem + '</div>' +
      '<button type="button" class="wl-madd">+ add member</button>' +
      '</div>';
  }

  function rowHtml(slot: any): string {
    const isRot = !!slot.rotating;
    const mainSel = isRot ? ROTATING : (parseInt(slot.id, 10) || 0);
    const encoded = (!isRot && slot.hide) ? (mainSel | HIDE_FLAG) : mainSel;
    let html = '<div class="wl-row">' +
      '<div class="wl-main">' +
      '<select class="wl-sel">' + mainOptionsHtml(mainSel) + '</select>' +
      (isRot ? '' : hideBtnHtml(encoded, 'wl-hide')) +
      '<button type="button" class="wl-up" title="Move up">&#9650;</button>' +
      '<button type="button" class="wl-down" title="Move down">&#9660;</button>' +
      '<button type="button" class="wl-del" title="Remove">&#10005;</button>' +
      '</div>';
    if (isRot) {
      const iv = (typeof slot.interval === 'number') ? slot.interval : DEFAULT_INTERVAL;
      const mems = Array.isArray(slot.members) ? slot.members : [];
      html += groupHtml(iv, mems);
    }
    return html + '</div>';
  }

  // Read the DOM rows into slot objects.
  function readSlots(): any[] {
    const slots: any[] = [];
    const rows = root.querySelectorAll('.wl-row');
    for (let i = 0; i < rows.length; i++) {
      const row = rows[i] as HTMLElement;
      const main = row.querySelector('.wl-sel') as HTMLSelectElement;
      const mainVal = main ? (parseInt(main.value, 10) || 0) : 0;
      if (mainVal === ROTATING) {
        const intSel = row.querySelector('.wl-int') as HTMLSelectElement;
        // NOTE: code 0 (5 s) is a VALID interval but falsy in JS, so `parseInt() ||
        // DEFAULT` would silently coerce a 5 s selection to 1 min. Use an explicit
        // NaN check, not `||`.
        let interval = DEFAULT_INTERVAL;
        if (intSel) { const ivCode = parseInt(intSel.value, 10); if (!isNaN(ivCode)) { interval = ivCode; } }
        const members: number[] = [];
        const memEls = row.querySelectorAll('.wl-mems .wl-mem');
        for (let m = 0; m < memEls.length; m++) {
          const mEl = memEls[m] as HTMLElement;
          const msel = mEl.querySelector('.wl-msel') as HTMLSelectElement;
          const base = msel ? (parseInt(msel.value, 10) || 0) : 0;
          const mhb = mEl.querySelector('.wl-mhide') as HTMLButtonElement;
          const mh = !!(mhb && mhb.classList.contains('wl-hidden'));
          members.push(mh ? (base | HIDE_FLAG) : base);
        }
        slots.push({ rotating: true, interval: interval, members: members });
      } else {
        const hb = row.querySelector('.wl-main .wl-hide') as HTMLButtonElement;
        const hidden = !!(hb && hb.classList.contains('wl-hidden'));
        slots.push({ id: mainVal, hide: hidden });
      }
    }
    return slots;
  }

  // slot objects -> flat marker-encoded value array.
  function slotsToValue(slots: any[]): number[] {
    const out: number[] = [];
    for (let i = 0; i < slots.length; i++) {
      const s = slots[i];
      if (s && s.rotating) {
        const mems: number[] = [];
        const arr = Array.isArray(s.members) ? s.members : [];
        for (let m = 0; m < arr.length && mems.length < MAX_MEMBERS; m++) {
          const raw = parseInt(arr[m], 10);
          const base = raw & 0xdf;
          if (!isNaN(raw) && base !== 0 && base !== ROTATING) { mems.push(raw); }
        }
        const iv = (s.interval >= 0 && s.interval <= 5) ? s.interval : DEFAULT_INTERVAL;
        if (mems.length >= 2) {
          out.push(ROTATING, mems.length, iv);
          for (let m = 0; m < mems.length; m++) { out.push(mems[m]); }
        } else if (mems.length === 1) {
          out.push(mems[0]);
        }
      } else if (s) {
        const id = parseInt(s.id, 10) || 0;
        out.push((s.hide && isToggleableId(id & 0xdf)) ? (id | HIDE_FLAG) : id);
      }
    }
    return out;
  }

  // flat marker-encoded value array -> slot objects (for rebuild).
  function valueToSlots(value: number[]): any[] {
    const slots: any[] = [];
    let i = 0;
    while (i < value.length && slots.length < MAX) {
      const head = parseInt(value[i] as any, 10);
      if (head === ROTATING) {
        const count = parseInt(value[i + 1] as any, 10);
        const interval = parseInt(value[i + 2] as any, 10);
        if (isNaN(count) || isNaN(interval)) { break; }
        const members: number[] = [];
        for (let m = 0; m < count && m < MAX_MEMBERS; m++) {
          const id = parseInt(value[i + 3 + m] as any, 10);
          if (!isNaN(id)) { members.push(id); }
        }
        i = i + 3 + count;
        slots.push({ rotating: true,
          interval: (interval >= 0 && interval <= 5) ? interval : DEFAULT_INTERVAL,
          members: members });
      } else {
        const v = isNaN(head) ? 0 : head;
        slots.push({ id: v & 0xdf, hide: (v & HIDE_FLAG) !== 0 });
        i += 1;
      }
    }
    return slots;
  }

  function renderSlots(slots: any[]): void {
    const list = root.querySelector('.wl-list') as HTMLElement;
    let html = '';
    for (let i = 0; i < slots.length && i < MAX; i++) { html += rowHtml(slots[i]); }
    list.innerHTML = html;
    updateButtons();
  }

  function updateButtons(): void {
    const rows = root.querySelectorAll('.wl-row');
    for (let i = 0; i < rows.length; i++) {
      (rows[i].querySelector('.wl-up') as HTMLButtonElement).disabled = (i === 0);
      (rows[i].querySelector('.wl-down') as HTMLButtonElement).disabled = (i === rows.length - 1);
      // Member delete is never disabled: deleting the 2nd-to-last member collapses the
      // whole group to a plain widget (a rotating group with 1 member == a plain widget),
      // so a group in the DOM always has >= 2 members. See the wl-mdel handler.
      const mdels = rows[i].querySelectorAll('.wl-mems .wl-mdel');
      const madd = rows[i].querySelector('.wl-madd') as HTMLButtonElement;
      if (madd) { madd.style.display = (mdels.length >= MAX_MEMBERS) ? 'none' : ''; }
    }
    const add = root.querySelector('.wl-add') as HTMLButtonElement;
    if (add) { add.style.display = (rows.length >= MAX) ? 'none' : ''; }
  }

  // expose for the manipulator (set runs after initialize)
  self._wlGetValue = function(): number[] { return slotsToValue(readSlots()); };
  self._wlRebuild = function(value: number[]): void { renderSlots(valueToSlots(value)); };

  function rowIndexOf(node: Node | null): number {
    const rows = root.querySelectorAll('.wl-row');
    for (let i = 0; i < rows.length; i++) { if (rows[i] === node) { return i; } }
    return -1;
  }
  function topRowOf(el: HTMLElement): HTMLElement | null {
    let n: HTMLElement | null = el;
    while (n && n !== root) { if (n.classList && n.classList.contains('wl-row')) { return n; } n = n.parentNode as HTMLElement; }
    return null;
  }

  root.addEventListener('click', function(ev: Event) {
    let target = ev.target as HTMLElement;
    if (!target) { return; }
    if (target.tagName !== 'BUTTON') {
      target = target.closest ? (target.closest('button') as HTMLElement) : null as any;
    }
    if (!target) { return; }

    if (target.classList.contains('wl-add')) {
      const slots = readSlots();
      if (slots.length < MAX) { slots.push({ id: 0 }); renderSlots(slots); self.trigger('change'); }
      return;
    }

    if (target.classList.contains('wl-hide') || target.classList.contains('wl-mhide')) {
      const nowHidden = !target.classList.contains('wl-hidden');
      if (nowHidden) { target.classList.add('wl-hidden'); } else { target.classList.remove('wl-hidden'); }
      target.innerHTML = nowHidden ? '⊘' : '◉';
      self.trigger('change');
      return;
    }

    const row = topRowOf(target);
    if (!row) { return; }
    const idx = rowIndexOf(row);
    if (idx === -1) { return; }
    const slots = readSlots();

    if (target.classList.contains('wl-del')) {
      slots.splice(idx, 1); renderSlots(slots); self.trigger('change');
    } else if (target.classList.contains('wl-up') && idx > 0) {
      const t = slots[idx]; slots[idx] = slots[idx - 1]; slots[idx - 1] = t;
      renderSlots(slots); self.trigger('change');
    } else if (target.classList.contains('wl-down') && idx < slots.length - 1) {
      const t = slots[idx]; slots[idx] = slots[idx + 1]; slots[idx + 1] = t;
      renderSlots(slots); self.trigger('change');
    } else if (target.classList.contains('wl-madd')) {
      const s = slots[idx];
      if (s && s.rotating && s.members.length < MAX_MEMBERS) {
        s.members.push(firstMemberId()); renderSlots(slots); self.trigger('change');
      }
    } else if (target.classList.contains('wl-mdel')) {
      const s = slots[idx];
      if (s && s.rotating && s.members.length >= 2) {
        // which member?
        const memEl = target.parentNode as HTMLElement;       // .wl-mem
        const memsWrap = memEl.parentNode as HTMLElement;      // .wl-mems
        const mems = memsWrap.querySelectorAll('.wl-mem');
        let mi = -1;
        for (let k = 0; k < mems.length; k++) { if (mems[k] === memEl) { mi = k; break; } }
        if (mi !== -1) {
          s.members.splice(mi, 1);
          // A rotating group with a single member is identical to a plain widget, so
          // collapse the row to a plain slot (mirrors slotsToValue / the C sanitizer)
          // rather than leaving a lone member with a disabled delete button.
          if (s.members.length === 1) {
            const raw = parseInt(s.members[0], 10) || 0;
            slots[idx] = { id: raw & 0xdf, hide: (raw & HIDE_FLAG) !== 0 };
          }
          renderSlots(slots); self.trigger('change');
        }
      }
    }
  });

  root.addEventListener('change', function(ev: Event) {
    const t = ev.target as HTMLElement;
    if (!t || t.tagName !== 'SELECT') { return; }
    if (t.classList.contains('wl-sel')) {
      // main select changed: reconcile rotating <-> plain, seeding a fresh group.
      const slots = readSlots();
      const row = topRowOf(t);
      const idx = row ? rowIndexOf(row) : -1;
      if (idx !== -1) {
        const newVal = parseInt((t as HTMLSelectElement).value, 10) || 0;
        if (newVal === ROTATING) {
          if (!slots[idx].rotating) {
            const a = firstMemberId();
            slots[idx] = { rotating: true, interval: DEFAULT_INTERVAL, members: [a, a] };
          }
        } else {
          slots[idx] = { id: newVal, hide: false };
        }
        renderSlots(slots);
      }
    }
    self.trigger('change');
  });

  // Refresh crypto-coin options when the user opens a select (main or member).
  root.addEventListener('mousedown', function(ev: Event) {
    const t = ev.target as HTMLElement;
    if (!t || t.tagName !== 'SELECT') { return; }
    const sel = t as HTMLSelectElement;
    const cur = parseInt(sel.value, 10) || 0;
    if (sel.classList.contains('wl-sel')) { sel.innerHTML = mainOptionsHtml(cur); }
    else if (sel.classList.contains('wl-msel')) { sel.innerHTML = memberOptionsHtml(cur); }
  });

  // After ALL components build (cryptoList DOM now exists), refresh every select's
  // options with real coin labels, preserving each selection.
  if (clayConfig && clayConfig.on && clayConfig.EVENTS) {
    clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
      const mains = root.querySelectorAll('.wl-row .wl-sel');
      for (let i = 0; i < mains.length; i++) {
        const sel = mains[i] as HTMLSelectElement; sel.innerHTML = mainOptionsHtml(parseInt(sel.value, 10) || 0);
      }
      const mems = root.querySelectorAll('.wl-row .wl-msel');
      for (let i = 0; i < mems.length; i++) {
        const sel = mems[i] as HTMLSelectElement; sel.innerHTML = memberOptionsHtml(parseInt(sel.value, 10) || 0);
      }
    });
  }
}

const widgetListComponent = {
  name: 'widgetList',
  template:
    '<div class="wl-root">' +
    '<div class="wl-list"></div>' +
    '<button type="button" class="wl-add">+ Add widget</button>' +
    '</div>',
  // Clay's base theme styles `button { min-width: 12rem; margin: 0 auto }`. Row /
  // member buttons MUST override min-width (else forced to 12rem, squeezing selects).
  style:
    '.wl-row{display:flex;flex-direction:column;margin:0 0 8px 0}' +
    '.wl-main{display:flex;align-items:center}' +
    '.wl-row .wl-sel{flex:1 1 auto;min-width:0;height:2.8rem;margin:0;' +
      'background-color:#767676;color:#fff;border:none;border-radius:0.3rem;' +
      'padding:0 0.5rem;color-scheme:dark}' +
    '.wl-row .wl-main button,.wl-row .wl-main .wl-hide{flex:0 0 auto;min-width:0;width:2.8rem;height:2.8rem;margin:0 0 0 6px;padding:0}' +
    '.wl-mem .wl-mhide{flex:0 0 auto;min-width:0;width:2.6rem;height:2.6rem;margin:0 0 0 6px;padding:0}' +
    '.wl-row .wl-hidden{opacity:.55}' +
    '.wl-row button[disabled]{opacity:.35}' +
    '.wl-group{margin:6px 0 0 12px;padding:6px 8px;border-left:3px solid #767676}' +
    '.wl-introw{display:flex;align-items:center;margin:0 0 6px 0}' +
    '.wl-intlbl{margin:0 8px 0 0;color:#fff}' +
    '.wl-group .wl-int{flex:1 1 auto;min-width:0;height:2.6rem;background-color:#767676;color:#fff;' +
      'border:none;border-radius:0.3rem;padding:0 0.5rem;color-scheme:dark}' +
    '.wl-mem{display:flex;align-items:center;margin:0 0 6px 0}' +
    '.wl-mem .wl-msel{flex:1 1 auto;min-width:0;height:2.6rem;background-color:#767676;color:#fff;' +
      'border:none;border-radius:0.3rem;padding:0 0.5rem;color-scheme:dark}' +
    '.wl-mem .wl-mdel{flex:0 0 auto;min-width:0;width:2.6rem;height:2.6rem;margin:0 0 0 6px;padding:0}' +
    '.wl-madd{min-width:0;margin:2px 0 4px 0}' +
    '.wl-add{margin:8px 0 10px 0}',
  manipulator: {
    get: function(this: any): number[] {
      return this._wlGetValue ? this._wlGetValue() : [];
    },
    set: function(this: any, value: any) {
      // Inlined normalization (toSource: no module-scope helper). Accepts a bare
      // array, a JSON string, or a {value:X} wrapper; anything else -> [].
      let ids: any[] = [];
      let v: any = value;
      if (v && typeof v === 'object' && !Array.isArray(v) && v.value !== undefined) { v = v.value; }
      if (Array.isArray(v)) {
        ids = v;
      } else if (typeof v === 'string' && v !== '') {
        try { const parsed = JSON.parse(v); if (Array.isArray(parsed)) { ids = parsed; } } catch (e) { ids = []; }
      }
      if (this._wlRebuild) { this._wlRebuild(ids); }
      return this;
    },
  },
  defaults: { label: '' },
  initialize: widgetListInitialize,
};

export = widgetListComponent;

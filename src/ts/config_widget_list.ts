// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Clay custom component "widgetList": a reorderable list of widget slots. Each row
   is either a plain widget (a widget <select> plus an "Aa" font <select>), or a "Rotating"
   group (main <select> == 255) that expands into an interval <select> + a member
   sub-list, each member with its own widget + font selects. The component value is
   an array of slot objects:
     { id, ident, font }                                  plain widget
     { rotating: true, interval, members: [{id, ident, font}, ...] }
   `ident` is 'icon' | 'unit' | 'none': what stands beside the value; the button on the
   row cycles the modes the widget offers (older entries: hide -> 'none').
   `font` is 0..16 (widget_options.ts FONT_OPTIONS) or 31 = the position's default
   font. Older stored values (a flat marker-encoded int array: 255, count, interval,
   members..., hide flag as 0x20 in the id, no font) are still parsed (valueToSlots).
   The optional `max` config option caps the number of top-level rows (no list uses it
   any more; "+ Add widget" is hidden when full). See also
   widget_list_payload.ts + src/c/widget_list.c and docs/widget_contract.md.
   Clay serializes this object via toSource() and re-evals it in
   the config webview, so EVERY function here MUST be self-contained: no module-scope
   helper, no import at runtime, no TS downlevel helper (__spreadArray/_this), no
   spread/destructuring. Native DOM + native array methods only.
   See docs/superpowers/specs/2026-06-24-timestyle-rotating-widget-design.md */

function widgetListInitialize(this: any, _minified: any, clayConfig: any): void {
  const self = this;
  const root: HTMLElement = self.$element[0];
  const cfgMax = parseInt(self.config && self.config.max, 10);
  const MAX = (cfgMax > 0 && cfgMax < 16) ? cfgMax : 16;   // max top-level rows
  const ROTATING = 255;
  const MAX_MEMBERS = 6;
  const DEFAULT_INTERVAL = 3;     // 1 min
  const FONT_DEFAULT = 31;        // "position default" (the watch picks per list)
  // Font the watch uses for FONT_DEFAULT in this list (5 side lists, 7 centre column);
  // only used to label the "Default (...)" option.
  const cfgDefFont = parseInt(self.config && self.config.defaultFont, 10);
  const MAX_FONT = 16;
  const LEGACY_HIDE_FLAG = 0x20;  // old stored values: hide flag OR-ed into the id
  // Identifier modes. KEEP IN SYNC with identModes() in widget_options.ts (the test
  // config_widget_list.test.js compares them for every offered id).
  const IDENT_ICON_IDS: number[] = [2, 7, 8, 9, 10, 12, 20, 21, 22, 23, 24, 25];
  const IDENT_UNIT_IDS: number[] = [2, 3, 6, 7, 8, 9, 10, 12, 13, 14, 18, 19, 20, 21, 22, 24, 25, 35];
  // The modes the button cycles for an id, in order; [] = no identifier (placeholder).
  function identModes(id: number): string[] {
    const slot = (id >= 128 && id <= 143) || (id >= 200 && id <= 222) || (id >= 15 && id <= 17);
    const out: string[] = [];
    if (IDENT_ICON_IDS.indexOf(id) !== -1) { out.push('icon'); }
    if (slot || IDENT_UNIT_IDS.indexOf(id) !== -1) { out.push('unit'); }
    if (out.length) { out.push('none'); }
    return out;
  }
  // A stored ident as shown for this id: itself when offered, else the watch's fallback.
  function resolveIdent(id: number, ident: string): string {
    const modes = identModes(id);
    if (modes.length === 0) { return 'icon'; }
    if (ident === 'none' || modes.indexOf(ident) !== -1) { return ident; }
    return modes[0];
  }
  function normIdent(raw: any): string {
    if (raw && (raw.ident === 'icon' || raw.ident === 'unit' || raw.ident === 'none')) { return raw.ident; }
    return (raw && raw.hide) ? 'none' : 'icon';
  }
  const IDENT_GLYPH: any = { icon: '◉', unit: 'U', none: '⊘' };
  const IDENT_TITLE: any = { icon: 'Identifier: icon', unit: 'Identifier: unit', none: 'Identifier: none' };
  // Has a font setting: text widgets and Empty (the font sets the slot height), not
  // the icon-only widgets nor a rotating group's header (its members have their own).
  function hasFontId(id: number): boolean {
    return !(id >= 36 && id <= 38) && id !== ROTATING;
  }
  function hideBtnHtml(id: number, ident: string, cls: string): string {
    if (identModes(id).length === 0) { return hidePlaceholderHtml(); }
    const m = resolveIdent(id, ident);
    return '<button type="button" class="' + cls + (m === 'none' ? ' wl-hidden' : '') +
      '" data-ident="' + m + '" title="' + IDENT_TITLE[m] + '">' + IDENT_GLYPH[m] + '</button>';
  }

  function fontPlaceholderHtml(): string {
    return '<span class="wl-placeholder wl-fontbox-ph" aria-hidden="true"></span>';
  }

  function hidePlaceholderHtml(): string {
    return '<span class="wl-placeholder wl-hide-ph" aria-hidden="true"></span>';
  }

  function buttonPlaceholderHtml(): string {
    return '<span class="wl-placeholder wl-btn-ph" aria-hidden="true"></span>';
  }

  // Static widget options. KEEP IN SYNC with src/ts/widget_options.ts STATIC_WIDGETS
  // and src/c/widgets.h. Includes Empty(0) + Rotating(255) for the MAIN
  // select; member selects filter those two out (memberOptionsHtml).
  const STATIC_OPTIONS: { id: number; label: string }[] = [
    { id: 0, label: 'EMPTY' },
    { id: 255, label: '🔁 Rotating' },
    { id: 35, label: 'TIME digital' },
    { id: 3, label: 'TIME alt time zone' },
    { id: 5, label: 'TIME seconds' },
    { id: 11, label: 'TIME swatch beats' },
    { id: 30, label: 'DATE wkday dd' },
    { id: 31, label: 'DATE wkday d.m.' },
    { id: 32, label: 'DATE wkday' },
    { id: 26, label: 'DATE d.m.' },
    { id: 27, label: 'DATE dd.mm.' },
    { id: 28, label: 'DATE dd.mm' },
    { id: 29, label: 'DATE d.m.yyyy' },
    { id: 33, label: 'DATE dd mon' },
    { id: 34, label: 'DATE yyyy-mm-dd' },
    { id: 6, label: 'DATE week number' },
    { id: 7, label: 'WEATHER temperature' },
    { id: 37, label: 'WEATHER condition icon' },
    { id: 8, label: 'WEATHER forecast high/low' },
    { id: 13, label: 'WEATHER UV index' },
    { id: 10, label: 'HEALTH steps' },
    { id: 21, label: 'HEALTH distance' },
    { id: 12, label: 'HEALTH heart rate' },
    { id: 9, label: 'SLEEP hours' },
    { id: 20, label: 'SLEEP deep hours' },
    { id: 24, label: 'SLEEP + deep hours' },
    { id: 25, label: 'SLEEP hours + deep bar' },
    { id: 2, label: 'BATTERY percentage' },
    { id: 36, label: 'BATTERY icon' },
    { id: 22, label: 'BATTERY days left' },
    { id: 14, label: 'ELECTR price' },
    { id: 18, label: 'ELECTR next cheap' },
    { id: 19, label: 'ELECTR cheapest hour' },
    { id: 38, label: 'MISC bluetooth icon' },
    { id: 23, label: 'MISC Tuya LEDs' },
  ];

  // Font choices; index == font index in the payload. KEEP IN SYNC with
  // src/ts/widget_options.ts FONT_OPTIONS and docs/widget_contract.md.
  const FONT_OPTIONS: string[] = [
    '3XS Gothic',
    '2XS Gothic',
    'XS Gothic',
    'S Gothic',
    'S Roboto',
    'M Gothic',
    'M Leco',
    'L Bitham',
    'L Droid serif',
    'L Leco',
    'XL Leco',
    '2XL Bitham',
    '2XL Leco 36',
    '2XL Leco 38',
    '3XL Leco',
    '4XL Leco',
    '5XL Leco',
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
      out.push({ id: wid, label: 'RATE ' + label });
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
      out.push({ id: cwid, label: 'RATE ' + label });
    }
    const tarr = readTuyaRows();
    for (let i = 0; i < tarr.length; i++) {
      const r = tarr[i];
      if (!r) { continue; }
      const twid = r.wid;
      if (isNaN(twid) || twid === 0) { continue; }
      const label = (typeof r.label === 'string' && r.label !== '') ? r.label : (r.code || 'Tuya');
      out.push({ id: twid, label: 'MISC ' + label });
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

  // ---- slot model: {id,ident,font} (plain) | {rotating:true, interval, members:[{id,ident,font}]}

  function normFont(raw: any): number {
    const f = parseInt(raw, 10);
    return (!isNaN(f) && ((f >= 0 && f <= MAX_FONT) || f === FONT_DEFAULT)) ? f : FONT_DEFAULT;
  }

  // Stored entry -> {id, ident, font} (older entries: hide -> 'none'). Accepts an entry object or a legacy int (hide
  // flag in 0x20; stored ids 33..57 can only be hidden ids 1..25 there).
  function normEntry(raw: any): { id: number; ident: string; font: number } {
    if (raw && typeof raw === 'object') {
      return { id: parseInt(raw.id, 10) || 0, ident: normIdent(raw), font: normFont(raw.font) };
    }
    const v = parseInt(raw, 10) || 0;
    if (v > LEGACY_HIDE_FLAG && v <= (LEGACY_HIDE_FLAG | 25)) {
      return { id: v - LEGACY_HIDE_FLAG, ident: 'none', font: FONT_DEFAULT };
    }
    return { id: v, ident: 'icon', font: FONT_DEFAULT };
  }

  function fontOptionsHtml(selected: number): string {
    let defLabel = 'Default';
    if (!isNaN(cfgDefFont) && cfgDefFont >= 0 && cfgDefFont < FONT_OPTIONS.length) {
      defLabel += ' (' + FONT_OPTIONS[cfgDefFont] + ')';
    }
    let html = '<option value="' + FONT_DEFAULT + '"' + (selected === FONT_DEFAULT ? ' selected' : '') +
      '>' + defLabel + '</option>';
    for (let i = 0; i < FONT_OPTIONS.length; i++) {
      html += '<option value="' + i + '"' + (i === selected ? ' selected' : '') + '>' + FONT_OPTIONS[i] + '</option>';
    }
    return html;
  }

  function fontRowHtml(id: number, font: number, cls: string): string {
    if (!hasFontId(id)) { return fontPlaceholderHtml(); }
    // A native <select> (opacity 0) stretched over the visible "Aa" label: closed it
    // reads "Aa", tapped it opens the native picker with the full list.
    return '<span class="wl-fontbox' + (font !== FONT_DEFAULT ? ' wl-custom' : '') + '">' +
      '<span class="wl-aa" aria-hidden="true">Aa</span>' +
      '<select class="' + cls + '" title="Font" aria-label="Font">' + fontOptionsHtml(font) + '</select></span>';
  }

  function memberHtml(raw: any): string {
    const e = normEntry(raw);
    return '<div class="wl-mem">' +
      '<div class="wl-mmain">' +
      '<select class="wl-msel">' + memberOptionsHtml(e.id) + '</select>' +
      fontRowHtml(e.id, e.font, 'wl-mfont') +
      hideBtnHtml(e.id, e.ident, 'wl-mhide') +
      buttonPlaceholderHtml() +
      buttonPlaceholderHtml() +
      '<button type="button" class="wl-mdel" title="Remove member">&#10005;</button>' +
      '</div></div>';
  }

  function groupHtml(interval: number, members: any[]): string {
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
    const e = normEntry(isRot ? 0 : slot);
    const mainSel = isRot ? ROTATING : e.id;
    let html = '<div class="wl-row">' +
      '<div class="wl-main">' +
      '<select class="wl-sel">' + mainOptionsHtml(mainSel) + '</select>' +
      fontRowHtml(isRot ? 255 : e.id, isRot ? FONT_DEFAULT : e.font, 'wl-font') +
      hideBtnHtml(isRot ? 255 : e.id, isRot ? 'icon' : e.ident, 'wl-hide') +
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
        const members: any[] = [];
        const memEls = row.querySelectorAll('.wl-mems .wl-mem');
        for (let m = 0; m < memEls.length; m++) {
          const mEl = memEls[m] as HTMLElement;
          const msel = mEl.querySelector('.wl-msel') as HTMLSelectElement;
          const base = msel ? (parseInt(msel.value, 10) || 0) : 0;
          const mhb = mEl.querySelector('.wl-mhide') as HTMLButtonElement;
          const mfont = mEl.querySelector('.wl-mfont') as HTMLSelectElement;
          members.push({ id: base, ident: mhb ? normIdent({ ident: mhb.getAttribute('data-ident') }) : 'icon',
            font: mfont ? normFont(mfont.value) : FONT_DEFAULT });
        }
        slots.push({ rotating: true, interval: interval, members: members });
      } else {
        const hb = row.querySelector('.wl-main .wl-hide') as HTMLButtonElement;
        const font = row.querySelector('.wl-font') as HTMLSelectElement;
        slots.push({ id: mainVal, ident: hb ? normIdent({ ident: hb.getAttribute('data-ident') }) : 'icon',
          font: font ? normFont(font.value) : FONT_DEFAULT });
      }
    }
    return slots;
  }

  // slot objects -> the component value (normalized slot objects).
  function slotsToValue(slots: any[]): any[] {
    const out: any[] = [];
    for (let i = 0; i < slots.length && out.length < MAX; i++) {
      const s = slots[i];
      if (s && s.rotating) {
        const mems: any[] = [];
        const arr = Array.isArray(s.members) ? s.members : [];
        for (let m = 0; m < arr.length && mems.length < MAX_MEMBERS; m++) {
          const e = normEntry(arr[m]);
          if (e.id !== 0 && e.id !== ROTATING) {
            mems.push({ id: e.id, ident: resolveIdent(e.id, e.ident), font: e.font });
          }
        }
        const iv = (s.interval >= 0 && s.interval <= 5) ? s.interval : DEFAULT_INTERVAL;
        if (mems.length >= 2) {
          out.push({ rotating: true, interval: iv, members: mems });
        } else if (mems.length === 1) {
          out.push(mems[0]);
        }
      } else if (s) {
        const e = normEntry(s);
        if (e.id !== ROTATING) {
          out.push({ id: e.id, ident: resolveIdent(e.id, e.ident), font: e.font });
        }
      }
    }
    return out;
  }

  // stored value (slot objects, or an older flat marker-encoded int array) -> slot
  // objects (for rebuild). Retired/internal ids 1 and 4 are dropped.
  function valueToSlots(value: any[]): any[] {
    const slots: any[] = [];
    let i = 0;
    while (i < value.length && slots.length < MAX) {
      const el: any = value[i];
      if (el && typeof el === 'object') {
        i += 1;
        if (el.rotating) {
          const members: any[] = [];
          const src = Array.isArray(el.members) ? el.members : [];
          for (let m = 0; m < src.length && m < MAX_MEMBERS; m++) {
            const e = normEntry(src[m]);
            if (e.id !== 1 && e.id !== 4) { members.push(e); }
          }
          const iv = parseInt(el.interval, 10);
          slots.push({ rotating: true,
            interval: (iv >= 0 && iv <= 5) ? iv : DEFAULT_INTERVAL, members: members });
        } else {
          const e = normEntry(el);
          if (e.id !== 1 && e.id !== 4) { slots.push(e); }
        }
        continue;
      }
      const head = parseInt(el, 10);
      if (head === ROTATING) {
        const count = parseInt(value[i + 1] as any, 10);
        const interval = parseInt(value[i + 2] as any, 10);
        if (isNaN(count) || isNaN(interval)) { break; }
        const members: any[] = [];
        for (let m = 0; m < count && m < MAX_MEMBERS; m++) {
          const raw = parseInt(value[i + 3 + m] as any, 10);
          if (!isNaN(raw)) {
            const e = normEntry(raw);
            if (e.id !== 1 && e.id !== 4) { members.push(e); }
          }
        }
        i = i + 3 + count;
        slots.push({ rotating: true,
          interval: (interval >= 0 && interval <= 5) ? interval : DEFAULT_INTERVAL,
          members: members });
      } else {
        const e = normEntry(isNaN(head) ? 0 : head);
        if (e.id !== 1 && e.id !== 4) { slots.push(e); }
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
      const up = rows[i].querySelector('.wl-up') as HTMLButtonElement;
      const down = rows[i].querySelector('.wl-down') as HTMLButtonElement;
      up.disabled = (i === 0);
      down.disabled = (i === rows.length - 1);
      // A single-slot list (a one-row list) has nothing to reorder.
      up.style.display = down.style.display = (MAX === 1) ? 'none' : '';
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
  self._wlGetValue = function(): any[] { return slotsToValue(readSlots()); };
  self._wlRebuild = function(value: any[]): void { renderSlots(valueToSlots(value)); };

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
      if (slots.length < MAX) { slots.push({ id: 0, ident: 'icon', font: FONT_DEFAULT }); renderSlots(slots); self.trigger('change'); }
      return;
    }

    if (target.classList.contains('wl-hide') || target.classList.contains('wl-mhide')) {
      // cycle through the modes this widget offers (icon -> unit -> none)
      const idSel = (target.parentNode as HTMLElement).querySelector('.wl-sel, .wl-msel') as HTMLSelectElement;
      const modes = identModes(idSel ? (parseInt(idSel.value, 10) || 0) : 0);
      if (modes.length === 0) { return; }
      const cur = target.getAttribute('data-ident') || 'icon';
      const next = modes[(modes.indexOf(cur) + 1) % modes.length];
      if (next === 'none') { target.classList.add('wl-hidden'); } else { target.classList.remove('wl-hidden'); }
      target.setAttribute('data-ident', next);
      target.setAttribute('title', IDENT_TITLE[next]);
      target.innerHTML = IDENT_GLYPH[next];
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
        s.members.push({ id: firstMemberId(), ident: 'icon', font: FONT_DEFAULT }); renderSlots(slots); self.trigger('change');
      }
    } else if (target.classList.contains('wl-mdel')) {
      const s = slots[idx];
      if (s && s.rotating && s.members.length >= 2) {
        // which member?
        const memEl = target.closest('.wl-mem') as HTMLElement;   // .wl-mem
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
            slots[idx] = normEntry(s.members[0]);
          }
          renderSlots(slots); self.trigger('change');
        }
      }
    }
  });

  root.addEventListener('change', function(ev: Event) {
    const t = ev.target as HTMLElement;
    if (!t || t.tagName !== 'SELECT') { return; }
    if (t.classList.contains('wl-font') || t.classList.contains('wl-mfont')) {
      const box = t.parentNode as HTMLElement;
      if ((parseInt((t as HTMLSelectElement).value, 10)) !== FONT_DEFAULT) { box.classList.add('wl-custom'); }
      else { box.classList.remove('wl-custom'); }
    }
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
            slots[idx] = { rotating: true, interval: DEFAULT_INTERVAL, members: [{ id: a, ident: 'icon', font: FONT_DEFAULT }, { id: a, ident: 'icon', font: FONT_DEFAULT }] };
          }
        } else {
          slots[idx] = { id: newVal, ident: 'icon', font: slots[idx].rotating ? FONT_DEFAULT : slots[idx].font };
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
    '.wl-main,.wl-mmain{display:flex;align-items:center;flex-wrap:nowrap;min-width:0}' +
    '.wl-row .wl-sel,.wl-mem .wl-msel{flex:1 1 0;min-width:0;width:0;height:2.6rem;margin:0;' +
      'background-color:#767676;color:#fff;border:none;border-radius:0.3rem;' +
      'padding:0 0.3rem;font-size:.85rem;text-overflow:ellipsis;white-space:nowrap;overflow:hidden;color-scheme:dark}' +
    '.wl-row .wl-main button,.wl-mem .wl-mmain button{flex:0 0 auto;min-width:0;width:2rem;height:2.6rem;' +
      'margin:0 0 0 3px;padding:0;font-size:.85rem}' +
    '.wl-row .wl-hidden{opacity:.55}' +
    '.wl-row button[disabled]{opacity:.35}' +
    '.wl-fontbox{position:relative;flex:0 0 auto;width:2.5rem;height:2.6rem;margin:0 0 0 3px;' +
      'background-color:#767676;border-radius:0.3rem;display:block}' +
    '.wl-aa{position:absolute;left:0;right:0;top:0;bottom:0;display:flex;align-items:center;' +
      'justify-content:center;color:#fff;font-size:.9rem;pointer-events:none}' +
    '.wl-custom .wl-aa{font-weight:bold;color:#ff9a3c}' +
    '.wl-custom:after{content:"";position:absolute;top:3px;right:3px;width:6px;height:6px;' +
      'border-radius:3px;background:#ff9a3c;pointer-events:none}' +
    '.wl-fontbox select{position:absolute;left:0;top:0;width:100%;height:100%;margin:0;padding:0;' +
      'opacity:0;border:none;font-size:16px}' +
    '.wl-placeholder{flex:0 0 auto;visibility:hidden;pointer-events:none}' +
    '.wl-fontbox-ph{width:2.5rem;height:2.6rem;margin:0 0 0 3px}' +
    '.wl-hide-ph{width:2rem;height:2.6rem;margin:0 0 0 3px}' +
    '.wl-btn-ph{width:2rem;height:2.6rem;margin:0 0 0 3px}' +
    '.wl-group{margin:6px 0 0 12px;padding:0;border-left:3px solid #767676}' +
    '.wl-introw{display:flex;align-items:center;margin:0 0 6px -12px;padding:6px 0 0 20px}' +
    '.wl-intlbl{margin:0 8px 0 0;color:#fff}' +
    '.wl-group .wl-int{flex:1 1 auto;min-width:0;height:2.6rem;background-color:#767676;color:#fff;' +
      'border:none;border-radius:0.3rem;padding:0 0.5rem;color-scheme:dark}' +
    '.wl-mems{margin:0 0 0 -12px;padding:0 0 0 20px}' +
    '.wl-mem{margin:0 0 8px 0}' +
    '.wl-madd{min-width:0;margin:2px 0 4px 0}' +
    '.wl-add{margin:8px 0 10px 0}',
  manipulator: {
    get: function(this: any): any[] {
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

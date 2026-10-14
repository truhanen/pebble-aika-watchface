"use strict";
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
function widgetListInitialize(_minified, clayConfig) {
    var self = this;
    var root = self.$element[0];
    var MAX = 16; // max top-level rows (matches MAX_WIDGET_LIST bytes-ish)
    var ROTATING = 255;
    var MAX_MEMBERS = 6;
    var DEFAULT_INTERVAL = 3; // 1 min
    var HIDE_FLAG = 0x20; // bit set on an id => identifier hidden (see widget_list.h)
    // Toggleable = has an icon or a title label to hide. No-op for Empty(0),
    // Bluetooth(1), Today's Date(4), Seconds(5), and the Rotating head(255).
    function isToggleableId(base) {
        return base !== 0 && base !== 1 && base !== 4 && base !== 5 && base !== ROTATING;
    }
    function hideBtnHtml(encoded, cls) {
        var base = encoded & 0xdf;
        if (!isToggleableId(base)) {
            return '';
        }
        var hidden = (encoded & HIDE_FLAG) !== 0;
        return '<button type="button" class="' + cls + (hidden ? ' wl-hidden' : '') +
            '" title="Show/hide icon or title">' + (hidden ? '⊘' : '◉') + '</button>';
    }
    // Static widget options. KEEP IN SYNC with src/ts/widget_options.ts STATIC_WIDGETS
    // and src/c/sidebar_widgets.c. Includes Empty(0) + Rotating(255) for the MAIN
    // select; member selects filter those two out (memberOptionsHtml).
    var STATIC_OPTIONS = [
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
    var INTERVAL_OPTIONS = [
        { code: 0, label: '5 s' },
        { code: 1, label: '10 s' },
        { code: 2, label: '30 s' },
        { code: 3, label: '1 min' },
        { code: 4, label: '2 min' },
        { code: 5, label: '5 min' },
    ];
    function readCryptoRows() {
        var out = [];
        var rowEls = document.querySelectorAll('.cl-root .cl-row');
        for (var i = 0; i < rowEls.length; i++) {
            var el = rowEls[i];
            var wid = parseInt(el.getAttribute('data-wid') || '0', 10) || 0;
            var coinSel = el.querySelector('.cl-coin');
            var coin = coinSel ? coinSel.value : '';
            if (coin === 'custom') {
                var cust = el.querySelector('.cl-custom');
                coin = cust ? cust.value : '';
            }
            var labelIn = el.querySelector('.cl-label');
            out.push({ wid: wid, coin: coin, label: labelIn ? labelIn.value : '' });
        }
        return out;
    }
    function readCurrencyRows() {
        var out = [];
        var rowEls = document.querySelectorAll('.cul-root .cul-row');
        for (var i = 0; i < rowEls.length; i++) {
            var el = rowEls[i];
            var wid = parseInt(el.getAttribute('data-wid') || '0', 10) || 0;
            var baseSel = el.querySelector('.cul-base');
            var base = baseSel ? baseSel.value : '';
            if (base === 'custom') {
                var c = el.querySelector('.cul-base-custom');
                base = c ? c.value.toUpperCase() : '';
            }
            var quoteSel = el.querySelector('.cul-quote');
            var quote = quoteSel ? quoteSel.value : '';
            if (quote === 'custom') {
                var c = el.querySelector('.cul-quote-custom');
                quote = c ? c.value.toUpperCase() : '';
            }
            var labelIn = el.querySelector('.cul-label');
            out.push({ wid: wid, base: base, quote: quote, label: labelIn ? labelIn.value : '' });
        }
        return out;
    }
    function readTuyaRows() {
        var out = [];
        var rowEls = document.querySelectorAll('.tul-root .tul-row');
        for (var i = 0; i < rowEls.length; i++) {
            var el = rowEls[i];
            var wid = parseInt(el.getAttribute('data-wid') || '0', 10) || 0;
            var devSel = el.querySelector('.tul-device');
            var codeSel = el.querySelector('.tul-code');
            var labelIn = el.querySelector('.tul-label');
            out.push({ wid: wid, deviceId: devSel ? devSel.value : '', code: codeSel ? codeSel.value : '',
                label: labelIn ? labelIn.value : '' });
        }
        return out;
    }
    function currentOptions() {
        var out = STATIC_OPTIONS.slice();
        var arr = readCryptoRows();
        for (var i = 0; i < arr.length; i++) {
            var r = arr[i];
            if (!r) {
                continue;
            }
            var wid = r.wid;
            if (isNaN(wid) || wid === 0) {
                continue;
            }
            var coin = (typeof r.coin === 'string') ? r.coin : '';
            var label = (typeof r.label === 'string' && r.label !== '') ? r.label : coin.toUpperCase();
            out.push({ id: wid, label: label });
        }
        var carr = readCurrencyRows();
        for (var i = 0; i < carr.length; i++) {
            var r = carr[i];
            if (!r) {
                continue;
            }
            var cwid = r.wid;
            if (isNaN(cwid) || cwid === 0) {
                continue;
            }
            var base = (typeof r.base === 'string') ? r.base : '';
            var quote = (typeof r.quote === 'string') ? r.quote : '';
            var label = (typeof r.label === 'string' && r.label !== '') ? r.label : (base + '/' + quote);
            out.push({ id: cwid, label: label });
        }
        var tarr = readTuyaRows();
        for (var i = 0; i < tarr.length; i++) {
            var r = tarr[i];
            if (!r) {
                continue;
            }
            var twid = r.wid;
            if (isNaN(twid) || twid === 0) {
                continue;
            }
            var label = (typeof r.label === 'string' && r.label !== '') ? r.label : (r.code || 'Tuya');
            out.push({ id: twid, label: label });
        }
        return out;
    }
    // Options for the MAIN select (includes Empty + Rotating). `selected` preserved
    // even if its option isn't built yet (crypto rows build later).
    function mainOptionsHtml(selected) {
        var opts = currentOptions();
        var html = '';
        var found = false;
        for (var i = 0; i < opts.length; i++) {
            var o = opts[i];
            if (o.id === selected) {
                found = true;
            }
            html += '<option value="' + o.id + '"' + (o.id === selected ? ' selected' : '') + '>' + o.label + '</option>';
        }
        if (!found && selected !== 0) {
            html += '<option value="' + selected + '" selected>Crypto #' + selected + '</option>';
        }
        return html;
    }
    // Options for a MEMBER select: drawable widgets only (drop Empty(0) + Rotating(255)).
    function memberOptionsHtml(selected) {
        var opts = currentOptions();
        var html = '';
        var found = false;
        for (var i = 0; i < opts.length; i++) {
            var o = opts[i];
            if (o.id === 0 || o.id === ROTATING) {
                continue;
            }
            if (o.id === selected) {
                found = true;
            }
            html += '<option value="' + o.id + '"' + (o.id === selected ? ' selected' : '') + '>' + o.label + '</option>';
        }
        if (!found && selected !== 0 && selected !== ROTATING) {
            html += '<option value="' + selected + '" selected>Crypto #' + selected + '</option>';
        }
        return html;
    }
    function firstMemberId() {
        var opts = currentOptions();
        for (var i = 0; i < opts.length; i++) {
            if (opts[i].id !== 0 && opts[i].id !== ROTATING) {
                return opts[i].id;
            }
        }
        return 2; // Battery fallback
    }
    function intervalOptionsHtml(selected) {
        var html = '';
        for (var i = 0; i < INTERVAL_OPTIONS.length; i++) {
            var o = INTERVAL_OPTIONS[i];
            html += '<option value="' + o.code + '"' + (o.code === selected ? ' selected' : '') + '>' + o.label + '</option>';
        }
        return html;
    }
    // ---- slot model: {id} (plain) | {rotating:true, interval, members:[]} ----
    function memberHtml(encoded) {
        var base = encoded & 0xdf;
        return '<div class="wl-mem">' +
            '<select class="wl-msel">' + memberOptionsHtml(base) + '</select>' +
            hideBtnHtml(encoded, 'wl-mhide') +
            '<button type="button" class="wl-mdel" title="Remove member">&#10005;</button>' +
            '</div>';
    }
    function groupHtml(interval, members) {
        var mem = '';
        for (var i = 0; i < members.length; i++) {
            mem += memberHtml(members[i]);
        }
        return '<div class="wl-group">' +
            '<div class="wl-introw"><span class="wl-intlbl">Every</span>' +
            '<select class="wl-int">' + intervalOptionsHtml(interval) + '</select></div>' +
            '<div class="wl-mems">' + mem + '</div>' +
            '<button type="button" class="wl-madd">+ add member</button>' +
            '</div>';
    }
    function rowHtml(slot) {
        var isRot = !!slot.rotating;
        var mainSel = isRot ? ROTATING : (parseInt(slot.id, 10) || 0);
        var encoded = (!isRot && slot.hide) ? (mainSel | HIDE_FLAG) : mainSel;
        var html = '<div class="wl-row">' +
            '<div class="wl-main">' +
            '<select class="wl-sel">' + mainOptionsHtml(mainSel) + '</select>' +
            (isRot ? '' : hideBtnHtml(encoded, 'wl-hide')) +
            '<button type="button" class="wl-up" title="Move up">&#9650;</button>' +
            '<button type="button" class="wl-down" title="Move down">&#9660;</button>' +
            '<button type="button" class="wl-del" title="Remove">&#10005;</button>' +
            '</div>';
        if (isRot) {
            var iv = (typeof slot.interval === 'number') ? slot.interval : DEFAULT_INTERVAL;
            var mems = Array.isArray(slot.members) ? slot.members : [];
            html += groupHtml(iv, mems);
        }
        return html + '</div>';
    }
    // Read the DOM rows into slot objects.
    function readSlots() {
        var slots = [];
        var rows = root.querySelectorAll('.wl-row');
        for (var i = 0; i < rows.length; i++) {
            var row = rows[i];
            var main = row.querySelector('.wl-sel');
            var mainVal = main ? (parseInt(main.value, 10) || 0) : 0;
            if (mainVal === ROTATING) {
                var intSel = row.querySelector('.wl-int');
                // NOTE: code 0 (5 s) is a VALID interval but falsy in JS, so `parseInt() ||
                // DEFAULT` would silently coerce a 5 s selection to 1 min. Use an explicit
                // NaN check, not `||`.
                var interval = DEFAULT_INTERVAL;
                if (intSel) {
                    var ivCode = parseInt(intSel.value, 10);
                    if (!isNaN(ivCode)) {
                        interval = ivCode;
                    }
                }
                var members = [];
                var memEls = row.querySelectorAll('.wl-mems .wl-mem');
                for (var m = 0; m < memEls.length; m++) {
                    var mEl = memEls[m];
                    var msel = mEl.querySelector('.wl-msel');
                    var base = msel ? (parseInt(msel.value, 10) || 0) : 0;
                    var mhb = mEl.querySelector('.wl-mhide');
                    var mh = !!(mhb && mhb.classList.contains('wl-hidden'));
                    members.push(mh ? (base | HIDE_FLAG) : base);
                }
                slots.push({ rotating: true, interval: interval, members: members });
            }
            else {
                var hb = row.querySelector('.wl-main .wl-hide');
                var hidden = !!(hb && hb.classList.contains('wl-hidden'));
                slots.push({ id: mainVal, hide: hidden });
            }
        }
        return slots;
    }
    // slot objects -> flat marker-encoded value array.
    function slotsToValue(slots) {
        var out = [];
        for (var i = 0; i < slots.length; i++) {
            var s = slots[i];
            if (s && s.rotating) {
                var mems = [];
                var arr = Array.isArray(s.members) ? s.members : [];
                for (var m = 0; m < arr.length && mems.length < MAX_MEMBERS; m++) {
                    var raw = parseInt(arr[m], 10);
                    var base = raw & 0xdf;
                    if (!isNaN(raw) && base !== 0 && base !== ROTATING) {
                        mems.push(raw);
                    }
                }
                var iv = (s.interval >= 0 && s.interval <= 5) ? s.interval : DEFAULT_INTERVAL;
                if (mems.length >= 2) {
                    out.push(ROTATING, mems.length, iv);
                    for (var m = 0; m < mems.length; m++) {
                        out.push(mems[m]);
                    }
                }
                else if (mems.length === 1) {
                    out.push(mems[0]);
                }
            }
            else if (s) {
                var id = parseInt(s.id, 10) || 0;
                out.push((s.hide && isToggleableId(id & 0xdf)) ? (id | HIDE_FLAG) : id);
            }
        }
        return out;
    }
    // flat marker-encoded value array -> slot objects (for rebuild).
    function valueToSlots(value) {
        var slots = [];
        var i = 0;
        while (i < value.length && slots.length < MAX) {
            var head = parseInt(value[i], 10);
            if (head === ROTATING) {
                var count = parseInt(value[i + 1], 10);
                var interval = parseInt(value[i + 2], 10);
                if (isNaN(count) || isNaN(interval)) {
                    break;
                }
                var members = [];
                for (var m = 0; m < count && m < MAX_MEMBERS; m++) {
                    var id = parseInt(value[i + 3 + m], 10);
                    if (!isNaN(id)) {
                        members.push(id);
                    }
                }
                i = i + 3 + count;
                slots.push({ rotating: true,
                    interval: (interval >= 0 && interval <= 5) ? interval : DEFAULT_INTERVAL,
                    members: members });
            }
            else {
                var v = isNaN(head) ? 0 : head;
                slots.push({ id: v & 0xdf, hide: (v & HIDE_FLAG) !== 0 });
                i += 1;
            }
        }
        return slots;
    }
    function renderSlots(slots) {
        var list = root.querySelector('.wl-list');
        var html = '';
        for (var i = 0; i < slots.length && i < MAX; i++) {
            html += rowHtml(slots[i]);
        }
        list.innerHTML = html;
        updateButtons();
    }
    function updateButtons() {
        var rows = root.querySelectorAll('.wl-row');
        for (var i = 0; i < rows.length; i++) {
            rows[i].querySelector('.wl-up').disabled = (i === 0);
            rows[i].querySelector('.wl-down').disabled = (i === rows.length - 1);
            // Member delete is never disabled: deleting the 2nd-to-last member collapses the
            // whole group to a plain widget (a rotating group with 1 member == a plain widget),
            // so a group in the DOM always has >= 2 members. See the wl-mdel handler.
            var mdels = rows[i].querySelectorAll('.wl-mems .wl-mdel');
            var madd = rows[i].querySelector('.wl-madd');
            if (madd) {
                madd.style.display = (mdels.length >= MAX_MEMBERS) ? 'none' : '';
            }
        }
        var add = root.querySelector('.wl-add');
        if (add) {
            add.style.display = (rows.length >= MAX) ? 'none' : '';
        }
    }
    // expose for the manipulator (set runs after initialize)
    self._wlGetValue = function () { return slotsToValue(readSlots()); };
    self._wlRebuild = function (value) { renderSlots(valueToSlots(value)); };
    function rowIndexOf(node) {
        var rows = root.querySelectorAll('.wl-row');
        for (var i = 0; i < rows.length; i++) {
            if (rows[i] === node) {
                return i;
            }
        }
        return -1;
    }
    function topRowOf(el) {
        var n = el;
        while (n && n !== root) {
            if (n.classList && n.classList.contains('wl-row')) {
                return n;
            }
            n = n.parentNode;
        }
        return null;
    }
    root.addEventListener('click', function (ev) {
        var target = ev.target;
        if (!target) {
            return;
        }
        if (target.tagName !== 'BUTTON') {
            target = target.closest ? target.closest('button') : null;
        }
        if (!target) {
            return;
        }
        if (target.classList.contains('wl-add')) {
            var slots_1 = readSlots();
            if (slots_1.length < MAX) {
                slots_1.push({ id: 0 });
                renderSlots(slots_1);
                self.trigger('change');
            }
            return;
        }
        if (target.classList.contains('wl-hide') || target.classList.contains('wl-mhide')) {
            var nowHidden = !target.classList.contains('wl-hidden');
            if (nowHidden) {
                target.classList.add('wl-hidden');
            }
            else {
                target.classList.remove('wl-hidden');
            }
            target.innerHTML = nowHidden ? '⊘' : '◉';
            self.trigger('change');
            return;
        }
        var row = topRowOf(target);
        if (!row) {
            return;
        }
        var idx = rowIndexOf(row);
        if (idx === -1) {
            return;
        }
        var slots = readSlots();
        if (target.classList.contains('wl-del')) {
            slots.splice(idx, 1);
            renderSlots(slots);
            self.trigger('change');
        }
        else if (target.classList.contains('wl-up') && idx > 0) {
            var t = slots[idx];
            slots[idx] = slots[idx - 1];
            slots[idx - 1] = t;
            renderSlots(slots);
            self.trigger('change');
        }
        else if (target.classList.contains('wl-down') && idx < slots.length - 1) {
            var t = slots[idx];
            slots[idx] = slots[idx + 1];
            slots[idx + 1] = t;
            renderSlots(slots);
            self.trigger('change');
        }
        else if (target.classList.contains('wl-madd')) {
            var s = slots[idx];
            if (s && s.rotating && s.members.length < MAX_MEMBERS) {
                s.members.push(firstMemberId());
                renderSlots(slots);
                self.trigger('change');
            }
        }
        else if (target.classList.contains('wl-mdel')) {
            var s = slots[idx];
            if (s && s.rotating && s.members.length >= 2) {
                // which member?
                var memEl = target.parentNode; // .wl-mem
                var memsWrap = memEl.parentNode; // .wl-mems
                var mems = memsWrap.querySelectorAll('.wl-mem');
                var mi = -1;
                for (var k = 0; k < mems.length; k++) {
                    if (mems[k] === memEl) {
                        mi = k;
                        break;
                    }
                }
                if (mi !== -1) {
                    s.members.splice(mi, 1);
                    // A rotating group with a single member is identical to a plain widget, so
                    // collapse the row to a plain slot (mirrors slotsToValue / the C sanitizer)
                    // rather than leaving a lone member with a disabled delete button.
                    if (s.members.length === 1) {
                        var raw = parseInt(s.members[0], 10) || 0;
                        slots[idx] = { id: raw & 0xdf, hide: (raw & HIDE_FLAG) !== 0 };
                    }
                    renderSlots(slots);
                    self.trigger('change');
                }
            }
        }
    });
    root.addEventListener('change', function (ev) {
        var t = ev.target;
        if (!t || t.tagName !== 'SELECT') {
            return;
        }
        if (t.classList.contains('wl-sel')) {
            // main select changed: reconcile rotating <-> plain, seeding a fresh group.
            var slots = readSlots();
            var row = topRowOf(t);
            var idx = row ? rowIndexOf(row) : -1;
            if (idx !== -1) {
                var newVal = parseInt(t.value, 10) || 0;
                if (newVal === ROTATING) {
                    if (!slots[idx].rotating) {
                        var a = firstMemberId();
                        slots[idx] = { rotating: true, interval: DEFAULT_INTERVAL, members: [a, a] };
                    }
                }
                else {
                    slots[idx] = { id: newVal, hide: false };
                }
                renderSlots(slots);
            }
        }
        self.trigger('change');
    });
    // Refresh crypto-coin options when the user opens a select (main or member).
    root.addEventListener('mousedown', function (ev) {
        var t = ev.target;
        if (!t || t.tagName !== 'SELECT') {
            return;
        }
        var sel = t;
        var cur = parseInt(sel.value, 10) || 0;
        if (sel.classList.contains('wl-sel')) {
            sel.innerHTML = mainOptionsHtml(cur);
        }
        else if (sel.classList.contains('wl-msel')) {
            sel.innerHTML = memberOptionsHtml(cur);
        }
    });
    // After ALL components build (cryptoList DOM now exists), refresh every select's
    // options with real coin labels, preserving each selection.
    if (clayConfig && clayConfig.on && clayConfig.EVENTS) {
        clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
            var mains = root.querySelectorAll('.wl-row .wl-sel');
            for (var i = 0; i < mains.length; i++) {
                var sel = mains[i];
                sel.innerHTML = mainOptionsHtml(parseInt(sel.value, 10) || 0);
            }
            var mems = root.querySelectorAll('.wl-row .wl-msel');
            for (var i = 0; i < mems.length; i++) {
                var sel = mems[i];
                sel.innerHTML = memberOptionsHtml(parseInt(sel.value, 10) || 0);
            }
        });
    }
}
var widgetListComponent = {
    name: 'widgetList',
    template: '<div class="wl-root">' +
        '<div class="wl-list"></div>' +
        '<button type="button" class="wl-add">+ Add widget</button>' +
        '</div>',
    // Clay's base theme styles `button { min-width: 12rem; margin: 0 auto }`. Row /
    // member buttons MUST override min-width (else forced to 12rem, squeezing selects).
    style: '.wl-row{display:flex;flex-direction:column;margin:0 0 8px 0}' +
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
        get: function () {
            return this._wlGetValue ? this._wlGetValue() : [];
        },
        set: function (value) {
            // Inlined normalization (toSource: no module-scope helper). Accepts a bare
            // array, a JSON string, or a {value:X} wrapper; anything else -> [].
            var ids = [];
            var v = value;
            if (v && typeof v === 'object' && !Array.isArray(v) && v.value !== undefined) {
                v = v.value;
            }
            if (Array.isArray(v)) {
                ids = v;
            }
            else if (typeof v === 'string' && v !== '') {
                try {
                    var parsed = JSON.parse(v);
                    if (Array.isArray(parsed)) {
                        ids = parsed;
                    }
                }
                catch (e) {
                    ids = [];
                }
            }
            if (this._wlRebuild) {
                this._wlRebuild(ids);
            }
            return this;
        },
    },
    defaults: { label: '' },
    initialize: widgetListInitialize,
};
module.exports = widgetListComponent;

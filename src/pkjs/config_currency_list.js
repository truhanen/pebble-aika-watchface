"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
/* Clay custom component "currencyList": the SET of fiat currency pairs to track.
   Value = array of { wid, base, quote, p, label }. Each row is one compact line: a
   base <select> (+ "Custom…" revealing a free-text 3-letter code), a quote <select>
   (same), a numeric decimals input `p`, an optional label (empty -> auto
   "BASE/QUOTE"), and a remove button. Row order is irrelevant (sidebar position is
   decided in the widget list). Each row carries a STABLE wid (data-wid) in
   [216, 223); the widget-list component reads these rows live from the DOM to build
   its currency options (see config_widget_list.ts).

   Serialized via toSource() + re-eval'd in the webview -> fully self-contained: no
   runtime imports, no spread/destructuring, no module-scope helpers, no TS
   downlevel helpers. Native DOM + native array methods only. See
   docs/superpowers/specs/2026-07-01-timestyle-currency-exchange-widget-design.md */
function currencyListInitialize(_minified, _clayConfig) {
    var self = this;
    var root = self.$element[0];
    var CURRENCY_BASE = 216;
    var MAX_CURRENCY = 7; // matches MAX_CURRENCY in src/c/currency.h
    var CURRENCIES = [
        'EUR', 'USD', 'GBP', 'JPY', 'CHF', 'SEK', 'NOK', 'DKK', 'PLN', 'CZK',
        'HUF', 'RON', 'BGN', 'ISK', 'TRY', 'CAD', 'AUD', 'NZD', 'CNY', 'HKD',
        'SGD', 'INR', 'BRL', 'ZAR', 'MXN', 'KRW', 'THB', 'ILS',
    ];
    function escAttr(s) {
        return String(s).replace(/&/g, '&amp;').replace(/"/g, '&quot;')
            .replace(/</g, '&lt;').replace(/>/g, '&gt;');
    }
    function ccyOptionsHtml(selected) {
        var html = '';
        var known = false;
        for (var i = 0; i < CURRENCIES.length; i++) {
            var c = CURRENCIES[i];
            if (c === selected) {
                known = true;
            }
            html += '<option value="' + escAttr(c) + '"' +
                (c === selected ? ' selected' : '') + '>' + escAttr(c) + '</option>';
        }
        var isCustom = (!known && selected !== '');
        html += '<option value="custom"' + (isCustom ? ' selected' : '') + '>Custom…</option>';
        return html;
    }
    function rowHtml(row) {
        var base = (row && typeof row.base === 'string' && row.base !== '') ? row.base : 'EUR';
        var quote = (row && typeof row.quote === 'string' && row.quote !== '') ? row.quote : 'USD';
        var p = (row && row.p !== undefined && row.p !== null) ? row.p : 4;
        var t = (row && row.t !== undefined && row.t !== null) ? row.t : 0;
        var label = (row && typeof row.label === 'string') ? row.label : '';
        var wid = (row && row.wid !== undefined) ? parseInt(row.wid, 10) : 0;
        var baseKnown = (CURRENCIES.indexOf(base) !== -1);
        var quoteKnown = (CURRENCIES.indexOf(quote) !== -1);
        var baseCustHidden = baseKnown ? ' style="display:none"' : '';
        var quoteCustHidden = quoteKnown ? ' style="display:none"' : '';
        return '<div class="cul-row" data-wid="' + wid + '">' +
            '<div class="cul-line">' +
            '<select class="cul-base">' + ccyOptionsHtml(baseKnown ? base : 'custom') + '</select>' +
            '<span class="cul-sep">/</span>' +
            '<select class="cul-quote">' + ccyOptionsHtml(quoteKnown ? quote : 'custom') + '</select>' +
            '<input class="cul-p" type="number" step="1" title="precision (decimals; negative rounds)" value="' +
            escAttr(String(p)) + '">' +
            '<input class="cul-t" type="number" step="1" min="0" title="trim: leading digits to cut (1.160, trim 2 -> 60)" value="' +
            escAttr(String(t)) + '">' +
            '<button type="button" class="cul-del" title="Remove">&#10005;</button>' +
            '</div>' +
            '<input class="cul-base-custom" type="text" maxlength="3" placeholder="base code" value="' +
            (baseKnown ? '' : escAttr(base)) + '"' + baseCustHidden + '>' +
            '<input class="cul-quote-custom" type="text" maxlength="3" placeholder="quote code" value="' +
            (quoteKnown ? '' : escAttr(quote)) + '"' + quoteCustHidden + '>' +
            '<input class="cul-label" type="text" placeholder="' + escAttr(quote) +
            '" maxlength="7" value="' + escAttr(label) + '">' +
            '</div>';
    }
    function currentRows() {
        var rows = [];
        var rowEls = root.querySelectorAll('.cul-row');
        for (var i = 0; i < rowEls.length; i++) {
            var el = rowEls[i];
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
            var pIn = el.querySelector('.cul-p');
            var p = parseInt(pIn ? pIn.value : '4', 10);
            if (isNaN(p)) {
                p = 4;
            }
            var tIn = el.querySelector('.cul-t');
            var t = parseInt(tIn ? tIn.value : '0', 10);
            if (isNaN(t) || t < 0) {
                t = 0;
            }
            var labelIn = el.querySelector('.cul-label');
            var wid = parseInt(el.getAttribute('data-wid') || '0', 10) || 0;
            rows.push({ wid: wid, base: base, quote: quote, p: p, t: t, label: labelIn ? labelIn.value : '' });
        }
        return rows;
    }
    function usedWids() {
        var out = [];
        var rowEls = root.querySelectorAll('.cul-row');
        for (var i = 0; i < rowEls.length; i++) {
            out.push(parseInt(rowEls[i].getAttribute('data-wid') || '0', 10) || 0);
        }
        return out;
    }
    function nextFreeWid() {
        var used = usedWids();
        for (var w = CURRENCY_BASE; w < CURRENCY_BASE + MAX_CURRENCY; w++) {
            if (used.indexOf(w) === -1) {
                return w;
            }
        }
        return CURRENCY_BASE;
    }
    function updateAddButton() {
        var rowEls = root.querySelectorAll('.cul-row');
        var add = root.querySelector('.cul-add');
        if (add) {
            add.style.display = (rowEls.length >= MAX_CURRENCY) ? 'none' : '';
        }
    }
    function rebuild(rows) {
        var list = root.querySelector('.cul-list');
        var html = '';
        for (var i = 0; i < rows.length && i < MAX_CURRENCY; i++) {
            html += rowHtml(rows[i]);
        }
        list.innerHTML = html;
        updateAddButton();
    }
    self._culCurrentRows = currentRows;
    self._culRebuild = rebuild;
    function rowIndexOf(node) {
        var rowEls = root.querySelectorAll('.cul-row');
        for (var i = 0; i < rowEls.length; i++) {
            if (rowEls[i] === node) {
                return i;
            }
        }
        return -1;
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
        if (target.classList.contains('cul-add')) {
            var rows = currentRows();
            if (rows.length < MAX_CURRENCY) {
                rows.push({ wid: nextFreeWid(), base: 'EUR', quote: 'USD', p: 4, t: 0, label: '' });
                rebuild(rows);
                self.trigger('change');
            }
            return;
        }
        if (target.classList.contains('cul-del')) {
            var rowEl = (target.closest ? target.closest('.cul-row') : null);
            var idx = rowIndexOf(rowEl);
            if (idx === -1) {
                return;
            }
            var rows = currentRows();
            rows.splice(idx, 1);
            rebuild(rows);
            self.trigger('change');
        }
    });
    root.addEventListener('change', function (ev) {
        var t = ev.target;
        if (!t) {
            return;
        }
        if (t.classList.contains('cul-base') || t.classList.contains('cul-quote')) {
            var rowEl = (t.closest ? t.closest('.cul-row') : null);
            if (rowEl) {
                var sel = t;
                var custCls = t.classList.contains('cul-base') ? 'cul-base-custom' : 'cul-quote-custom';
                var cust = rowEl.querySelector('.' + custCls);
                if (cust) {
                    if (sel.value === 'custom') {
                        cust.style.display = '';
                        cust.value = '';
                    }
                    else {
                        cust.style.display = 'none';
                    }
                }
            }
        }
        self.trigger('change');
    });
}
var currencyListComponent = {
    name: 'currencyList',
    template: '<div class="cul-root">' +
        '<div class="cul-list"></div>' +
        '<button type="button" class="cul-add">+ Add pair</button>' +
        '</div>',
    // Clay's base theme forces `button { min-width: 12rem }`; row buttons override
    // it. Native <select>/<input> render light by default; theme them to Clay's dark
    // controls (gray-7 #767676, white text). flex-wrap keeps custom-code + label
    // inputs on a second line.
    style: '.cul-row{display:flex;flex-wrap:wrap;align-items:center;' +
        'border-bottom:1px solid #555;padding:6px 0;margin:0 0 6px 0}' +
        '.cul-line{flex:1 1 100%;display:flex;flex-wrap:wrap;align-items:center;margin:0}' +
        '.cul-row select,.cul-row input{min-width:0;height:2.6rem;margin:0 4px 0 0;' +
        'background-color:#767676;color:#fff;border:none;border-radius:0.3rem;' +
        'padding:0 0.4rem;color-scheme:dark}' +
        '.cul-row .cul-base,.cul-row .cul-quote{flex:1 1 4rem}' +
        '.cul-row .cul-sep{flex:0 0 auto;margin:0 4px 0 0;color:#fff}' +
        '.cul-row .cul-p{flex:0 0 3rem}' +
        '.cul-row .cul-t{flex:0 0 3rem}' +
        '.cul-row .cul-base-custom,.cul-row .cul-quote-custom{flex:1 1 auto;margin-top:4px}' +
        '.cul-row .cul-label{flex:1 1 6rem;margin-top:4px}' +
        '.cul-row button{flex:0 0 auto;min-width:0;width:2.6rem;height:2.6rem;margin:0;padding:0}' +
        '.cul-add{margin:8px 0 10px 0}',
    manipulator: {
        get: function () {
            return this._culCurrentRows ? this._culCurrentRows() : [];
        },
        set: function (value) {
            var rows = [];
            var v = value;
            if (v && typeof v === 'object' && !Array.isArray(v) && v.value !== undefined) {
                v = v.value;
            }
            if (Array.isArray(v)) {
                rows = v;
            }
            else if (typeof v === 'string' && v !== '') {
                try {
                    var parsed = JSON.parse(v);
                    if (Array.isArray(parsed)) {
                        rows = parsed;
                    }
                }
                catch (e) {
                    rows = [];
                }
            }
            if (this._culRebuild) {
                this._culRebuild(rows);
            }
            return this;
        },
    },
    defaults: { label: '' },
    initialize: currencyListInitialize,
};
module.exports = currencyListComponent;

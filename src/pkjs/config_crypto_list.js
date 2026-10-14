"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
/* Clay custom component "cryptoList": the SET of crypto/currency coins to track.
   Value = array of { wid, coin, vs, p, label }. Each row is one compact line: a
   popular-coin <select> (+ "Custom…" revealing a free-text CoinGecko id on a
   second line), a vs-currency <select> (USD/EUR), a numeric precision input `p`,
   an optional label input, and a remove button. There is NO ordering here — a
   coin's position in the sidebar is decided in the widget list, so row order is
   irrelevant (it only affects the order coins appear in the widget picker's
   dropdown). Each row carries a STABLE wid (data-wid): legacy migrated rows keep
   15/16/17; new rows get the smallest free id in [200,216). The widget-list
   component reads these rows live from the DOM to build its crypto options
   (see config_widget_list.ts).

   Serialized via toSource() + re-eval'd in the webview -> fully self-contained:
   no imports at runtime, no spread/destructuring, no module-scope helpers, no
   TS downlevel helpers. Native DOM + native array methods only. The row
   controls are the source of truth, read back on `get`. See
   docs/superpowers/specs/2026-06-13-timestyle-generic-crypto-widgets-design.md */
function cryptoListInitialize(_minified, _clayConfig) {
    var self = this;
    var root = self.$element[0];
    var CRYPTO_BASE = 200;
    var MAX_CRYPTO = 16; // matches MAX_CRYPTO in src/c/crypto.h
    var COINS = [
        { id: 'bitcoin', label: 'BTC', p: -3 },
        { id: 'ethereum', label: 'ETH', p: 0 },
        { id: 'tether', label: 'USDT', p: 2 },
        { id: 'binancecoin', label: 'BNB', p: 0 },
        { id: 'solana', label: 'SOL', p: 1 },
        { id: 'ripple', label: 'XRP', p: 4 },
        { id: 'cardano', label: 'ADA', p: 4 },
        { id: 'dogecoin', label: 'DOGE', p: 4 },
        { id: 'tron', label: 'TRX', p: 4 },
        { id: 'polkadot', label: 'DOT', p: 2 },
        { id: 'chainlink', label: 'LINK', p: 2 },
        { id: 'matic-network', label: 'MATIC', p: 4 },
        { id: 'litecoin', label: 'LTC', p: 1 },
        { id: 'avalanche-2', label: 'AVAX', p: 1 },
        { id: 'monero', label: 'XMR', p: 0 },
        { id: 'stellar', label: 'XLM', p: 4 },
        { id: 'cosmos', label: 'ATOM', p: 2 },
        { id: 'uniswap', label: 'UNI', p: 2 },
        { id: 'shiba-inu', label: 'SHIB', p: 8 },
        { id: 'euro-coin', label: 'EUR', p: 3 },
    ];
    function escAttr(s) {
        return String(s).replace(/&/g, '&amp;').replace(/"/g, '&quot;')
            .replace(/</g, '&lt;').replace(/>/g, '&gt;');
    }
    function coinOptionsHtml(selected) {
        var html = '';
        var known = false;
        for (var i = 0; i < COINS.length; i++) {
            var c = COINS[i];
            if (c.id === selected) {
                known = true;
            }
            html += '<option value="' + escAttr(c.id) + '"' +
                (c.id === selected ? ' selected' : '') + '>' + escAttr(c.label) + '</option>';
        }
        var isCustom = (!known && selected !== '');
        html += '<option value="custom"' + (isCustom ? ' selected' : '') + '>Custom…</option>';
        return html;
    }
    function rowHtml(row) {
        var coin = (row && typeof row.coin === 'string') ? row.coin : 'bitcoin';
        var vs = (row && row.vs === 'eur') ? 'eur' : 'usd';
        var p = (row && row.p !== undefined && row.p !== null) ? row.p : -3;
        var t = (row && row.t !== undefined && row.t !== null) ? row.t : 0;
        var label = (row && typeof row.label === 'string') ? row.label : '';
        var wid = (row && row.wid !== undefined) ? parseInt(row.wid, 10) : 0;
        var sym = symbolFor(coin); // '' for a custom/unknown coin
        var isCustom = (sym === '');
        // A known coin's sidebar label is its symbol (BTC, ETH...), auto-filled and
        // hidden. A custom coin needs both a free-text CoinGecko id and a short label
        // (the raw id would render long/ugly), so both inputs are shown on a 2nd line.
        var labelVal = isCustom ? label : sym;
        var custHidden = isCustom ? '' : ' style="display:none"';
        return '<div class="cl-row" data-wid="' + wid + '">' +
            '<div class="cl-line">' +
            '<select class="cl-coin">' + coinOptionsHtml(coin) + '</select>' +
            '<select class="cl-vs">' +
            '<option value="usd"' + (vs === 'usd' ? ' selected' : '') + '>USD</option>' +
            '<option value="eur"' + (vs === 'eur' ? ' selected' : '') + '>EUR</option>' +
            '</select>' +
            '<input class="cl-p" type="number" step="1" title="precision (decimals; negative rounds)" value="' +
            escAttr(String(p)) + '">' +
            '<input class="cl-t" type="number" step="1" min="0" title="trim: leading digits to cut (1.160, trim 2 -> 60)" value="' +
            escAttr(String(t)) + '">' +
            '<button type="button" class="cl-del" title="Remove">&#10005;</button>' +
            '</div>' +
            '<input class="cl-custom" type="text" placeholder="coingecko id" value="' +
            (isCustom ? escAttr(coin) : '') + '"' + custHidden + '>' +
            '<input class="cl-label" type="text" placeholder="label" maxlength="5" value="' +
            escAttr(labelVal) + '"' + custHidden + '>' +
            '</div>';
    }
    function defaultPFor(coinId) {
        for (var i = 0; i < COINS.length; i++) {
            if (COINS[i].id === coinId) {
                return COINS[i].p;
            }
        }
        return 2;
    }
    // The display symbol for a known coin (BTC, ETH...), or '' if not in the table.
    function symbolFor(coinId) {
        for (var i = 0; i < COINS.length; i++) {
            if (COINS[i].id === coinId) {
                return COINS[i].label;
            }
        }
        return '';
    }
    function rowCoinId(rowEl) {
        var sel = rowEl.querySelector('.cl-coin');
        if (sel && sel.value === 'custom') {
            var cust = rowEl.querySelector('.cl-custom');
            return cust ? cust.value : '';
        }
        return sel ? sel.value : '';
    }
    function currentRows() {
        var rows = [];
        var rowEls = root.querySelectorAll('.cl-row');
        for (var i = 0; i < rowEls.length; i++) {
            var el = rowEls[i];
            var vsSel = el.querySelector('.cl-vs');
            var pIn = el.querySelector('.cl-p');
            var tIn = el.querySelector('.cl-t');
            var labelIn = el.querySelector('.cl-label');
            var wid = parseInt(el.getAttribute('data-wid') || '0', 10) || 0;
            var p = parseInt(pIn ? pIn.value : '0', 10);
            var tv = parseInt(tIn ? tIn.value : '0', 10);
            if (isNaN(tv) || tv < 0) {
                tv = 0;
            }
            rows.push({
                wid: wid,
                coin: rowCoinId(el),
                vs: (vsSel && vsSel.value === 'eur') ? 'eur' : 'usd',
                p: isNaN(p) ? 0 : p,
                t: tv,
                label: labelIn ? labelIn.value : '',
            });
        }
        return rows;
    }
    function usedWids() {
        var out = [];
        var rowEls = root.querySelectorAll('.cl-row');
        for (var i = 0; i < rowEls.length; i++) {
            out.push(parseInt(rowEls[i].getAttribute('data-wid') || '0', 10) || 0);
        }
        return out;
    }
    function nextFreeWid() {
        var used = usedWids();
        for (var w = CRYPTO_BASE; w < CRYPTO_BASE + MAX_CRYPTO; w++) {
            if (used.indexOf(w) === -1) {
                return w;
            }
        }
        return CRYPTO_BASE;
    }
    function updateAddButton() {
        var rowEls = root.querySelectorAll('.cl-row');
        var add = root.querySelector('.cl-add');
        if (add) {
            add.style.display = (rowEls.length >= MAX_CRYPTO) ? 'none' : '';
        }
    }
    function rebuild(rows) {
        var list = root.querySelector('.cl-list');
        var html = '';
        for (var i = 0; i < rows.length && i < MAX_CRYPTO; i++) {
            html += rowHtml(rows[i]);
        }
        list.innerHTML = html;
        updateAddButton();
    }
    self._clCurrentRows = currentRows;
    self._clRebuild = rebuild;
    function rowIndexOf(node) {
        var rowEls = root.querySelectorAll('.cl-row');
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
        if (target.classList.contains('cl-add')) {
            var rows = currentRows();
            if (rows.length < MAX_CRYPTO) {
                rows.push({ wid: nextFreeWid(), coin: 'bitcoin', vs: 'usd', p: defaultPFor('bitcoin'), t: 0, label: '' });
                rebuild(rows);
                self.trigger('change');
            }
            return;
        }
        if (target.classList.contains('cl-del')) {
            var rowEl = (target.closest ? target.closest('.cl-row') : null);
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
        if (t.classList.contains('cl-coin')) {
            var rowEl = (t.closest ? t.closest('.cl-row') : null);
            if (rowEl) {
                var sel = t;
                var cust = rowEl.querySelector('.cl-custom');
                var pIn = rowEl.querySelector('.cl-p');
                var labelIn = rowEl.querySelector('.cl-label');
                if (sel.value === 'custom') {
                    // Custom: reveal both the gecko-id and label inputs (cleared to prompt).
                    if (cust) {
                        cust.style.display = '';
                        cust.value = '';
                    }
                    if (labelIn) {
                        labelIn.style.display = '';
                        labelIn.value = '';
                    }
                }
                else {
                    // Known coin: hide both; auto-fill label with the symbol, p with default.
                    if (cust) {
                        cust.style.display = 'none';
                    }
                    if (labelIn) {
                        labelIn.value = symbolFor(sel.value);
                        labelIn.style.display = 'none';
                    }
                    if (pIn) {
                        pIn.value = String(defaultPFor(sel.value));
                    }
                }
            }
        }
        self.trigger('change');
    });
}
var cryptoListComponent = {
    name: 'cryptoList',
    template: '<div class="cl-root">' +
        '<div class="cl-list"></div>' +
        '<button type="button" class="cl-add">+ Add crypto</button>' +
        '</div>',
    // Clay's base theme forces `button { min-width: 12rem }`; row buttons override
    // it. Native <select>/<input> render light by default; theme them to match
    // Clay's dark controls (gray-7 #767676, white text). One flex row per coin;
    // flex-wrap keeps it usable if a very narrow screen can't fit all controls.
    style: 
    // .cl-row is a flex-wrap container: the first line (coin/vs/p/✕) spans 100%,
    // so the custom-id + label inputs (only shown for a custom coin) wrap onto a
    // second line beneath it.
    '.cl-row{display:flex;flex-wrap:wrap;align-items:center;' +
        'border-bottom:1px solid #555;padding:6px 0;margin:0 0 6px 0}' +
        '.cl-line{flex:1 1 100%;display:flex;flex-wrap:wrap;align-items:center;margin:0}' +
        '.cl-row select,.cl-row input{min-width:0;height:2.6rem;margin:0 4px 0 0;' +
        'background-color:#767676;color:#fff;border:none;border-radius:0.3rem;' +
        'padding:0 0.4rem;color-scheme:dark}' +
        '.cl-row .cl-coin{flex:1 1 5rem}' +
        '.cl-row .cl-vs{flex:0 0 3.8rem}' +
        '.cl-row .cl-p{flex:0 0 3rem}' +
        '.cl-row .cl-t{flex:0 0 3rem}' +
        '.cl-row .cl-custom{flex:1 1 auto;margin-top:4px}' +
        '.cl-row .cl-label{flex:0 0 5rem;margin-top:4px}' +
        '.cl-row button{flex:0 0 auto;min-width:0;width:2.6rem;height:2.6rem;margin:0;padding:0}' +
        '.cl-add{margin:8px 0 10px 0}',
    manipulator: {
        get: function () {
            return this._clCurrentRows ? this._clCurrentRows() : [];
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
            if (this._clRebuild) {
                this._clRebuild(rows);
            }
            return this;
        },
    },
    defaults: { label: '' },
    initialize: cryptoListInitialize,
};
module.exports = cryptoListComponent;

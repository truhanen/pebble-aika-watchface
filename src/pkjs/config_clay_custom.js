"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
function clayConfigCustom(minified) {
    var clayConfig = this;
    var groups = [];
    var openIndex = -1;
    function widgetIds() {
        var ids = [];
        ['WidgetList', 'WidgetListRight'].forEach(function (k) {
            var v = clayConfig.getItemByMessageKey(k).get();
            if (v && v.length) {
                for (var i = 0; i < v.length; i++) {
                    ids.push((parseInt(v[i], 10) || 0) & 0xdf);
                }
            }
        });
        return ids;
    }
    function has(ids, set) {
        return set.some(function (v) { return ids.indexOf(v) !== -1; });
    }
    function key(k) { return clayConfig.getItemByMessageKey(k); }
    // ---- Gating: per-item visibility decisions (a key absent from gk/gi = visible)
    function computeGate() {
        var ids = widgetIds();
        var weather = has(ids, [7, 8, 13]);
        var temp = has(ids, [7, 8]);
        var manual = weather && key('weather_loc_mode').get() === 'manual';
        var cheapHour = has(ids, [18, 19]);
        var nextCheap = has(ids, [18]);
        var autoBattery = parseInt(key('SettingDisableAutobattery').get(), 10) === 0;
        var fallbackManual = parseInt(key('SettingFallbackColumn').get(), 10) !== 0;
        var analog = parseInt(key('SettingClockStyle').get(), 10) === 1;
        var belowDigi = key('SettingAnalogDigitalClock').get() === true;
        var bigDate = parseInt(key('SettingBigDate').get(), 10) === 1;
        var altClock = has(ids, [3]);
        // 0 == NIGHT_WINDOW_OFF: with no window, nothing below the mode select applies.
        var nightMode = parseInt(key('SettingNightRotationMode').get(), 10);
        var nightOn = nightMode !== 0;
        var nightCustom = nightMode === 2; // only Custom hours uses the two hour fields
        var nightColors = key('SettingNightColors').get() === true;
        // Either battery trigger switched on makes the battery colour relevant.
        var batWarn = parseInt(key('SettingBatteryWarnPct').get(), 10) > 0
            || parseInt(key('SettingBatteryWarnDays').get(), 10) > 0;
        var btWarn = key('SettingBtWarnBorder').get() === true;
        var gk = {};
        var gi = {};
        gk.SettingUseMetric = temp;
        gk.weather_loc_mode = weather;
        gk.weather_datasource = weather;
        gk.weather_loc = manual;
        gk.weather_loc_lat = manual;
        gk.weather_loc_lng = manual;
        gk.SettingElecQuietStart = cheapHour;
        gk.SettingElecQuietEnd = cheapHour;
        gk.SettingElecCheapFactorPct = nextCheap;
        gk.elec_cheap_floor = nextCheap;
        gk.elec_cheap_ceiling = nextCheap;
        gk.SettingAltClockName = altClock;
        gk.SettingAltClockOffset = altClock;
        gk.SettingShowBatteryPct = has(ids, [2]) || autoBattery;
        gk.SettingAutoBatteryThreshold = autoBattery;
        gk.SettingFallbackPosition = fallbackManual;
        gk.SettingAnalogTicks = analog;
        gk.SettingAnalogDigitalClock = analog;
        gk.SettingStatusClockDigital = analog && belowDigi;
        gk.SettingBigDateMonth = bigDate;
        gk.SettingBigDateFont = bigDate;
        gk.SettingNightRotationStart = nightCustom;
        gk.SettingNightRotationEnd = nightCustom;
        gk.SettingNightSlowRotation = nightOn;
        gk.SettingNightColors = nightOn;
        gk.SettingNightBgColor = nightOn && nightColors;
        gk.SettingNightFgColor = nightOn && nightColors;
        gk.SettingBatteryWarnColor = batWarn;
        gk.SettingBtWarnColor = btWarn;
        gi['analog-credit'] = analog;
        gi['heading-weather'] = weather;
        gi['heading-electricity'] = cheapHour;
        gi['night-rotation-help'] = nightOn;
        gi['night-help'] = nightOn;
        // NOTE: heading-crypto / heading-currency are deliberately NOT gated — each
        // section is the only place to create its widget type, so gating it on
        // placement deadlocks adding the first coin/pair (fb27e08 / 5553520 bug).
        return { gk: gk, gi: gi };
    }
    function gateVisible(it, g) {
        if (it.messageKey && g.gk.hasOwnProperty(it.messageKey)) {
            return g.gk[it.messageKey];
        }
        if (it.id && g.gi.hasOwnProperty(it.id)) {
            return g.gi[it.id];
        }
        return true;
    }
    // Toggle Clay's `hide` class on the item's root element directly, NOT the
    // manipulator's it.show()/it.hide(): those methods only exist on standard
    // components (whose string manipulator resolves to manipulators.js). Custom
    // components (widgetList/cryptoList/currencyList/tuyaCatalog/tuyaList) declare a
    // plain {get,set} manipulator with NO hide/show, so it.hide() would throw and
    // abort the whole applyVisibility pass at the first custom component (leaving
    // every later section stuck visible). $element + the `hide` class is exactly
    // what manipulators.js hide()/show() do, and every ClayItem has $element.
    function setShown(it, on) {
        if (on) {
            it.$element.set('-hide');
        }
        else {
            it.$element.set('+hide');
        }
    }
    // ---- Accordion: group items by heading, wire clicks, render chevrons
    function buildGroups() {
        groups = [];
        var items = clayConfig.getAllItems();
        var cur = null;
        for (var i = 0; i < items.length; i++) {
            var it = items[i];
            if (it.config.type === 'heading') {
                cur = { heading: it, items: [], label: String(it.config.defaultValue || '') };
                groups.push(cur);
            }
            else if (cur) {
                cur.items.push(it);
            }
        }
    }
    function wireHeadings() {
        groups.forEach(function (grp, idx) {
            grp.heading.$element.on('click', function () {
                openIndex = (openIndex === idx) ? -1 : idx;
                applyVisibility();
            });
        });
    }
    function setChevron(grp, isOpen) {
        var mark = isOpen ? '▾' : '▸'; // ▾ open, ▸ collapsed
        grp.heading.$manipulatorTarget.set('innerHTML', mark + ' ' + grp.label);
    }
    function applyVisibility() {
        var g = computeGate();
        // If the open section became gated-off (widget removed), treat it as closed.
        if (openIndex >= 0 && openIndex < groups.length &&
            !gateVisible(groups[openIndex].heading, g)) {
            openIndex = -1;
        }
        for (var gi = 0; gi < groups.length; gi++) {
            var grp = groups[gi];
            var headOk = gateVisible(grp.heading, g);
            setShown(grp.heading, headOk);
            if (headOk) {
                setChevron(grp, gi === openIndex);
            }
            var open_1 = headOk && (gi === openIndex);
            for (var j = 0; j < grp.items.length; j++) {
                var it = grp.items[j];
                if (it.config.type === 'submit') {
                    it.$element.set('-hide');
                    continue;
                }
                setShown(it, gateVisible(it, g) && open_1);
            }
        }
    }
    function injectFloatingSaveStyle() {
        if (typeof document === 'undefined') {
            return;
        }
        if (document.getElementById('ts-floating-save')) {
            return;
        }
        var style = document.createElement('style');
        style.id = 'ts-floating-save';
        // Reserve clearance on #main-form (the scrolling content), NOT body: Clay sets
        // html,body{height:100%}, so a body padding-bottom sits inside the fixed-height
        // body box and never clears the fixed Save bar (the 8.x last-setting bug).
        style.textContent =
            '.component-submit{position:fixed;bottom:0;left:0;right:0;margin:0;' +
                'z-index:100;background:#262626;padding:8px 0;' +
                'box-shadow:0 -2px 6px rgba(0,0,0,0.4);}' +
                '#main-form{padding-bottom:96px;}';
        document.head.appendChild(style);
    }
    function injectAccordionStyle() {
        if (typeof document === 'undefined') {
            return;
        }
        if (document.getElementById('ts-accordion')) {
            return;
        }
        var style = document.createElement('style');
        style.id = 'ts-accordion';
        style.textContent =
            '.component-heading{cursor:pointer;background:#2b2b2b;margin:0;' +
                'padding:8px;border-top:1px solid #000;}' +
                '.component-heading h1,.component-heading h2,.component-heading h3,' +
                '.component-heading h4,.component-heading h5{margin:0;}' +
                '.component-heading:active{background:#3a3a3a;}';
        document.head.appendChild(style);
    }
    // AFTER_BUILD fires once items are built with initial values (Clay 1.0.4 has no
    // AFTER_RENDER); getItem*/show/hide/$element are all valid by then.
    clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
        injectFloatingSaveStyle();
        injectAccordionStyle();
        buildGroups();
        openIndex = -1;
        wireHeadings();
        applyVisibility();
        ['WidgetList', 'WidgetListRight', 'weather_loc_mode', 'SettingDisableAutobattery',
            'SettingFallbackColumn', 'SettingClockStyle', 'SettingAnalogDigitalClock', 'SettingBigDate',
            'SettingNightRotationMode', 'SettingNightColors',
            'SettingBatteryWarnPct', 'SettingBatteryWarnDays', 'SettingBtWarnBorder']
            .forEach(function (k) { clayConfig.getItemByMessageKey(k).on('change', applyVisibility); });
    });
}
module.exports = clayConfigCustom;

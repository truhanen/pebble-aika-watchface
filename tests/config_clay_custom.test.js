// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

const test = require('node:test');
const assert = require('node:assert');
const clayConfigCustom = require('../src/pkjs/config_clay_custom');
const configArr = require('../src/pkjs/config_clay');

// Flatten the real config exactly like Clay's _addItems: sections are unwrapped
// (their items spliced in, in order); top-level items (the submit button) kept.
function flattenConfig(cfg) {
  const out = [];
  cfg.forEach((entry) => {
    if (entry.type === 'section' && Array.isArray(entry.items)) {
      entry.items.forEach((it) => out.push(it));
    } else {
      out.push(entry);
    }
  });
  return out;
}

// Minified-element stub: records class ops, click handlers and innerHTML sets.
function makeElement() {
  return {
    classes: {},
    clickHandlers: [],
    innerHTML: '',
    on(ev, fn) { if (ev === 'click') { this.clickHandlers.push(fn); } },
    set(spec, value) {
      if (spec === 'innerHTML') { this.innerHTML = value; return; }
      if (typeof spec === 'string' && spec.charAt(0) === '+') { this.classes[spec.slice(1)] = true; }
      else if (typeof spec === 'string' && spec.charAt(0) === '-') { delete this.classes[spec.slice(1)]; }
    }
  };
}

// Custom Clay components (config_widget_list.ts etc.) declare a plain {get,set}
// manipulator — Clay attaches ONLY those methods, so these items have NO
// hide()/show(). Standard components' string manipulator resolves to
// manipulators.js, which DOES include hide/show. Model that faithfully so the mock
// reproduces the "it.hide() throws on a custom component" bug rather than masking
// it: give hide/show only to non-custom items, and derive `.shown` from the `hide`
// class on $element (the real visibility mechanism), not a separate flag.
const CUSTOM_TYPES = {
  widgetList: 1, cryptoList: 1, currencyList: 1, tuyaCatalog: 1, tuyaList: 1
};

function makeItem(cfg) {
  const el = makeElement();
  const item = {
    config: cfg,
    id: cfg.id || null,
    messageKey: cfg.messageKey || null,
    value: cfg.defaultValue,
    changeHandlers: [],
    $element: el,
    $manipulatorTarget: el,
    get() { return this.value; },
    // Production uses item.on only for 'change'; heading clicks go via $element.on.
    on(ev, fn) { if (ev === 'click') { this.$element.clickHandlers.push(fn); } else { this.changeHandlers.push(fn); } }
  };
  // `.shown` reflects the actual `hide` class (what Clay's CSS keys visibility on).
  Object.defineProperty(item, 'shown', {
    enumerable: true,
    get() { return !el.classes.hide; }
  });
  if (!CUSTOM_TYPES[cfg.type]) {
    item.show = function() { el.set('-hide'); };
    item.hide = function() { el.set('+hide'); };
  }
  return item;
}

function makeDocument() {
  const head = { children: [], appendChild(el) { this.children.push(el); } };
  return {
    head: head,
    getElementById(id) {
      for (let i = 0; i < head.children.length; i++) {
        if (head.children[i].id === id) { return head.children[i]; }
      }
      return null;
    },
    createElement(tag) { return { tagName: tag, id: '', textContent: '' }; }
  };
}

// widgetVals: left-list widget IDs. opts: {locMode, autoBatteryDisabled, rightVals,
// fallbackColumn, clockStyle, bigDate}
function makeClay(widgetVals, opts) {
  opts = opts || {};
  const items = flattenConfig(configArr).map(makeItem);
  const byKey = {};
  const byId = {};
  items.forEach((it) => {
    if (it.messageKey) { byKey[it.messageKey] = it; }
    if (it.id) { byId[it.id] = it; }
  });
  byKey['WidgetList'].value = (widgetVals || []).map((v) => parseInt(v, 10) || 0);
  byKey['WidgetListRight'].value = (opts.rightVals || []).map((v) => parseInt(v, 10) || 0);
  byKey['weather_loc_mode'].value = opts.locMode || 'auto';
  byKey['SettingDisableAutobattery'].value = String(opts.autoBatteryDisabled ? 1 : 0);
  byKey['SettingFallbackColumn'].value = String(opts.fallbackColumn !== undefined ? opts.fallbackColumn : 0);
  byKey['SettingClockStyle'].value = String(opts.clockStyle !== undefined ? opts.clockStyle : 0);
  byKey['SettingBigDate'].value = String(opts.bigDate !== undefined ? opts.bigDate : 1);
  byKey['SettingAnalogDigitalClock'].value = opts.analogDigital !== undefined ? opts.analogDigital : false;
  byKey['SettingBatteryWarnPct'].value = String(opts.batteryWarnPct !== undefined ? opts.batteryWarnPct : 0);
  byKey['SettingBatteryWarnDays'].value = String(opts.batteryWarnDays !== undefined ? opts.batteryWarnDays : 0);
  byKey['SettingBtWarnBorder'].value = opts.btWarnBorder !== undefined ? opts.btWarnBorder : false;
  byKey['SettingNightRotationMode'].value = String(opts.nightMode !== undefined ? opts.nightMode : 0);
  byKey['SettingNightColors'].value = opts.nightColors !== undefined ? opts.nightColors : false;
  return {
    // Mirror Clay 1.0.4's real event set. There is NO AFTER_RENDER — a missing
    // constant passes undefined to on(), which Clay's _transformEventNames
    // crashes on (undefined.split). The on() below reproduces that crash.
    EVENTS: { BEFORE_BUILD: 'BEFORE_BUILD', AFTER_BUILD: 'AFTER_BUILD',
              BEFORE_DESTROY: 'BEFORE_DESTROY', AFTER_DESTROY: 'AFTER_DESTROY' },
    _handlers: {},
    getItemByMessageKey(k) { return byKey[k]; },
    getItemById(i) { return byId[i]; },
    getAllItems() { return items; },
    on(ev, fn) {
      if (typeof ev !== 'string') {
        throw new TypeError("Cannot read properties of undefined (reading 'split')");
      }
      this._handlers[ev] = fn;
    },
    byKey,
    byId,
    items
  };
}

function render(widgetVals, opts) {
  const clay = makeClay(widgetVals, opts);
  clayConfigCustom.call(clay, {});  // minified arg unused by logic
  assert.ok(clay._handlers.AFTER_BUILD,
    'custom fn must register an AFTER_BUILD handler');
  clay._handlers.AFTER_BUILD();
  return clay;
}

// Open the accordion section that owns `keyOrId` by firing its heading's click
// handlers (as the webview would on a tap). Walks items tracking the last heading
// seen, stops at the target. No-op if the target/heading isn't found.
function openSectionFor(c, keyOrId) {
  const items = c.getAllItems();
  let head = null;
  for (let i = 0; i < items.length; i++) {
    const it = items[i];
    if (it.config.type === 'heading') { head = it; }
    if (it.messageKey === keyOrId || it.id === keyOrId) { break; }
  }
  if (head) { head.$element.clickHandlers.forEach((fn) => fn()); }
  return head;
}

test('no widgets: weather/electricity/alt hidden; battery style shown (auto-battery on)', () => {
  const c = render([]);
  openSectionFor(c, 'SettingShowBatteryPct');
  assert.strictEqual(c.byId['heading-weather'].shown, false);
  assert.strictEqual(c.byKey['SettingUseMetric'].shown, false);
  assert.strictEqual(c.byKey['weather_loc_mode'].shown, false);
  assert.strictEqual(c.byId['heading-electricity'].shown, false);
  assert.strictEqual(c.byKey['SettingElecQuietStart'].shown, false);
  assert.strictEqual(c.byKey['SettingElecCheapFactorPct'].shown, false);
  assert.strictEqual(c.byKey['SettingAltClockName'].shown, false);
  assert.strictEqual(c.byKey['SettingShowBatteryPct'].shown, true);
});

test('weather temp widget (7): weather shown incl units; manual fields hidden in auto mode', () => {
  const c = render([7]);
  openSectionFor(c, 'SettingUseMetric');
  assert.strictEqual(c.byId['heading-weather'].shown, true);
  assert.strictEqual(c.byKey['weather_loc_mode'].shown, true);
  assert.strictEqual(c.byKey['weather_datasource'].shown, true);
  assert.strictEqual(c.byKey['SettingUseMetric'].shown, true);
  assert.strictEqual(c.byKey['weather_loc'].shown, false);
  assert.strictEqual(c.byKey['weather_loc_lat'].shown, false);
  assert.strictEqual(c.byKey['weather_loc_lng'].shown, false);
});

test('UV index only (13): weather shown but units hidden (UV is unitless)', () => {
  const c = render([13]);
  openSectionFor(c, 'SettingUseMetric');
  assert.strictEqual(c.byId['heading-weather'].shown, true);
  assert.strictEqual(c.byKey['weather_loc_mode'].shown, true);
  assert.strictEqual(c.byKey['SettingUseMetric'].shown, false);
});

test('hidden-identifier flag (0x20) still gates the widget\'s settings', () => {
  const c = render([7 | 0x20]);   // hidden weather is still weather
  openSectionFor(c, 'SettingUseMetric');
  assert.strictEqual(c.byId['heading-weather'].shown, true);
  assert.strictEqual(c.byKey['SettingUseMetric'].shown, true);
});

test('weather + manual location: manual fields shown', () => {
  const c = render([8], { locMode: 'manual' });
  openSectionFor(c, 'weather_loc');
  assert.strictEqual(c.byKey['weather_loc'].shown, true);
  assert.strictEqual(c.byKey['weather_loc_lat'].shown, true);
  assert.strictEqual(c.byKey['weather_loc_lng'].shown, true);
});

test('current-price electricity only (14): whole electricity section hidden', () => {
  const c = render([14]);
  assert.strictEqual(c.byId['heading-electricity'].shown, false);
  assert.strictEqual(c.byKey['SettingElecQuietStart'].shown, false);
  assert.strictEqual(c.byKey['SettingElecCheapFactorPct'].shown, false);
});

test('cheapest-hour (19): heading + quiet hours shown; cheap factor/floor/ceiling hidden', () => {
  const c = render([19]);
  openSectionFor(c, 'SettingElecQuietStart');
  assert.strictEqual(c.byId['heading-electricity'].shown, true);
  assert.strictEqual(c.byKey['SettingElecQuietStart'].shown, true);
  assert.strictEqual(c.byKey['SettingElecQuietEnd'].shown, true);
  assert.strictEqual(c.byKey['SettingElecCheapFactorPct'].shown, false);
  assert.strictEqual(c.byKey['elec_cheap_floor'].shown, false);
  assert.strictEqual(c.byKey['elec_cheap_ceiling'].shown, false);
});

test('next-cheap (18): quiet hours + cheap factor/floor/ceiling all shown', () => {
  const c = render([18]);
  openSectionFor(c, 'SettingElecQuietStart');
  assert.strictEqual(c.byId['heading-electricity'].shown, true);
  assert.strictEqual(c.byKey['SettingElecQuietStart'].shown, true);
  assert.strictEqual(c.byKey['SettingElecCheapFactorPct'].shown, true);
  assert.strictEqual(c.byKey['elec_cheap_floor'].shown, true);
  assert.strictEqual(c.byKey['elec_cheap_ceiling'].shown, true);
});

test('alt time zone widget (3): alt clock name + offset shown', () => {
  const c = render([3]);
  openSectionFor(c, 'SettingAltClockName');
  assert.strictEqual(c.byKey['SettingAltClockName'].shown, true);
  assert.strictEqual(c.byKey['SettingAltClockOffset'].shown, true);
});

test('battery style: shown with battery widget even if auto-battery off', () => {
  const c = render([2], { autoBatteryDisabled: true });
  openSectionFor(c, 'SettingShowBatteryPct');
  assert.strictEqual(c.byKey['SettingShowBatteryPct'].shown, true);
});

test('battery style: hidden when no battery widget AND auto-battery off', () => {
  const c = render([], { autoBatteryDisabled: true });
  openSectionFor(c, 'SettingShowBatteryPct');
  assert.strictEqual(c.byKey['SettingShowBatteryPct'].shown, false);
});

test('live change: selecting a weather widget reveals weather after re-render', () => {
  const c = render([]);
  assert.strictEqual(c.byId['heading-weather'].shown, false);
  c.byKey['WidgetList'].value = [7];
  c.byKey['WidgetList'].changeHandlers.forEach((fn) => fn());
  openSectionFor(c, 'SettingUseMetric');
  assert.strictEqual(c.byId['heading-weather'].shown, true);
  assert.strictEqual(c.byKey['SettingUseMetric'].shown, true);
});

test('live change: switching weather location to manual reveals lat/lng/label', () => {
  const c = render([7]);  // weather present, auto mode -> manual fields hidden
  assert.strictEqual(c.byKey['weather_loc'].shown, false);
  c.byKey['weather_loc_mode'].value = 'manual';
  c.byKey['weather_loc_mode'].changeHandlers.forEach((fn) => fn());
  openSectionFor(c, 'weather_loc');
  assert.strictEqual(c.byKey['weather_loc'].shown, true);
  assert.strictEqual(c.byKey['weather_loc_lat'].shown, true);
  assert.strictEqual(c.byKey['weather_loc_lng'].shown, true);
});

test('live change: disabling auto-battery hides battery style when no battery widget', () => {
  const c = render([]);  // no widgets, auto-battery on -> style shown
  openSectionFor(c, 'SettingShowBatteryPct');
  assert.strictEqual(c.byKey['SettingShowBatteryPct'].shown, true);
  c.byKey['SettingDisableAutobattery'].value = '1';
  c.byKey['SettingDisableAutobattery'].changeHandlers.forEach((fn) => fn());
  assert.strictEqual(c.byKey['SettingShowBatteryPct'].shown, false);
});

test('right-list weather widget reveals weather section', () => {
  const c = render([], { rightVals: [7] });
  openSectionFor(c, 'SettingUseMetric');
  assert.strictEqual(c.byId['heading-weather'].shown, true);
  assert.strictEqual(c.byKey['SettingUseMetric'].shown, true);
});

test('live change: adding a weather widget to the right list reveals weather', () => {
  const c = render([]);
  assert.strictEqual(c.byId['heading-weather'].shown, false);
  c.byKey['WidgetListRight'].value = [7];
  c.byKey['WidgetListRight'].changeHandlers.forEach((fn) => fn());
  assert.strictEqual(c.byId['heading-weather'].shown, true);
});

test('battery threshold: shown when auto-battery on, hidden when off', () => {
  const on = render([]);
  openSectionFor(on, 'SettingAutoBatteryThreshold');
  assert.strictEqual(on.byKey['SettingAutoBatteryThreshold'].shown, true);
  const off = render([], { autoBatteryDisabled: true });
  openSectionFor(off, 'SettingAutoBatteryThreshold');
  assert.strictEqual(off.byKey['SettingAutoBatteryThreshold'].shown, false);
});

test('fallback position: hidden in Automatic column, shown for Left/Right', () => {
  const auto = render([], { fallbackColumn: 0 });
  openSectionFor(auto, 'SettingFallbackPosition');
  assert.strictEqual(auto.byKey['SettingFallbackPosition'].shown, false);
  const left = render([], { fallbackColumn: 1 });
  openSectionFor(left, 'SettingFallbackPosition');
  assert.strictEqual(left.byKey['SettingFallbackPosition'].shown, true);
  const right = render([], { fallbackColumn: 2 });
  openSectionFor(right, 'SettingFallbackPosition');
  assert.strictEqual(right.byKey['SettingFallbackPosition'].shown, true);
});

test('live change: choosing a fallback column reveals the position input', () => {
  const c = render([], { fallbackColumn: 0 });
  openSectionFor(c, 'SettingFallbackPosition');
  assert.strictEqual(c.byKey['SettingFallbackPosition'].shown, false);
  c.byKey['SettingFallbackColumn'].value = '1';
  c.byKey['SettingFallbackColumn'].changeHandlers.forEach((fn) => fn());
  assert.strictEqual(c.byKey['SettingFallbackPosition'].shown, true);
});

test('live change: disabling auto-battery hides the threshold chooser', () => {
  const c = render([]);
  openSectionFor(c, 'SettingAutoBatteryThreshold');
  assert.strictEqual(c.byKey['SettingAutoBatteryThreshold'].shown, true);
  c.byKey['SettingDisableAutobattery'].value = '1';
  c.byKey['SettingDisableAutobattery'].changeHandlers.forEach((fn) => fn());
  assert.strictEqual(c.byKey['SettingAutoBatteryThreshold'].shown, false);
});

test('floating save: AFTER_BUILD injects a fixed-position style once (idempotent)', () => {
  const prev = global.document;
  global.document = makeDocument();
  try {
    const c = render([]);   // render() fires AFTER_BUILD once
    const styles = global.document.head.children;
    assert.strictEqual(styles.length, 2, 'two <style>s injected (save + accordion)');
    const save = styles.filter((s) => s.id === 'ts-floating-save')[0];
    const acc = styles.filter((s) => s.id === 'ts-accordion')[0];
    assert.ok(save, 'floating-save style present');
    assert.ok(acc, 'accordion style present');
    assert.match(save.textContent, /\.component-submit/);
    assert.match(save.textContent, /position\s*:\s*fixed/);
    // Clearance reserved on the scrolling form (#main-form), NOT body (Clay sets
    // html,body{height:100%}, which swallows a body padding-bottom — the 8.x bug).
    assert.match(save.textContent, /#main-form\s*\{[^}]*padding-bottom/);
    assert.doesNotMatch(save.textContent, /(^|[^-])\bbody\s*\{[^}]*padding-bottom/,
      'must NOT reserve clearance on body (height:100% swallows it)');
    assert.match(acc.textContent, /\.component-heading\s*\{[^}]*cursor\s*:\s*pointer/);
    // Firing AFTER_BUILD again must not add duplicates.
    c._handlers.AFTER_BUILD();
    assert.strictEqual(global.document.head.children.length, 2, 'no duplicate <style>');
  } finally {
    global.document = prev;
  }
});

test('digital clock style: analog rows hidden', () => {
  const c = render([], { clockStyle: 0 });
  openSectionFor(c, 'SettingClockStyle');
  assert.strictEqual(c.byKey['SettingAnalogDigitalClock'].shown, false);
  assert.strictEqual(c.byKey['SettingAnalogTicks'].shown, false);
  assert.strictEqual(c.byId['analog-credit'].shown, false);
});

test('analog clock style: analog rows shown', () => {
  const c = render([], { clockStyle: 1 });
  openSectionFor(c, 'SettingClockStyle');
  assert.strictEqual(c.byKey['SettingAnalogDigitalClock'].shown, true);
  assert.strictEqual(c.byKey['SettingAnalogTicks'].shown, true);
  assert.strictEqual(c.byId['analog-credit'].shown, true);
});

test('live change: switching to analog reveals the analog rows', () => {
  const c = render([], { clockStyle: 0 });
  openSectionFor(c, 'SettingClockStyle');
  assert.strictEqual(c.byKey['SettingAnalogDigitalClock'].shown, false);
  c.byKey['SettingClockStyle'].value = '1';
  c.byKey['SettingClockStyle'].changeHandlers.forEach((fn) => fn());
  assert.strictEqual(c.byKey['SettingAnalogDigitalClock'].shown, true);
  assert.strictEqual(c.byKey['SettingAnalogTicks'].shown, true);
});

test('status digital toggle hidden in digital clock mode', () => {
  const c = render([], { clockStyle: 0, analogDigital: true });
  openSectionFor(c, 'SettingStatusClockDigital');
  assert.strictEqual(c.byKey['SettingStatusClockDigital'].shown, false);
});

test('status digital toggle hidden in analog mode when below-digi is off', () => {
  const c = render([], { clockStyle: 1, analogDigital: false });
  openSectionFor(c, 'SettingStatusClockDigital');
  assert.strictEqual(c.byKey['SettingStatusClockDigital'].shown, false);
});

test('status digital toggle shown in analog mode when below-digi is on', () => {
  const c = render([], { clockStyle: 1, analogDigital: true });
  openSectionFor(c, 'SettingStatusClockDigital');
  assert.strictEqual(c.byKey['SettingStatusClockDigital'].shown, true);
});

test('live change: turning below-digi on reveals the status digital toggle', () => {
  const c = render([], { clockStyle: 1, analogDigital: false });
  openSectionFor(c, 'SettingStatusClockDigital');
  assert.strictEqual(c.byKey['SettingStatusClockDigital'].shown, false);
  c.byKey['SettingAnalogDigitalClock'].value = true;
  c.byKey['SettingAnalogDigitalClock'].changeHandlers.forEach((fn) => fn());
  assert.strictEqual(c.byKey['SettingStatusClockDigital'].shown, true);
});

test('big date shown: month toggle visible', () => {
  const c = render([], { bigDate: 1 });
  openSectionFor(c, 'SettingBigDateMonth');
  assert.strictEqual(c.byKey['SettingBigDateMonth'].shown, true);
});

test('big date none: month toggle hidden', () => {
  const c = render([], { bigDate: 0 });
  openSectionFor(c, 'SettingBigDateMonth');
  assert.strictEqual(c.byKey['SettingBigDateMonth'].shown, false);
});

// Regression guard: the Crypto and Currency section headings must ALWAYS be
// visible, because each section is the ONLY place to create its widget type.
// Gating them on placement (fb27e08 / 5553520) deadlocked adding the first
// coin/pair — the widget only appears in the picker after a row exists, but a
// row can only be added inside the (then-hidden) section.
test('currency heading always shown (only place to add a pair)', () => {
  assert.strictEqual(render([]).byId['heading-currency'].shown, true);
  assert.strictEqual(render([7]).byId['heading-currency'].shown, true);   // weather only
  assert.strictEqual(render([216]).byId['heading-currency'].shown, true); // pair placed
});

test('crypto heading always shown (only place to add a coin)', () => {
  assert.strictEqual(render([]).byId['heading-crypto'].shown, true);
  assert.strictEqual(render([7]).byId['heading-crypto'].shown, true);
});

test('live change: turning the big date off hides the month toggle', () => {
  const c = render([], { bigDate: 1 });
  openSectionFor(c, 'SettingBigDateMonth');
  assert.strictEqual(c.byKey['SettingBigDateMonth'].shown, true);
  c.byKey['SettingBigDate'].value = '0';
  c.byKey['SettingBigDate'].changeHandlers.forEach((fn) => fn());
  assert.strictEqual(c.byKey['SettingBigDateMonth'].shown, false);
});

test('big date shown: font picker visible', () => {
  const c = render([], { bigDate: 1 });
  openSectionFor(c, 'SettingBigDateFont');
  assert.strictEqual(c.byKey['SettingBigDateFont'].shown, true);
});

test('big date none: font picker hidden', () => {
  const c = render([], { bigDate: 0 });
  openSectionFor(c, 'SettingBigDateFont');
  assert.strictEqual(c.byKey['SettingBigDateFont'].shown, false);
});

// ---------------------------------------------------------------- Accordion
test('accordion: all section items collapsed on open; non-gated headings shown', () => {
  const c = render([7]);   // weather widget -> weather heading gated-visible
  // Non-gated headings visible (find the Clock heading = first heading).
  const items = c.getAllItems();
  const firstHeading = items.filter((it) => it.config.type === 'heading')[0];
  assert.strictEqual(firstHeading.shown, true, 'first heading (Clock) visible');
  assert.strictEqual(c.byId['heading-weather'].shown, true, 'gated-on heading visible');
  // Every non-heading, non-submit item starts hidden (nothing open).
  items.forEach((it) => {
    if (it.config.type !== 'heading' && it.config.type !== 'submit') {
      assert.strictEqual(it.shown, false, 'item hidden while its section is collapsed: ' + (it.messageKey || it.id || it.config.type));
    }
  });
});

test('accordion: submit (Save) stays visible while collapsed', () => {
  const c = render([]);
  const submit = c.getAllItems().filter((it) => it.config.type === 'submit')[0];
  assert.ok(submit, 'submit item exists');
  assert.strictEqual(submit.shown, true);
});

test('accordion: clicking a heading opens only that section', () => {
  const c = render([7]);
  openSectionFor(c, 'SettingUseMetric');   // opens Weather
  assert.strictEqual(c.byKey['SettingUseMetric'].shown, true, 'opened section item shown');
  assert.strictEqual(c.byKey['SettingClockStyle'].shown, false, 'other section stays collapsed');
});

test('accordion: opening a second section closes the first (one open at a time)', () => {
  const c = render([7]);
  openSectionFor(c, 'SettingUseMetric');    // Weather open
  assert.strictEqual(c.byKey['SettingUseMetric'].shown, true);
  openSectionFor(c, 'SettingClockStyle');   // Clock open -> Weather closes
  assert.strictEqual(c.byKey['SettingClockStyle'].shown, true);
  assert.strictEqual(c.byKey['SettingUseMetric'].shown, false, 'previously-open section closed');
});

test('accordion: clicking an open heading again collapses it', () => {
  const c = render([7]);
  openSectionFor(c, 'SettingUseMetric');   // open
  assert.strictEqual(c.byKey['SettingUseMetric'].shown, true);
  openSectionFor(c, 'SettingUseMetric');   // toggle closed
  assert.strictEqual(c.byKey['SettingUseMetric'].shown, false);
});

test('accordion: combined gate+open — Weather open with UV-only keeps units hidden', () => {
  const c = render([13]);                  // UV widget: weather on, units off
  openSectionFor(c, 'SettingUseMetric');
  assert.strictEqual(c.byId['heading-weather'].shown, true);
  assert.strictEqual(c.byKey['weather_loc_mode'].shown, true, 'gate-on item shown when open');
  assert.strictEqual(c.byKey['SettingUseMetric'].shown, false, 'gate-off item hidden even when section open');
});

test('accordion: gated-off section cannot be opened (heading hidden, items stay hidden)', () => {
  const c = render([]);                    // no widgets -> weather gated off
  assert.strictEqual(c.byId['heading-weather'].shown, false, 'gated-off heading hidden');
  openSectionFor(c, 'SettingUseMetric');   // tries to open weather
  assert.strictEqual(c.byKey['weather_loc_mode'].shown, false, 'items stay hidden for a gated-off section');
});

test('accordion: chevron flips ▸ -> ▾ when a section opens', () => {
  const c = render([7]);
  const w = c.byId['heading-weather'];
  openSectionFor(c, 'SettingClockStyle');  // Weather collapsed
  assert.match(w.$manipulatorTarget.innerHTML, /▸/, 'collapsed shows ▸');
  openSectionFor(c, 'SettingUseMetric');   // Weather open
  assert.match(w.$manipulatorTarget.innerHTML, /▾/, 'open shows ▾');
});

test('accordion: Sidebar widgets is a single group (15 rows, not split by sub-labels)', () => {
  const c = render([]);
  const headings = c.getAllItems().filter((it) => it.config.type === 'heading');
  assert.strictEqual(headings.length, 15, 'exactly 15 accordion heading rows (15 sections 1:1)');
  const head = openSectionFor(c, 'SettingShowBatteryPct');
  assert.strictEqual(head.config.defaultValue, 'Sidebar widgets',
    'battery setting opens under the "Sidebar widgets" heading, not a sub-label');
  assert.strictEqual(c.byKey['SettingShowBatteryPct'].shown, true);
  assert.strictEqual(c.byKey['SettingFallbackColumn'].shown, true);
  assert.strictEqual(c.byKey['WidgetListRight'].shown, true);
});

test('accordion: removing an open section\'s gating widget closes it; re-adding reopens collapsed', () => {
  const c = render([7]);                    // weather widget present
  openSectionFor(c, 'SettingUseMetric');    // open Weather
  assert.strictEqual(c.byKey['SettingUseMetric'].shown, true);
  c.byKey['WidgetList'].value = [];         // remove weather widget
  c.byKey['WidgetList'].changeHandlers.forEach((fn) => fn());
  assert.strictEqual(c.byId['heading-weather'].shown, false, 'heading hidden when gated off');
  assert.strictEqual(c.byKey['SettingUseMetric'].shown, false, 'items hidden when force-closed');
  c.byKey['WidgetList'].value = [7];        // re-add weather widget
  c.byKey['WidgetList'].changeHandlers.forEach((fn) => fn());
  assert.strictEqual(c.byId['heading-weather'].shown, true, 'heading shown again');
  assert.strictEqual(c.byKey['SettingUseMetric'].shown, false, 'came back collapsed, not auto-opened');
});

test('accordion: custom-component sections collapse (widgetList/cryptoList/... have no hide/show manipulator)', () => {
  const c = render([]);
  // Sidebar widgets contains widgetList — a custom component whose manipulator is a
  // bare {get,set} with no hide/show. The old code called it.hide() and threw here,
  // aborting applyVisibility so every section after Sidebar widgets stayed visible.
  openSectionFor(c, 'SettingClockStyle');   // open Clock -> all other sections collapsed
  assert.strictEqual(c.byKey['WidgetList'].shown, false, 'widgetList hidden when collapsed');
  assert.strictEqual(c.byKey['WidgetListRight'].shown, false);
  assert.strictEqual(c.byKey['CryptoList'].shown, false);
  assert.strictEqual(c.byKey['CurrencyList'].shown, false);
  assert.strictEqual(c.byKey['TuyaList'].shown, false);
  // Sections AFTER the first custom component must also collapse (the abort left
  // Weather..Data Refresh stuck visible).
  assert.strictEqual(c.byKey['weather_datasource'].shown, false, 'Weather collapses (after widgetList)');
  assert.strictEqual(c.byKey['SettingMidiVibe'].shown, false, 'MIDI collapses');
  assert.strictEqual(c.byKey['SettingPollIntervalMin'].shown, false, 'Data Refresh collapses');
});

// ----------------------------------------------------------- Warning border
test('warning-frame colours are hidden until their trigger is on', () => {
  // Both triggers Off, BT border off -> neither colour is shown.
  const off = render([], { batteryWarnPct: 0, batteryWarnDays: 0, btWarnBorder: false });
  openSectionFor(off, 'SettingBatteryWarnColor');
  assert.strictEqual(off.byKey['SettingBatteryWarnColor'].shown, false);
  assert.strictEqual(off.byKey['SettingBtWarnColor'].shown, false);

  // Either battery trigger alone reveals the battery colour.
  const pctOn = render([], { batteryWarnPct: 20, batteryWarnDays: 0, btWarnBorder: false });
  openSectionFor(pctOn, 'SettingBatteryWarnColor');
  assert.strictEqual(pctOn.byKey['SettingBatteryWarnColor'].shown, true);
  const daysOn = render([], { batteryWarnPct: 0, batteryWarnDays: 10, btWarnBorder: false });
  openSectionFor(daysOn, 'SettingBatteryWarnColor');
  assert.strictEqual(daysOn.byKey['SettingBatteryWarnColor'].shown, true);

  // The toggle reveals the disconnect colour.
  const btOn = render([], { batteryWarnPct: 0, batteryWarnDays: 0, btWarnBorder: true });
  openSectionFor(btOn, 'SettingBtWarnColor');
  assert.strictEqual(btOn.byKey['SettingBtWarnColor'].shown, true);
});

test('live change: turning on a battery trigger reveals the battery colour without reopening the page', () => {
  const c = render([], { batteryWarnPct: 0, batteryWarnDays: 0, btWarnBorder: false });
  openSectionFor(c, 'SettingBatteryWarnColor');
  assert.strictEqual(c.byKey['SettingBatteryWarnColor'].shown, false);
  c.byKey['SettingBatteryWarnPct'].value = '20';
  c.byKey['SettingBatteryWarnPct'].changeHandlers.forEach((fn) => fn());
  assert.strictEqual(c.byKey['SettingBatteryWarnColor'].shown, true);
});

test('live change: enabling the disconnect toggle reveals the disconnect colour', () => {
  const c = render([], { btWarnBorder: false });
  openSectionFor(c, 'SettingBtWarnColor');
  assert.strictEqual(c.byKey['SettingBtWarnColor'].shown, false);
  c.byKey['SettingBtWarnBorder'].value = true;
  c.byKey['SettingBtWarnBorder'].changeHandlers.forEach((fn) => fn());
  assert.strictEqual(c.byKey['SettingBtWarnColor'].shown, true);
});

// ----------------------------------------------------------------- Night
test('night items are gated on the window and the colours toggle', () => {
  // Window off -> everything below the mode select is hidden.
  let c = render([], { nightMode: 0, nightColors: false });
  openSectionFor(c, 'SettingNightSlowRotation');
  assert.strictEqual(c.byKey['SettingNightSlowRotation'].shown, false);
  assert.strictEqual(c.byKey['SettingNightColors'].shown, false);
  assert.strictEqual(c.byKey['SettingNightBgColor'].shown, false);
  assert.strictEqual(c.byKey['SettingNightRotationStart'].shown, false);

  // Follow Quiet Time -> the consumers show, the hour fields do not.
  c = render([], { nightMode: 1, nightColors: false });
  openSectionFor(c, 'SettingNightSlowRotation');
  assert.strictEqual(c.byKey['SettingNightSlowRotation'].shown, true);
  assert.strictEqual(c.byKey['SettingNightColors'].shown, true);
  assert.strictEqual(c.byKey['SettingNightRotationStart'].shown, false);
  assert.strictEqual(c.byKey['SettingNightBgColor'].shown, false);

  // Custom hours + colours on -> everything shows.
  c = render([], { nightMode: 2, nightColors: true });
  openSectionFor(c, 'SettingNightSlowRotation');
  assert.strictEqual(c.byKey['SettingNightRotationStart'].shown, true);
  assert.strictEqual(c.byKey['SettingNightRotationEnd'].shown, true);
  assert.strictEqual(c.byKey['SettingNightBgColor'].shown, true);
  assert.strictEqual(c.byKey['SettingNightFgColor'].shown, true);
});

test('live change: turning on the night window reveals the consumers without reopening the page', () => {
  const c = render([], { nightMode: 0 });
  openSectionFor(c, 'SettingNightSlowRotation');
  assert.strictEqual(c.byKey['SettingNightSlowRotation'].shown, false);
  c.byKey['SettingNightRotationMode'].value = '1';
  c.byKey['SettingNightRotationMode'].changeHandlers.forEach((fn) => fn());
  assert.strictEqual(c.byKey['SettingNightSlowRotation'].shown, true);
  assert.strictEqual(c.byKey['SettingNightColors'].shown, true);
});

test('live change: turning on night colours reveals the palette pickers', () => {
  const c = render([], { nightMode: 1, nightColors: false });
  openSectionFor(c, 'SettingNightColors');
  assert.strictEqual(c.byKey['SettingNightBgColor'].shown, false);
  c.byKey['SettingNightColors'].value = true;
  c.byKey['SettingNightColors'].changeHandlers.forEach((fn) => fn());
  assert.strictEqual(c.byKey['SettingNightBgColor'].shown, true);
  assert.strictEqual(c.byKey['SettingNightFgColor'].shown, true);
});

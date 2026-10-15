// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Clay-native configuration for the Nyquist watchface. Replaces the hosted
   Jekyll HTML config page. Every watch `messageKey` matches package.json's
   messageKeys list; every option `value` matches the integer/string the C side
   expects (cross-checked against the historical index.js webviewclosed mapping).

   Phone-only settings (weather location / data source, decimal electricity
   thresholds) use NON-watch messageKey labels and are handled entirely in the
   index.js webviewclosed handler (written to localStorage; never sent to the
   watch). */

interface ClayOption {
  label: string;
  value: number | string;
}

// Language options. Mirrors docs/_includes/config_common_options.html:349-387.
const LANGUAGES: ClayOption[] = [
  { label: 'English (Default)', value: 0 },
  { label: 'Bahasa Indonesia', value: 30 },
  { label: 'Catala', value: 15 },
  { label: 'Cestina', value: 7 },
  { label: 'Cymraeg', value: 32 },
  { label: 'Dansk', value: 21 },
  { label: 'Deutsch', value: 2 },
  { label: 'Eesti', value: 18 },
  { label: 'Espanol', value: 3 },
  { label: 'Euskara', value: 19 },
  { label: 'Francais', value: 1 },
  { label: 'Gaeilge', value: 26 },
  { label: 'Galego', value: 33 },
  { label: 'Hrvatski', value: 25 },
  { label: 'Italiano', value: 4 },
  { label: 'Latviesu', value: 27 },
  { label: 'Lietuviskai', value: 22 },
  { label: 'Magyar', value: 24 },
  { label: 'Nederlands', value: 5 },
  { label: 'Norsk', value: 16 },
  { label: 'Romana', value: 14 },
  { label: 'Polski', value: 11 },
  { label: 'Portugues', value: 8 },
  { label: 'Slovencina', value: 12 },
  { label: 'Slovenscina', value: 23 },
  { label: 'Suomi', value: 20 },
  { label: 'Srpski', value: 28 },
  { label: 'Svenska', value: 10 },
  { label: 'Tieng Viet', value: 13 },
  { label: 'Turkce', value: 6 },
  { label: 'Ellinika (requires language pack)', value: 9 },
  { label: 'Russkiy (requires language pack)', value: 17 },
  { label: 'Ukrayinska (requires language pack)', value: 31 },
  { label: 'Zhongwen (requires language pack)', value: 29 },
  { label: 'Nihongo (requires language pack)', value: 34 },
  { label: 'Hangugeo (requires language pack)', value: 35 },
  { label: 'Ivrit (requires language pack)', value: 36 },
];

// Alternate-clock timezone offsets. Mirrors config_common_options.html:127-164.
// Commented-out half-hour zones are intentionally omitted.
const ALT_CLOCK_OFFSETS: ClayOption[] = [
  { label: 'UTC-12:00 (Baker Island)', value: -12 },
  { label: 'UTC-11:00 (American Samoa)', value: -11 },
  { label: 'UTC-10:00 (Hawaii, Tahiti)', value: -10 },
  { label: 'UTC-09:00 (Alaska)', value: -9 },
  { label: 'UTC-08:00 (Los Angeles, Vancouver)', value: -8 },
  { label: 'UTC-07:00 (Denver, Phoenix)', value: -7 },
  { label: 'UTC-06:00 (Chicago, Mexico City)', value: -6 },
  { label: 'UTC-05:00 (New York, Bogota)', value: -5 },
  { label: 'UTC-04:00 (Santiago, Halifax)', value: -4 },
  { label: 'UTC-03:00 (Buenos Aires, Montevideo)', value: -3 },
  { label: 'UTC-02:00 (South Georgia)', value: -2 },
  { label: 'UTC-01:00 (Azores, Cape Verde)', value: -1 },
  { label: 'UTC+00:00 (London, Reykjavik)', value: 0 },
  { label: 'UTC+01:00 (Berlin, Lagos, Paris)', value: 1 },
  { label: 'UTC+02:00 (Cairo, Johannesburg)', value: 2 },
  { label: 'UTC+03:00 (Moscow, Riyadh)', value: 3 },
  { label: 'UTC+04:00 (Dubai, Baku)', value: 4 },
  { label: 'UTC+05:00 (Karachi, Tashkent)', value: 5 },
  { label: 'UTC+06:00 (Dhaka, Almaty)', value: 6 },
  { label: 'UTC+07:00 (Bangkok, Jakarta)', value: 7 },
  { label: 'UTC+08:00 (Beijing, Perth, Singapore)', value: 8 },
  { label: 'UTC+09:00 (Tokyo, Seoul)', value: 9 },
  { label: 'UTC+10:00 (Sydney, Vladivostok)', value: 10 },
  { label: 'UTC+11:00 (Solomon Islands, Magadan)', value: 11 },
  { label: 'UTC+12:00 (Auckland, Fiji)', value: 12 },
  { label: 'UTC+13:00 (Tonga, Phoenix Islands)', value: 13 },
  { label: 'UTC+14:00 (Kiritimati, Line Islands)', value: 14 },
];

function emptySlot() { return { id: 0, ident: 'icon', font: 31 }; }

const config = [
  // ------------------------------------------------------------------ Clock
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Clock' },
      { type: 'text', id: 'analog-credit', defaultValue:
        'The clock face is based on the <b>Nyquist</b> watchface ' +
        'by truhanen, used under the GPL-3.0 license.<br>' +
        'github.com/truhanen/pebble-nyquist-watchface' },
      { type: 'radiogroup', messageKey: 'SettingShowLeadingZero', label: 'Leading zero in times',
        defaultValue: '0', options: [{ label: 'No', value: '0' }, { label: 'Yes', value: '1' }] },
      { type: 'radiogroup', messageKey: 'SettingHourlyVibe', label: 'Periodic vibration',
        defaultValue: '0', options: [
          { label: 'No vibration', value: '0' },
          { label: 'Every 30 minutes', value: '2' },
          { label: 'Every hour', value: '1' },
        ] },
    ],
  },

  // ---------------------------------------------------------------- Colors
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Colors' },
      { type: 'color', messageKey: 'SettingColorTime', label: 'Time text color',
        defaultValue: '0x000000', sunlight: false, allowGray: true },
      { type: 'color', messageKey: 'SettingColorBG', label: 'Background color',
        defaultValue: '0xFFFFFF', sunlight: false, allowGray: true },
    ],
  },

  // --------------------------------------------------------------- Widgets
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Widgets' },
      { type: 'text', id: 'widgets-help', defaultValue:
        'Widgets are placed in three lists: the <b>center column</b> and the <b>left</b> and ' +
        '<b>right edge</b> of the screen. Each column is laid out top to bottom, spread ' +
        'evenly between the top and bottom of the screen (a single widget sits in the ' +
        'middle; add Empty widgets to push the others together or apart). In the center ' +
        'column each widget is centered, with its icon or unit to the right of the value ' +
        '(for example the ISO date in a small font right below the digital time). On an ' +
        'edge, a widget’s value sits at the screen edge and its icon sits next to it, ' +
        'toward the centre.<br><br>' +
        'Each row is one widget. Pick the widget from the dropdown, choose the ' +
        '<b>font</b> of its value (Default = Gothic medium at the edges, Bitham large ' +
        'in the center column), reorder with ▲ / ▼, remove with ✕, and add more with ' +
        '“+ Add widget”.<br><br>' +
        'The <b>◉ / U / ⊘</b> button after the dropdown picks that widget’s ' +
        '<b>identifier</b>: <b>◉</b> = its icon (battery, weather, steps, heart rate, …), ' +
        '<b>U</b> = its unit (%, °C, h, bpm, UVI, a coin code, …), <b>⊘</b> = none, ' +
        'leaving only the value and making the widget shorter so you can fit more along the ' +
        'edge. A widget offers only the identifiers it has; one without an icon shows its ' +
        'unit by default.<br>' +
        '(Widgets with no separate identifier — Empty, the dates, Seconds and the ' +
        'icon-only widgets — have no such button.)' },
      { type: 'text', id: 'widgets-power-warning', defaultValue:
        '<b>⚡ Battery note.</b> The watchface redraws <i>everything</i> at once — ' +
        'there is no partial refresh — so anything that forces a faster refresh ' +
        'costs battery across the whole face, not just its own widget.<br><br>' +
        '<b>Seconds</b> is by far the most expensive widget: it redraws the entire ' +
        'watchface <b>60× per minute</b> instead of once, all day and all night. ' +
        'Expect a large drop in battery life if you place it.<br>' +
        'A rotating group set to <b>5 / 10 / 30 s</b> is the same effect on a smaller ' +
        'scale (12 / 6 / 2 extra full redraws per minute). Intervals of <b>1 min or ' +
        'longer are free</b> — they ride the refresh the watchface does anyway.' },
      // Defaults: font 31 = "Default" (the watch maps it per list: Gothic medium on
      // the edges, Bitham large in the center column). Each edge groups an icon-only
      // widget over its value (temperature: no identifier; battery: the % unit) in the top corner and one widget in the
      // bottom corner; the centre column holds the date at the top and an Empty slot below it; five Empty slots push them apart. Ids: 37 weather icon,
      // 7 weather, 0 Empty, 10 steps, 36 battery icon, 2 battery, 12 heart rate,
      // 30 date "Wkday dd". Must match the watch defaults in src/c/settings.c.
      { type: 'text', defaultValue: '<b>Center column</b>' },
      { type: 'widgetList', messageKey: 'WidgetListCenter', defaultFont: 7,
        defaultValue: [{ id: 30, ident: 'icon', font: 31 }, emptySlot()] },
      { type: 'text', defaultValue: '<b>Left edge</b>' },
      { type: 'widgetList', messageKey: 'WidgetList', defaultFont: 5,
        defaultValue: [{ id: 37, ident: 'icon', font: 31 }, { id: 7, ident: 'none', font: 31 },
          emptySlot(), emptySlot(), emptySlot(), emptySlot(), emptySlot(),
          { id: 10, ident: 'icon', font: 31 }] },
      { type: 'text', defaultValue: '<b>Right edge</b>' },
      { type: 'widgetList', messageKey: 'WidgetListRight', defaultFont: 5,
        defaultValue: [{ id: 36, ident: 'icon', font: 31 }, { id: 2, ident: 'unit', font: 31 },
          emptySlot(), emptySlot(), emptySlot(), emptySlot(), emptySlot(),
          { id: 12, ident: 'icon', font: 31 }] },
      { type: 'radiogroup', messageKey: 'SettingDisableAutobattery', label: 'Auto-show battery meter',
        defaultValue: '0', options: [{ label: 'On', value: '0' }, { label: 'Off', value: '1' }] },
      { type: 'select', messageKey: 'SettingAutoBatteryThreshold', label: 'Auto-show below battery %',
        defaultValue: '10', options: [
          { label: '5 %', value: '5' }, { label: '10 %', value: '10' }, { label: '15 %', value: '15' },
          { label: '20 %', value: '20' }, { label: '25 %', value: '25' }, { label: '30 %', value: '30' },
          { label: '40 %', value: '40' }, { label: '50 %', value: '50' },
        ] },
      { type: 'radiogroup', messageKey: 'SettingFallbackColumn',
        label: 'Battery / disconnect icon edge', defaultValue: '0', options: [
          { label: 'Automatic', value: '0' }, { label: 'Left edge', value: '1' }, { label: 'Right edge', value: '2' },
        ] },
      { type: 'input', messageKey: 'SettingFallbackPosition', label: 'Position in that edge list',
        attributes: { type: 'number', min: 1, max: 17, step: 1, placeholder: '1' }, defaultValue: '1' },
      { type: 'text', id: 'fallback-help', defaultValue:
        'Position N replaces the Nth widget in the chosen edge list (left or right; the ' +
        'center column is not used). A position one past the last widget ' +
        'adds the battery / disconnect icon as an extra at the end instead of ' +
        'replacing one.' },
    ],
  },

  // --------------------------------------------------------------- Weather
  {
    type: 'section',
    items: [
      { type: 'heading', id: 'heading-weather', defaultValue: 'Weather' },
      { type: 'radiogroup', messageKey: 'SettingUseMetric', label: 'Temperature units',
        defaultValue: '1', options: [{ label: 'Celsius', value: '1' }, { label: 'Fahrenheit', value: '0' }] },
      { type: 'radiogroup', messageKey: 'weather_loc_mode', label: 'Location',
        defaultValue: 'auto', options: [{ label: 'Automatic (GPS)', value: 'auto' }, { label: 'Manual', value: 'manual' }] },
      { type: 'input', messageKey: 'weather_loc', label: 'Manual location label',
        attributes: { placeholder: 'e.g. Helsinki' }, defaultValue: '' },
      { type: 'input', messageKey: 'weather_loc_lat', label: 'Latitude',
        attributes: { type: 'number', step: 0.0001, placeholder: '60.1699' }, defaultValue: '' },
      { type: 'input', messageKey: 'weather_loc_lng', label: 'Longitude',
        attributes: { type: 'number', step: 0.0001, placeholder: '24.9384' }, defaultValue: '' },
      { type: 'radiogroup', messageKey: 'weather_datasource', label: 'Data source',
        defaultValue: 'openmeteo', options: [
          { label: 'Open-Meteo', value: 'openmeteo' },
          { label: 'FMI (Ilmatieteen laitos)', value: 'fmi' },
        ] },
      { type: 'radiogroup', messageKey: 'SettingPrecipUnit', label: 'Precipitation unit',
        defaultValue: '0', options: [{ label: 'mm', value: '0' }, { label: 'inch', value: '1' }] },
    ],
  },

  // ----------------------------------------------------------- Electricity
  {
    type: 'section',
    items: [
      { type: 'heading', id: 'heading-electricity', defaultValue: 'Electricity' },
      { type: 'input', messageKey: 'SettingElecQuietStart', label: 'Quiet hours start (0-23)',
        attributes: { type: 'number', min: 0, max: 23, placeholder: '23' }, defaultValue: '23' },
      { type: 'input', messageKey: 'SettingElecQuietEnd', label: 'Quiet hours end (0-23)',
        attributes: { type: 'number', min: 0, max: 23, placeholder: '7' }, defaultValue: '7' },
      { type: 'text', id: 'elec-cheap-help', defaultValue:
        'The “Next cheap electricity” and “Cheapest electricity hour” widgets ' +
        'find upcoming low-price hours. An hour counts as ' +
        '<b>cheap</b> when its price is below a threshold worked out from the ' +
        'three settings below:<br>' +
        'threshold = <b>average</b> upcoming price × <b>Cheap factor</b>, then ' +
        'kept within <b>Cheap floor … Cheap ceiling</b>.<br><br>' +
        '<b>Cheap factor</b> — the threshold as a percentage of the average ' +
        'upcoming price (quiet hours excluded). 70 % means “below 70 % of the ' +
        'average”. Lower = stricter (fewer hours qualify), higher = more ' +
        'lenient.<br>' +
        '<b>Cheap floor</b> (snt/kWh) — the lowest the threshold can ever go. ' +
        'When prices are low across the board this stops the threshold ' +
        'collapsing, so a genuinely cheap hour is still found instead of ' +
        'none.<br>' +
        '<b>Cheap ceiling</b> (snt/kWh) — the highest the threshold can ever go. ' +
        'During an expensive period nothing above the ceiling is called cheap, so ' +
        'the widget honestly shows “--” rather than a merely “less bad” ' +
        'hour.<br><br>' +
        'Examples with factor 70 %, floor 2, ceiling 8: average 2 → threshold ' +
        '2.0 · average 10 → 7.0 · average 30 → 8.0 (so usually nothing ' +
        'qualifies).' },
      { type: 'input', messageKey: 'SettingElecCheapFactorPct', label: 'Cheap factor (% of average)',
        attributes: { type: 'number', min: 1, max: 100, placeholder: '70' }, defaultValue: '70' },
      { type: 'input', messageKey: 'elec_cheap_floor', label: 'Cheap floor (snt/kWh)',
        attributes: { type: 'number', step: 0.1, placeholder: '2.0' }, defaultValue: '2.0' },
      { type: 'input', messageKey: 'elec_cheap_ceiling', label: 'Cheap ceiling (snt/kWh)',
        attributes: { type: 'number', step: 0.1, placeholder: '8.0' }, defaultValue: '8.0' },
    ],
  },

  // --------------------------------------------------------------- Crypto
  {
    type: 'section',
    items: [
      { type: 'heading', id: 'heading-crypto', defaultValue: 'Crypto' },
      { type: 'text', defaultValue:
        'Prices load from CoinGecko; the Data Refresh interval (bottom of this page) ' +
        'sets how often. Add coins, then place each in one of the widget lists above. ' +
        'Precision: positive = decimals (2 → 104000.00), negative = round (−3 → 104). ' +
        'Trim: cut N leading digits (1.160 with trim 2 → 60).' },
      { type: 'cryptoList', messageKey: 'CryptoList', defaultValue: [] },
    ],
  },

  // ------------------------------------------------------------- Currency
  {
    type: 'section',
    items: [
      { type: 'heading', id: 'heading-currency', defaultValue: 'Currency pairs' },
      { type: 'text', defaultValue:
        'Live fiat exchange rates from ExchangeRate-API (open access, no key). ' +
        'Add pairs (base / quote), then place each in one of the widget lists above. ' +
        'Precision: positive = decimals (EUR/USD 4 → 1.0823), negative = round. ' +
        'Trim: cut N leading digits (1.160, trim 2 → 60). ' +
        'Leave the label empty to show the quote currency (e.g. USD); short labels fit best.' },
      { type: 'currencyList', messageKey: 'CurrencyList', defaultValue: [] },
    ],
  },

  // ------------------------------------------------------------------ Tuya sensors
  {
    type: 'section',
    items: [
      { type: 'heading', id: 'heading-tuya', defaultValue: 'Tuya sensors & switches' },
      { type: 'text', defaultValue:
        'Show live readings from your Tuya cloud sensors (soil moisture, temperature, ' +
        'humidity, battery, …) in a widget. This talks to Tuya&#39;s cloud, so it ' +
        'needs your own free Tuya developer keys &mdash; the official Tuya / Smart Life app ' +
        'cannot be read directly.<br><br>' +
        '<b>1.</b> Go to <b>iot.tuya.com</b> and sign up. In the left menu pick <b>Cloud &gt; ' +
        'Development</b> and <b>Create Cloud Project</b> (type: Smart Home). This is a ' +
        '<i>Cloud Project</i>, not an <i>App</i>. Pick the <b>Data Center</b> for your region ' +
        '(e.g. Central Europe).<br>' +
        '<b>2.</b> The project shows an <b>Access ID</b> and <b>Access Secret</b> &mdash; paste ' +
        'them below.<br>' +
        '<b>3.</b> In the project open <b>Devices &gt; Link App Account</b> and scan the QR with ' +
        'the Smart&nbsp;Life app (<b>Me &gt; Scan</b>). Your sensors then appear under the ' +
        'project.<br>' +
        '<b>4.</b> Choose the <b>same Data Center</b> below as your account&#39;s region.<br><br>' +
        'Already use the <b>Sykerö Tuya Lights</b> watchapp? It takes the very same Access ID / ' +
        'Access Secret / Data Center &mdash; just reuse them here.' },
      { type: 'input', messageKey: 'TuyaAccessId', label: 'Access ID', attributes: { placeholder: 'from your Cloud Project' } },
      { type: 'input', messageKey: 'TuyaAccessSecret', label: 'Access Secret', attributes: { placeholder: 'from your Cloud Project' } },
      { type: 'select', messageKey: 'TuyaRegion', label: 'Data center', defaultValue: 'eu', options: [
        { label: 'Central Europe', value: 'eu' }, { label: 'Western America', value: 'us' },
        { label: 'China', value: 'cn' }, { label: 'India', value: 'in' },
        { label: 'Western Europe', value: 'eu-w' }, { label: 'Eastern America', value: 'us-e' } ] },
      { type: 'text', defaultValue:
        'Tip: this is Tuya&#39;s <b>data center</b> &mdash; not your country. Your country maps to ' +
        'one of these six, and it must match your Cloud Project&#39;s data center or no sensors ' +
        'will appear.<br><b>Western America</b> = all of the Americas (Argentina, Brazil, Chile, ' +
        'Mexico, US, …), Japan/Korea/South-East Asia, and any country Tuya doesn&#39;t list. ' +
        '<b>Central Europe</b> = Europe, most of Africa, Australia, the Middle East. ' +
        '<b>China</b> and <b>India</b> = those two countries only.' },
      { type: 'text', defaultValue:
        'Save your credentials, then <b>close and reopen this page</b> so your devices can load. ' +
        '(The first load talks to the Tuya cloud and can take a few seconds; after that the list ' +
        'is cached, so this page opens instantly and refreshes in the background &mdash; a ' +
        'newly-added device shows up the next time you open settings.)<br><br>' +
        'Add a sensor, pick its datapoint &mdash; the dropdown shows each code&#39;s current value, ' +
        'so the right one is easy to spot (e.g. a soil sensor&#39;s real moisture) &mdash; then place ' +
        'it in one of the widget lists near the top of this page.<br><br>' +
        'Precision: positive = decimals, negative = round. Trim: cut N leading digits. ' +
        'Leave the label empty to use the datapoint code.' },
      { type: 'tuyaCatalog', messageKey: 'TuyaCatalog', defaultValue: '' },
      { type: 'tuyaList', messageKey: 'TuyaList', defaultValue: [] },
      { type: 'text', defaultValue:
        'Switches can also be shown as a compact row of coloured LEDs — add them below, ' +
        'then place the <b>Tuya LEDs</b> widget in one of the widget lists.' },
      { type: 'tuyaLedList', messageKey: 'TuyaLedList', defaultValue: [] },
    ],
  },

  // ------------------------------------------------------------- Bluetooth
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Bluetooth' },
      { type: 'radiogroup', messageKey: 'SettingDisconnectIcon', label: 'Disconnect icon',
        defaultValue: '1', options: [{ label: 'Show', value: '1' }, { label: 'Hide', value: '0' }] },
      { type: 'radiogroup', messageKey: 'SettingBluetoothVibe', label: 'Disconnect vibration',
        defaultValue: '0', options: [{ label: 'Vibrate', value: '1' }, { label: 'None', value: '0' }] },
    ],
  },

  // -------------------------------------------------------------- Regional
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Regional' },
      { type: 'select', messageKey: 'SettingLanguageID', label: 'Language', defaultValue: 0, options: LANGUAGES },
      { type: 'input', messageKey: 'SettingAltClockName', label: 'Alternate clock name (max 4)',
        attributes: { maxlength: 4, placeholder: 'ALT' }, defaultValue: 'ALT' },
      { type: 'select', messageKey: 'SettingAltClockOffset', label: 'Alternate clock time zone',
        defaultValue: 0, options: ALT_CLOCK_OFFSETS },
      { type: 'radiogroup', messageKey: 'SettingDecimalSep', label: 'Decimal separator',
        defaultValue: '.', options: [{ label: '.', value: '.' }, { label: ',', value: ',' }] },
    ],
  },

  // ---------------------------------------------------------- Data Refresh
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Data Refresh' },
      { type: 'text', defaultValue:
        'How often the watch asks the phone for fresh online data: weather, ' +
        'electricity (spot prices) and crypto / currency prices. Lower = more ' +
        'up-to-date but more battery/Bluetooth use. (Electricity self-limits to ' +
        '~2 fetches/day; crypto is sent only when a displayed price changes.)' },
      { type: 'input', messageKey: 'SettingPollIntervalMin', label: 'Data refresh interval (min)',
        attributes: { type: 'number', min: 5, max: 240, step: 1, placeholder: '30' }, defaultValue: '30' },
    ],
  },

  // ------------------------------------------------------------ Warning border
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Warning border' },
      { type: 'text', defaultValue:
        'Draws a coloured frame around the clock face when something needs attention. Off by default.' },
      { type: 'select', messageKey: 'SettingBatteryWarnPct', label: 'Battery warning below',
        defaultValue: '0', options: [
          { label: 'Off', value: '0' },
          { label: '5 %', value: '5' },
          { label: '10 %', value: '10' },
          { label: '15 %', value: '15' },
          { label: '20 %', value: '20' },
          { label: '25 %', value: '25' },
          { label: '30 %', value: '30' },
        ] },
      { type: 'select', messageKey: 'SettingBatteryWarnDays', label: 'Battery warning below (days left)',
        defaultValue: '0', options: [
          { label: 'Off', value: '0' },
          { label: '0.5 days', value: '5' },
          { label: '1 day', value: '10' },
          { label: '2 days', value: '20' },
          { label: '3 days', value: '30' },
          { label: '5 days', value: '50' },
        ] },
      { type: 'color', messageKey: 'SettingBatteryWarnColor', label: 'Battery border colour',
        defaultValue: '0xFF0000', sunlight: false, allowGray: true },
      { type: 'toggle', messageKey: 'SettingBtWarnBorder',
        label: 'Border while phone is disconnected', defaultValue: false },
      { type: 'color', messageKey: 'SettingBtWarnColor', label: 'Disconnect border colour',
        defaultValue: '0xFFFF00', sunlight: false, allowGray: true },
      { type: 'text', id: 'warn-border-help', defaultValue:
        'The two battery triggers are independent — either one alone switches the frame ' +
        'on, and either can be Off. The days trigger uses the same estimate as the ' +
        '“Battery (days left)” widget, so it does nothing until the watch has learned ' +
        'your discharge rate. No frame is shown while charging.<br>' +
        'If both a battery and a disconnect warning apply, the battery one wins. A ' +
        'colour that matches your clock background is drawn black or white instead, so ' +
        'the frame is never invisible.' },
    ],
  },

  // -------------------------------------------------------------------- Night
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Night' },
      { type: 'text', defaultValue:
        'One night window, two independent uses: saving battery on widget rotation, ' +
        'and a dark palette.' },
      // NOTE: the messageKey names below keep the historical "SettingNightRotation*"
      // spelling on purpose, even though this window now feeds two independent
      // consumers (slow rotation + night colours), not just rotation. Clay's settings
      // store keys off the messageKey, so renaming any of the three would silently
      // reset every existing user's configured night window on their next config open.
      // Only the displayed labels changed — do not "fix" the names.
      { type: 'select', messageKey: 'SettingNightRotationMode', label: 'Night mode',
        defaultValue: '0', options: [
          { label: 'Off', value: '0' },
          { label: 'Follow Quiet Time', value: '1' },
          { label: 'Custom hours', value: '2' },
        ] },
      { type: 'input', messageKey: 'SettingNightRotationStart', label: 'Night starts (hour)',
        attributes: { type: 'number', min: 0, max: 23, step: 1, placeholder: '23' }, defaultValue: '23' },
      { type: 'input', messageKey: 'SettingNightRotationEnd', label: 'Night ends (hour)',
        attributes: { type: 'number', min: 0, max: 23, step: 1, placeholder: '7' }, defaultValue: '7' },
      { type: 'toggle', messageKey: 'SettingNightSlowRotation', label: 'Slow widget rotation',
        defaultValue: true },
      { type: 'text', id: 'night-rotation-help', defaultValue:
        'During the night window, groups set to 5 / 10 / 30 s drop to one change per ' +
        'minute. Rotation keeps going and every widget is still shown — only the extra ' +
        'wakeups go away, which is where the battery saving comes from. Has no effect ' +
        'if all your groups are already 1 min or slower.' },
      { type: 'toggle', messageKey: 'SettingNightColors', label: 'Night colours',
        defaultValue: false },
      { type: 'color', messageKey: 'SettingNightBgColor', label: 'Night background',
        defaultValue: '0x000000', sunlight: false, allowGray: true },
      { type: 'color', messageKey: 'SettingNightFgColor', label: 'Night foreground',
        defaultValue: '0xFFFFFF', sunlight: false, allowGray: true },
      { type: 'text', id: 'night-help', defaultValue:
        'Night colours replace every background and every text/hand colour while the ' +
        'window is on, then restore your own colours afterwards — nothing is ' +
        'overwritten. The window runs from the start hour up to (not including) the end ' +
        'hour, and may cross midnight. Setting both to the same hour disables it.<br>' +
        'Warning borders keep their own colours at night, so a low battery still stands ' +
        'out.' },
    ],
  },

  { type: 'submit', defaultValue: 'Save' },
];

export = config;

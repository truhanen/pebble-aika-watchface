// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// Watch settings sent "straight through" from the Clay config to the watchapp in
// webviewclosed (index.ts): each is coerced to an int (or kept as a string for the
// STRING_KEYS) and placed in the outbound AppMessage dict. Every scalar
// toggle / radiogroup / select / numeric-input setting the watch reads MUST appear
// here, or its config value never reaches the watch and it silently stays at the C
// default. Kept in its own module so tests can assert coverage without loading
// index.ts's Pebble runtime.
export const STRAIGHT_THROUGH_KEYS: string[] = [
  'SettingLanguageID', 'SettingShowLeadingZero', 'SettingDisconnectIcon',
  'SettingBluetoothVibe', 'SettingHourlyVibe',
  'SettingUseMetric',
  'SettingDisableAutobattery', 'SettingAltClockName', 'SettingAltClockOffset',
  'SettingDecimalSep',
  'SettingPollIntervalMin', 'SettingElecQuietStart', 'SettingElecQuietEnd',
  'SettingElecCheapFactorPct',
  'SettingAutoBatteryThreshold', 'SettingFallbackColumn', 'SettingFallbackPosition',
  'SettingNightRotationMode', 'SettingNightRotationStart', 'SettingNightRotationEnd',
  'SettingNightSlowRotation', 'SettingNightColors',
  'SettingBatteryWarnPct', 'SettingBatteryWarnDays', 'SettingBtWarnBorder',
];

// Colour settings, sent on index.ts's own path: Clay yields a 24-bit RGB value which the
// watch decodes with GColorFromHEX. Kept here, not inline in index.ts, so
// config_send_keys.test.js can assert that every Clay colour item is actually sent -- the same class of silent bug the
// straight-through coverage test exists for.
export const COLOR_KEYS: string[] = [
  'SettingColorTime', 'SettingColorBG',
  'SettingBatteryWarnColor', 'SettingBtWarnColor',
  'SettingNightBgColor', 'SettingNightFgColor',
];

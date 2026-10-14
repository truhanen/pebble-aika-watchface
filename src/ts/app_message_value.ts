// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

// Coerce a Clay getSettings(resp, false) raw value to the int/string the watch
// expects. Clay returns radiogroup/select/input values as strings and toggles as
// booleans (the `checked` manipulator), while the watch reads ints (value->int32)
// for every key except genuine string settings. parseInt(boolean) is NaN, so
// booleans MUST be mapped explicitly — this mirrors Clay's own prepareForAppMessage
// (boolean -> val ? 1 : 0).
export function toAppMessageValue(raw: any, isStringKey: boolean): any {
  if (isStringKey) { return raw; }
  if (typeof raw === 'boolean') { return raw ? 1 : 0; }
  return parseInt(raw, 10);
}

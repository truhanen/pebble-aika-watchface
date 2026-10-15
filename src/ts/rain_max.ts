// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Pure helpers for the "maximum hourly precipitation in the next N hours" widgets.
   No Pebble/browser globals -- unit-testable with `node --test`. See docs/rain_data.md.

   Hour slots: p[i] is the precipitation (mm) that falls during the hour
   [H + i h, H + (i+1) h) where H = now rounded down to a full hour, i = 0..11.
   Both FMI (Precipitation1h) and Open-Meteo (precipitation) stamp an hourly value
   with the END of the hour it accumulated over, so p[i] is the sample stamped
   H + (i+1) h. p[0] is therefore "the hour we are in right now". */

export interface PrecipSample {
  epoch: number;   // seconds since the Unix epoch (the end of the accumulation hour)
  value: number;   // mm in that hour; NaN/non-finite samples are ignored
}

export const RAIN_HOURS = 12;
export const RAIN_WINDOWS: number[] = [1, 3, 6, 9, 12];
export const RAIN_UNKNOWN = 0xFFFF;

const HOUR = 3600;

// Samples -> p[0..11] (mm), NaN for an hour with no usable sample.
export function hourlyPrecip(samples: PrecipSample[], nowEpochSec: number): number[] {
  const h0 = Math.floor(nowEpochSec / HOUR) * HOUR;
  const p: number[] = [];
  for (let i = 0; i < RAIN_HOURS; i++) { p.push(NaN); }
  for (const s of samples) {
    if (!isFinite(s.epoch) || !isFinite(s.value)) { continue; }
    const rel = s.epoch - h0;
    if (rel % HOUR !== 0) { continue; }          // only exact full-hour stamps
    const i = rel / HOUR - 1;
    if (i >= 0 && i < RAIN_HOURS) { p[i] = Math.max(0, s.value); }
  }
  return p;
}

// Max over the first N hours for each window, in hundredths of a mm (rounded),
// RAIN_UNKNOWN when none of those N hours is known. Unknown hours inside a window
// are skipped (the result is the max of the known ones).
export function rainMaxHundredths(p: number[]): number[] {
  return RAIN_WINDOWS.map((n) => {
    let best = -1;
    for (let i = 0; i < n && i < p.length; i++) {
      const v = p[i];
      if (typeof v === 'number' && isFinite(v) && v > best) { best = v; }
    }
    if (best < 0) { return RAIN_UNKNOWN; }
    // 0xFFFF is reserved, so clamp real values just below it.
    return Math.min(RAIN_UNKNOWN - 1, Math.round(best * 100));
  });
}

// uint16 little-endian byte array (10 bytes) for the WeatherRainMax message key.
export function packRainMax(values: number[]): number[] {
  const out: number[] = [];
  for (const v of values) {
    const u = Math.max(0, Math.min(RAIN_UNKNOWN, Math.round(v)));
    out.push(u & 0xFF, (u >> 8) & 0xFF);
  }
  return out;
}

export function rainMaxBytes(samples: PrecipSample[], nowEpochSec: number): number[] {
  return packRainMax(rainMaxHundredths(hourlyPrecip(samples, nowEpochSec)));
}

// Open-Meteo hourly arrays -> samples. `time` entries are local ISO strings
// ("2026-06-10T13:00") in the LOCATION's zone, so convert with the response's
// utc_offset_seconds rather than the phone's zone.
export function openMeteoPrecipSamples(
  times: unknown, precip: unknown, utcOffsetSeconds: unknown,
): PrecipSample[] {
  const out: PrecipSample[] = [];
  if (!Array.isArray(times) || !Array.isArray(precip)) { return out; }
  const off = typeof utcOffsetSeconds === 'number' && isFinite(utcOffsetSeconds) ? utcOffsetSeconds : 0;
  const re = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2})/;
  for (let i = 0; i < times.length && i < precip.length; i++) {
    const m = re.exec(String(times[i]));
    const v = precip[i];
    if (!m || typeof v !== 'number') { continue; }   // null = no data
    const ms = Date.UTC(+m[1], +m[2] - 1, +m[3], +m[4], +m[5]);
    out.push({ epoch: Math.floor(ms / 1000) - off, value: v });
  }
  return out;
}

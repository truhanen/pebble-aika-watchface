// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Pure parser for FMI open-data WFS "point timevaluepair" forecast XML
   (fmi::forecast::edited::weather::scandinavia::point::timevaluepair). No
   Pebble/browser globals -- unit-testable with `node --test`. Unlike the
   ::simple variant, this one snaps the queried lat/lon to FMI's nearest *named*
   forecast location (the same one ilmatieteenlaitos.fi shows, e.g. "Jyskä") and
   returns its gml:name. One <wfs:member> per parameter: an om:OM_Observation
   whose observedProperty href carries param=<name>, with a MeasurementTimeseries
   of <wml2:time>/<wml2:value> pairs ('NaN' = gap). */

const NAME_RE = /<gml:name[^>]*locationcode\/name[^>]*>([^<]+)<\/gml:name>/;
const PARAM_RE = /param=([a-z0-9]+)/;
const TVP_RE = /<wml2:time>([^<]+)<\/wml2:time>\s*<wml2:value>([^<]+)<\/wml2:value>/g;

interface Sample {
  epoch: number;
  value: number;
}

export interface FmiForecastTvp {
  ok: true;
  currentTemp: number;
  currentSymbol: number | null;
  forecastSymbol: number | null;
  forecastHigh: number;
  forecastLow: number;
  uvIndex: number;
  name: string;
}

export type FmiForecastTvpResult = FmiForecastTvp | { ok: false };

function nearest(series: Sample[], nowEpochSec: number): Sample | null {
  let best: Sample | null = null;
  let bestDiff = Infinity;
  for (const s of series) {
    const d = Math.abs(s.epoch - nowEpochSec);
    if (d < bestDiff) { bestDiff = d; best = s; }
  }
  return best;
}

export function parseFmiForecastTvp(
  xml: unknown, nowEpochSec: number, fcSymbolEpochSec?: number,
): FmiForecastTvpResult {
  if (typeof xml !== 'string' || xml.indexOf('ExceptionReport') !== -1) {
    return { ok: false };
  }

  const nm = NAME_RE.exec(xml);
  const name = nm ? nm[1] : '';

  const temps: Sample[] = [];
  const syms: Sample[] = [];
  const uvs: Sample[] = [];
  const members = xml.split('<wfs:member>');
  for (let i = 1; i < members.length; i++) {        // [0] is the header chunk
    const chunk = members[i];
    const pm = PARAM_RE.exec(chunk);
    if (!pm) { continue; }
    const target =
      pm[1] === 'temperature' ? temps
      : pm[1] === 'smartsymbol' ? syms
      : pm[1] === 'uvindex' ? uvs : null;
    if (!target) { continue; }

    let m: RegExpExecArray | null;
    TVP_RE.lastIndex = 0;
    while ((m = TVP_RE.exec(chunk)) !== null) {
      const epoch = Math.floor(Date.parse(m[1]) / 1000);
      const value = parseFloat(m[2]);
      if (isNaN(epoch) || isNaN(value)) { continue; }
      target.push({ epoch, value });
    }
  }

  if (temps.length === 0) { return { ok: false }; }

  const curTemp = nearest(temps, nowEpochSec)!;
  const curSym = syms.length ? nearest(syms, nowEpochSec) : null;
  const curUv = uvs.length ? nearest(uvs, nowEpochSec) : null;

  let high = temps[0].value;
  let low = temps[0].value;
  for (let j = 1; j < temps.length; j++) {
    if (temps[j].value > high) { high = temps[j].value; }
    if (temps[j].value < low) { low = temps[j].value; }
  }

  // Forecast condition = the smartsymbol at ~15:00 local (fcSymbolEpochSec), or
  // the nearest sample -- this is how FMI derives the day's forecast symbol shown
  // on ilmatieteenlaitos.fi (a single representative afternoon reading), NOT the
  // day's most notable weather (which spuriously showed rain when a lone morning
  // or night hour was wet). Strip the +100 night offset so the mapping yields a
  // daytime icon. Falls back to now when no target time is supplied.
  const fcTarget = (typeof fcSymbolEpochSec === 'number' && !isNaN(fcSymbolEpochSec))
    ? fcSymbolEpochSec : nowEpochSec;
  const fcSample = syms.length ? nearest(syms, fcTarget) : null;
  const fcSym: number | null = fcSample ? Math.round(fcSample.value) % 100 : null;

  return {
    ok: true,
    currentTemp: Math.round(curTemp.value),
    currentSymbol: curSym ? Math.round(curSym.value) : null,
    forecastSymbol: fcSym,
    forecastHigh: Math.round(high),
    forecastLow: Math.round(low),
    uvIndex: curUv ? Math.round(curUv.value) : 0,
    name,
  };
}

// FMI named location -> short label: drop the leading municipality token
// ("Jyväskylä keskus" -> "keskus"), plain-truncate to maxLen (no ellipsis), and
// force an uppercase first character.
export function abbreviateLocationName(name: unknown, maxLen?: number): string {
  if (typeof name !== 'string') { return ''; }
  let s = name.trim();
  const sp = s.indexOf(' ');
  if (sp > 0 && sp < s.length - 1) { s = s.substring(sp + 1); }
  if (maxLen && s.length > maxLen) { s = s.substring(0, maxLen); }
  if (s.length > 0) { s = s.charAt(0).toUpperCase() + s.slice(1); }
  return s;
}

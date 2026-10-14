// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Pure helpers for Tuya device /specification results: extract the reportable
   datapoint codes (with scale + unit), the config catalog (baked into the config
   page), and a scale/unit lookup for the poll driver. No Pebble/browser globals ->
   unit-tested in tests/tuya_spec.test.js. */

export interface SensorCode { code: string; type: string; scale: number; unit: string; sample?: any }
export interface CatalogDevice { id: string; name: string; codes: SensorCode[] }
export interface Property { code: string; type: string; value: any }

function parseValues(values: any): { scale: number; unit: string } {
  try {
    const v = JSON.parse(values);
    const scale = (typeof v.scale === 'number') ? v.scale : 0;
    const unit = (typeof v.unit === 'string') ? v.unit : '';
    return { scale: scale, unit: unit };
  } catch (e) {
    return { scale: 0, unit: '' };
  }
}

// Reportable datapoints from a /specification result. Tuya lists read-only sensor
// dps under result.status; some devices also expose them under result.functions.
// status wins on a code collision.
export function parseSpecCodes(specResult: any): SensorCode[] {
  const byCode: Record<string, SensorCode> = {};
  const order: string[] = [];
  function add(entry: any) {
    if (!entry || typeof entry.code !== 'string') { return; }
    const pv = parseValues(entry.values);
    if (!(entry.code in byCode)) { order.push(entry.code); }
    byCode[entry.code] = { code: entry.code, type: String(entry.type || ''), scale: pv.scale, unit: pv.unit };
  }
  const fns = (specResult && specResult.functions) || [];
  for (let i = 0; i < fns.length; i++) { add(fns[i]); }
  const st = (specResult && specResult.status) || [];
  for (let i = 0; i < st.length; i++) { add(st[i]); }   // status overrides functions
  return order.map((c) => byCode[c]);
}

// Parse a /v2.0/cloud/thing/{id}/shadow/properties result into [{code, type, value}].
// This endpoint returns ALL datapoints INCLUDING manufacturer-custom DPs (e.g. a soil
// sensor's real moisture under `humidity1`) that /specification + iot-03/status omit,
// with fresh values. It carries no scale/unit, so scale/unit still come from the spec.
export function parseProperties(propsResult: any): Property[] {
  const props = (propsResult && propsResult.properties) || [];
  const out: Property[] = [];
  for (let i = 0; i < props.length; i++) {
    const p = props[i];
    if (!p || typeof p.code !== 'string') { continue; }
    out.push({ code: p.code, type: String(p.type || ''), value: p.value });
  }
  return out;
}

// Build the catalog code list for one device: EVERY reported property becomes a
// selectable code, carrying its current value as `sample` (shown in the config
// dropdown) and its scale/unit from the /specification where the code is declared
// there (custom DPs absent from the spec default to scale 0 / no unit).
export function mergeCodes(specCodes: SensorCode[], props: Property[], prevCodes?: SensorCode[]): SensorCode[] {
  const specByCode: Record<string, SensorCode> = {};
  const sc = Array.isArray(specCodes) ? specCodes : [];
  for (let i = 0; i < sc.length; i++) { specByCode[sc[i].code] = sc[i]; }
  // Fallback scale/unit from a previously-cached catalog for this device. When a
  // device's /specification fetch fails transiently (shadow/properties still
  // succeeds), the spec is empty and every code would default to scale 0 — showing
  // e.g. a temperature of 235 instead of 23.5 until a future good discovery. Keep
  // the last-known scale/unit for those codes instead of degrading to 0.
  const prevByCode: Record<string, SensorCode> = {};
  const pc = Array.isArray(prevCodes) ? prevCodes : [];
  for (let i = 0; i < pc.length; i++) { prevByCode[pc[i].code] = pc[i]; }
  const out: SensorCode[] = [];
  const ps = Array.isArray(props) ? props : [];
  for (let i = 0; i < ps.length; i++) {
    const p = ps[i];
    const s = specByCode[p.code];
    const fb = prevByCode[p.code];
    // Only carry a sample for numeric/boolean readings. Some DPs hold huge config
    // strings (e.g. a soil sensor's `plants_1` plant table); keeping those as the
    // dropdown sample bloats the catalog and breaks the config layout.
    const sample = (typeof p.value === 'number' || typeof p.value === 'boolean') ? p.value : undefined;
    out.push({
      code: p.code,
      type: p.type || (s ? s.type : (fb ? fb.type : '')),
      scale: s ? s.scale : (fb ? fb.scale : 0),
      unit: s ? s.unit : (fb ? fb.unit : ''),
      sample: sample,
    });
  }
  return out;
}

// Extract {deviceId: SensorCode[]} from a cached catalog (buildCatalog's shape),
// so a re-discovery can fall back to the last-known scale/unit per device when a
// device's /specification fetch fails. Missing/malformed catalog -> {}.
export function catalogCodesById(catalog: any): Record<string, SensorCode[]> {
  const out: Record<string, SensorCode[]> = {};
  const devs = (catalog && Array.isArray(catalog.devices)) ? catalog.devices : [];
  for (let i = 0; i < devs.length; i++) {
    const d = devs[i];
    if (d && typeof d.id === 'string' && Array.isArray(d.codes)) { out[d.id] = d.codes; }
  }
  return out;
}

export function shortUnit(unit: string): string {
  const u = (typeof unit === 'string') ? unit : '';
  if (u === '℃' || u === '°C' || u === '℉' || u === '°F' || u === '°') { return '°'; }
  if (u === '%') { return '%'; }
  return '';
}

export function buildCatalog(devices: any[], codesById: Record<string, SensorCode[]>):
    { v: number; devices: CatalogDevice[] } {
  const arr: CatalogDevice[] = [];
  const list = Array.isArray(devices) ? devices : [];
  for (let i = 0; i < list.length; i++) {
    const d = list[i];
    if (!d || typeof d.id !== 'string') { continue; }
    arr.push({ id: d.id, name: String(d.name || d.id), codes: codesById[d.id] || [] });
  }
  return { v: 1, devices: arr };
}

export function catalogScaleMap(catalog: any):
    Record<string, Record<string, { scale: number; unit: string }>> {
  const out: Record<string, any> = {};
  const devs = (catalog && Array.isArray(catalog.devices)) ? catalog.devices : [];
  for (let i = 0; i < devs.length; i++) {
    const d = devs[i];
    if (!d || typeof d.id !== 'string') { continue; }
    const codes = Array.isArray(d.codes) ? d.codes : [];
    const cm: Record<string, any> = {};
    for (let j = 0; j < codes.length; j++) {
      const c = codes[j];
      if (c && typeof c.code === 'string') { cm[c.code] = { scale: c.scale || 0, unit: c.unit || '' }; }
    }
    out[d.id] = cm;
  }
  return out;
}

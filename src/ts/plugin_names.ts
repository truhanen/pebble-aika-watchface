// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Widget names for plugin-sourced widgets (ids 144..159), shown in the widget picker:
     CATEGORY Plugin Item Instance Prop      e.g.  WEATHER FMI Loc Jyv Tem
   Rules:
   - category: upper case (ASCII-folded).
   - plugin / item / instance / property: ASCII-folded (diacritics stripped, a-umlaut ->
     a, o-slash -> o, ...), non-alphanumerics dropped, cut to the first `len` characters
     (3 by default) with the first character upper-cased (the rest is kept as is, so
     "FMI" stays "FMI", "Jyvaskyla" -> "Jyv").
   - instance part: the instance's own name when it has one, else its number ("1");
     omitted entirely for items that do not support multiple instances.
   - the list is made unique: entries that collide get a longer cut (4, 5, ... chars,
     all four cut fields together) until they differ; entries still equal once nothing
     can be extended get a numeric suffix (" 2", " 3", ...) as a last resort.
   KEEP IN SYNC with the copy inlined (between the BEGIN/END naming markers) in
   config_plugin_list.ts -- that component is serialised by Clay's toSource() and cannot
   import this module; tests/plugin_names.test.js checks both give the same names. Written
   ES5 / helper-free so the inline copy can be identical. */

export interface NameParts {
  category: string;
  plugin: string;
  item: string;
  instance: string;   // instance name or number; '' = no instance part
  property: string;
}

const NAME_START_LEN = 3;

// Letters NFD cannot decompose.
const FOLD_MAP: { [ch: string]: string } = {
  'ø': 'o', 'Ø': 'O', 'æ': 'ae', 'Æ': 'AE', 'œ': 'oe', 'Œ': 'OE', 'ß': 'ss',
  'đ': 'd', 'Đ': 'D', 'ł': 'l', 'Ł': 'L', 'ð': 'd', 'Ð': 'D', 'þ': 'th', 'Þ': 'TH', 'ı': 'i',
};

// ASCII-fold: strip diacritics, map the common non-decomposing letters, drop the rest
// (anything that is not A-Z a-z 0-9).
export function foldAscii(s: any): string {
  let t = (s === undefined || s === null) ? '' : String(s);
  if (typeof t.normalize === 'function') { t = t.normalize('NFD'); }
  let out = '';
  for (let i = 0; i < t.length; i++) {
    const ch = t.charAt(i);
    const m = FOLD_MAP[ch];
    out += (m !== undefined) ? m : ch;
  }
  return out.replace(/[^A-Za-z0-9]/g, '');
}

// One name part: folded, cut to `len` chars, first character upper-cased.
export function shortPart(s: any, len: number): string {
  const f = foldAscii(s).substring(0, len);
  return f.charAt(0).toUpperCase() + f.substring(1);
}

// The name of one selection with every part cut to `len` chars.
export function shortenName(p: NameParts, len?: number): string {
  const n = (len === undefined || len < 1) ? NAME_START_LEN : len;
  const out: string[] = [foldAscii(p.category).toUpperCase()];
  out.push(shortPart(p.plugin, n), shortPart(p.item, n));
  if (p.instance !== '' && p.instance !== undefined && p.instance !== null) { out.push(shortPart(p.instance, n)); }
  out.push(shortPart(p.property, n));
  return out.filter(function(x) { return x !== ''; }).join(' ');
}

// Names for a whole list, unique within it (see the rules above). Result[i] belongs to
// list[i]; a null entry (empty slot) stays ''.
export function uniqueNames(list: (NameParts | null)[]): string[] {
  const lens: number[] = [];
  const names: string[] = [];
  let maxLen = NAME_START_LEN;
  for (let i = 0; i < list.length; i++) {
    const p = list[i];
    lens.push(NAME_START_LEN);
    if (p) {
      const longest = Math.max(foldAscii(p.plugin).length, foldAscii(p.item).length,
        foldAscii(p.instance).length, foldAscii(p.property).length);
      if (longest > maxLen) { maxLen = longest; }
    }
  }
  for (let round = 0; round <= maxLen; round++) {
    let changed = false;
    for (let i = 0; i < list.length; i++) {
      const p = list[i];
      names[i] = p ? shortenName(p, lens[i]) : '';
    }
    for (let i = 0; i < list.length; i++) {
      if (!list[i]) { continue; }
      let dup = false;
      for (let j = 0; j < list.length; j++) { if (j !== i && names[j] === names[i]) { dup = true; break; } }
      if (dup && lens[i] < maxLen) { lens[i] += 1; changed = true; }
    }
    if (!changed) { break; }
  }
  for (let i = 0; i < list.length; i++) {
    const p = list[i];
    names[i] = p ? shortenName(p, lens[i]) : '';
  }
  // Last resort: still-equal names get " 2", " 3", ... (the first keeps its name).
  const seen: { [n: string]: number } = {};
  for (let i = 0; i < list.length; i++) {
    if (!list[i]) { continue; }
    const base = names[i];
    seen[base] = (seen[base] || 0) + 1;
    if (seen[base] > 1) { names[i] = base + ' ' + seen[base]; }
  }
  return names;
}

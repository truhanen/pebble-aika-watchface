// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Per-coin display formatting. `p` controls the rendered value: p >= 0 -> that
   many decimal places; p < 0 -> round to the nearest 10^(-p) and divide it out.
   `t` then cuts the first `t` DIGIT characters off the left (a separator/sign
   passed while still owing digits is dropped but doesn't count toward t), and any
   resulting leading separator is stripped (e.g. 1.160 -> t=1 "160", t=2 "60").
   Leading zeros are kept; if trimming consumes every digit the result is "--".
   Pure, no Pebble/browser globals, unit-tested in crypto_format.test.js. */

export function formatPrice(price: number, p: number, t: number): string {
  if (typeof price !== 'number' || !isFinite(price)) {
    return '--';
  }
  let s: string;
  if (p >= 0) {
    s = price.toFixed(p);
  } else {
    const scale = Math.pow(10, -p);
    s = String(Math.round(price / scale));
  }
  if (!t || t <= 0) {
    return s;
  }
  // Drop `t` leading digits; non-digit chars passed meanwhile don't count.
  let i = 0;
  let dropped = 0;
  while (i < s.length && dropped < t) {
    const c = s.charAt(i);
    if (c >= '0' && c <= '9') { dropped++; }
    i++;
  }
  // Strip an orphaned leading separator/sign from the remainder.
  while (i < s.length && !(s.charAt(i) >= '0' && s.charAt(i) <= '9')) {
    i++;
  }
  const rest = s.substring(i);
  return rest === '' ? '--' : rest;
}

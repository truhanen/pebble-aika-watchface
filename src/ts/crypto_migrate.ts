// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* One-time migration: before the configurable coin list existed, the three
   coins were gated by `disable_btc` / `disable_xmr` / `disable_eurusd`
   localStorage flags and placed as widget ids 15/16/17. Seed `CryptoList` once
   from whichever legacy coins were enabled, KEEPING their legacy wids (15/16/17)
   so already-placed widgets keep resolving to the same coin. Runs in
   showConfiguration before clay.generateUrl(), mirroring migrateWidgetListSettings. */

import { CoinRow } from './crypto_parse';

interface LegacyCoin { flag: string; wid: number; coin: string; p: number; label: string; }

const LEGACY: LegacyCoin[] = [
  { flag: 'disable_btc',    wid: 15, coin: 'bitcoin',   p: -3, label: 'BTC' },
  { flag: 'disable_xmr',    wid: 16, coin: 'monero',    p: 0,  label: 'XMR' },
  { flag: 'disable_eurusd', wid: 17, coin: 'euro-coin', p: 3,  label: 'EUR' },
];

export function seedCryptoList(flags: Record<string, any>): CoinRow[] {
  const out: CoinRow[] = [];
  for (let i = 0; i < LEGACY.length; i++) {
    const L = LEGACY[i];
    if (flags[L.flag] != null && flags[L.flag] !== 'yes') {
      out.push({ wid: L.wid, coin: L.coin, vs: 'usd', p: L.p, t: 0, label: L.label });
    }
  }
  return out;
}

export function migrateCryptoList(): void {
  let stored: Record<string, any>;
  try {
    stored = JSON.parse(window.localStorage.getItem('clay-settings') || '{}') || {};
  } catch (e) {
    return;
  }
  if (stored.CryptoList !== undefined) { return; }
  const flags: Record<string, any> = {
    disable_btc: window.localStorage.getItem('disable_btc'),
    disable_xmr: window.localStorage.getItem('disable_xmr'),
    disable_eurusd: window.localStorage.getItem('disable_eurusd'),
  };
  stored.CryptoList = seedCryptoList(flags);
  window.localStorage.setItem('clay-settings', JSON.stringify(stored));
}

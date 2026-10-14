// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

declare module 'js-sha256' {
  interface Sha256 {
    (message: string): string;
    hmac(key: string, message: string): string;
  }
  export const sha256: Sha256;
}

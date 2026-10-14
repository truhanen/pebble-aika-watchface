// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include <pebble.h>
#include <string.h>
#include "crypto.h"

CryptoSlot Crypto_slots[MAX_CRYPTO];
uint8_t    Crypto_count = 0;

bool Crypto_isWid(uint8_t wid) {
  return wid == 15 || wid == 16 || wid == 17 ||
         (wid >= CRYPTO_WID_BASE && wid < CRYPTO_WID_BASE + MAX_CRYPTO);
}

void Crypto_parse(const char *data) { Crypto_count = DataSlots_parse(data, Crypto_slots, MAX_CRYPTO); }
CryptoSlot *Crypto_find(uint8_t wid) { return DataSlots_find(Crypto_slots, Crypto_count, wid); }

void Crypto_init() {
  Crypto_count = 0;
  if (persist_exists(CRYPTO_PERSIST_KEY_DATA)) {
    char buf[256];
    int n = persist_read_string(CRYPTO_PERSIST_KEY_DATA, buf, sizeof(buf));
    if (n > 0) { Crypto_count = DataSlots_parse(buf, Crypto_slots, MAX_CRYPTO); }
  }
#ifdef SCREENSHOT_FIXTURES
  // Demo coins for appstore screenshots (no phone): BTC, ETH, EUR/USD.
  Crypto_count = 3;
  Crypto_slots[0] = (CryptoSlot){ .wid = 15,  .valid = true };
  strcpy(Crypto_slots[0].label, "BTC"); strcpy(Crypto_slots[0].value, "62");
  Crypto_slots[1] = (CryptoSlot){ .wid = 201, .valid = true };
  strcpy(Crypto_slots[1].label, "ETH"); strcpy(Crypto_slots[1].value, "17");
  Crypto_slots[2] = (CryptoSlot){ .wid = 17,  .valid = true };
  strcpy(Crypto_slots[2].label, "EUR"); strcpy(Crypto_slots[2].value, "136");
#endif
}

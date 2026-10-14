// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include <pebble.h>
#include <string.h>
#include "currency.h"

CryptoSlot Currency_slots[MAX_CURRENCY];
uint8_t    Currency_count = 0;

bool Currency_isWid(uint8_t wid) {
  return wid >= CURRENCY_WID_BASE && wid < CURRENCY_WID_BASE + MAX_CURRENCY;
}

void Currency_parse(const char *data) { Currency_count = DataSlots_parse(data, Currency_slots, MAX_CURRENCY); }
CryptoSlot *Currency_find(uint8_t wid) { return DataSlots_find(Currency_slots, Currency_count, wid); }

void Currency_init() {
  Currency_count = 0;
  if (persist_exists(CURRENCY_PERSIST_KEY_DATA)) {
    char buf[256];
    int n = persist_read_string(CURRENCY_PERSIST_KEY_DATA, buf, sizeof(buf));
    if (n > 0) { Currency_count = DataSlots_parse(buf, Currency_slots, MAX_CURRENCY); }
  }
#ifdef SCREENSHOT_FIXTURES
  // Demo pairs for appstore screenshots (no phone): EUR/USD, USD/JPY.
  Currency_count = 2;
  Currency_slots[0] = (CryptoSlot){ .wid = 216, .valid = true };
  strcpy(Currency_slots[0].label, "EUR/USD"); strcpy(Currency_slots[0].value, "1.0823");
  Currency_slots[1] = (CryptoSlot){ .wid = 217, .valid = true };
  strcpy(Currency_slots[1].label, "USD/JPY"); strcpy(Currency_slots[1].value, "162.4");
#endif
}

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <pebble.h>
#include "crypto.h"   // reuse the CryptoSlot alias (see data_slots.h) + label/value buffers

// The whole CurrencyData wire string is persisted under this one key and
// re-parsed on boot. 313 = crypto, 314/315 = battery-days, so 316 is free.
#define CURRENCY_PERSIST_KEY_DATA 316

#define MAX_CURRENCY       7            // matches MAX_CURRENCY in src/ts/config
#define CURRENCY_WID_BASE  216          // pair wid range [216, 216+MAX_CURRENCY) = 216-222.
                                        // 223 is excluded: 223|0x20 == 0xFF, the rotating
                                        // marker (a hidden wid-223 collides); 224+ set the
                                        // 0x20 hide flag. So 7 is the safe max after crypto.

extern CryptoSlot Currency_slots[MAX_CURRENCY];
extern uint8_t    Currency_count;

// True iff `wid` is a currency-pair widget id (the 216+ range).
bool Currency_isWid(uint8_t wid);

void        Currency_init();                       // load + parse persisted string
void        Currency_parse(const char *data);      // parse wire string -> slots
CryptoSlot *Currency_find(uint8_t wid);            // slot for a wid, or NULL

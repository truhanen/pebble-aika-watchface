// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <pebble.h>
#include "data_slots.h"

// The whole CryptoData wire string (the bytes the phone sent) is persisted under
// this one key and re-parsed on boot. Pebble persist values cap at 256 bytes, so
// the stored string is truncated to 255 + NUL; realistic coin counts fit easily.
#define CRYPTO_PERSIST_KEY_DATA 313   // was the legacy BtcInfo blob; repurposed

#define MAX_CRYPTO       16           // matches MAX_CRYPTO in the TS config
#define CRYPTO_WID_BASE  200          // new-coin wid range [200, 200+MAX_CRYPTO)
#define CRYPTO_LABEL_LEN DATA_SLOT_LABEL_LEN
#define CRYPTO_VALUE_LEN DATA_SLOT_VALUE_LEN

typedef DataSlot CryptoSlot;   // shared slot struct (see data_slots.h)

extern CryptoSlot Crypto_slots[MAX_CRYPTO];
extern uint8_t    Crypto_count;

// True iff `wid` is a crypto coin widget id (legacy 15/16/17 or the 200+ range).
bool Crypto_isWid(uint8_t wid);

void        Crypto_init();                       // load + parse persisted string
void        Crypto_parse(const char *data);      // parse wire string -> slots (no persist)
CryptoSlot *Crypto_find(uint8_t wid);            // slot for a wid, or NULL

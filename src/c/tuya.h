// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <pebble.h>
#include "crypto.h"   // reuse the CryptoSlot struct + label/value buffers

// The whole TuyaData wire string is persisted under this one key and re-parsed on
// boot. Keys 4/100/200/223/300-303/310-316 are taken; 317 is free.
#define TUYA_PERSIST_KEY_DATA 317

#define MAX_TUYA       16           // matches MAX_TUYA in src/ts/config_tuya_list.ts
#define TUYA_WID_BASE  128          // sensor wid range [128, 128+MAX_TUYA) = 128-143.
                                    // bit 0x20 (hide flag) is clear across 128-143 and
                                    // none is 223/255, so no rotating-marker collision.

extern CryptoSlot Tuya_slots[MAX_TUYA];
extern uint8_t    Tuya_count;

// True iff `wid` is a Tuya sensor widget id (the 128+ range).
bool Tuya_isWid(uint8_t wid);

void        Tuya_init();                       // load + parse persisted string
void        Tuya_parse(const char *data);      // parse wire string -> slots
CryptoSlot *Tuya_find(uint8_t wid);            // slot for a wid, or NULL

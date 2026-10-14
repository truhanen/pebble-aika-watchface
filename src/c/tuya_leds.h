// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <pebble.h>
#include "tuya_leds_parse.h"

// The whole TuyaLeds wire string is persisted under this one key and re-parsed on
// boot. Keys 4/100/200/223/300-303/310-317 are taken; 318 is free.
#define TUYA_LEDS_PERSIST_KEY_DATA 318

extern uint8_t TuyaLeds_states[TUYA_LEDS_MAX];
extern uint8_t TuyaLeds_count;

void TuyaLeds_init(void);                 // load + parse the persisted string
void TuyaLeds_parse(const char *data);    // parse wire string -> states

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <stdint.h>
#include <stddef.h>

// Parser for the TuyaLeds wire string: one character per configured switch,
// '1' = on, '0' = off, anything else = unknown. Pebble-free so it host-compiles
// for tests/test_tuya_leds_parse.c.
#define TUYA_LEDS_MAX 6   // matches MAX_TUYA_LEDS in src/ts/tuya_leds.ts

typedef enum {
  TUYA_LED_OFF     = 0,
  TUYA_LED_ON      = 1,
  TUYA_LED_UNKNOWN = 2
} TuyaLedState;

// Writes up to `max` states into out[]; returns how many were written.
uint8_t TuyaLeds_parseInto(const char *s, uint8_t *out, uint8_t max);

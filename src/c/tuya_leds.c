// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include <pebble.h>
#include "tuya_leds.h"

uint8_t TuyaLeds_states[TUYA_LEDS_MAX];
uint8_t TuyaLeds_count = 0;

void TuyaLeds_parse(const char *data) {
  TuyaLeds_count = TuyaLeds_parseInto(data, TuyaLeds_states, TUYA_LEDS_MAX);
}

void TuyaLeds_init(void) {
  TuyaLeds_count = 0;
  if (persist_exists(TUYA_LEDS_PERSIST_KEY_DATA)) {
    char buf[TUYA_LEDS_MAX + 1];
    int n = persist_read_string(TUYA_LEDS_PERSIST_KEY_DATA, buf, sizeof(buf));
    if (n > 0) { TuyaLeds_parse(buf); }
  }
#ifdef SCREENSHOT_FIXTURES
  // Demo switch row for appstore/verification screenshots (no phone): on, off,
  // unknown, on, on, off.
  TuyaLeds_count = 6;
  TuyaLeds_states[0] = TUYA_LED_ON;
  TuyaLeds_states[1] = TUYA_LED_OFF;
  TuyaLeds_states[2] = TUYA_LED_UNKNOWN;
  TuyaLeds_states[3] = TUYA_LED_ON;
  TuyaLeds_states[4] = TUYA_LED_ON;
  TuyaLeds_states[5] = TUYA_LED_OFF;
#endif
}

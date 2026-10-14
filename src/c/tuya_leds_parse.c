// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include "tuya_leds_parse.h"

uint8_t TuyaLeds_parseInto(const char *s, uint8_t *out, uint8_t max) {
  if (s == NULL || out == NULL) { return 0; }
  uint8_t n = 0;
  while (s[n] != '\0' && n < max) {
    out[n] = (s[n] == '1') ? TUYA_LED_ON
           : (s[n] == '0') ? TUYA_LED_OFF
                           : TUYA_LED_UNKNOWN;
    n++;
  }
  return n;
}

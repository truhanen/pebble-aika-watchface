// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#include "tuya_leds_parse.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  uint8_t s[TUYA_LEDS_MAX];

  assert(TuyaLeds_parseInto("10?1", s, TUYA_LEDS_MAX) == 4);
  assert(s[0] == TUYA_LED_ON);
  assert(s[1] == TUYA_LED_OFF);
  assert(s[2] == TUYA_LED_UNKNOWN);
  assert(s[3] == TUYA_LED_ON);

  // unknown characters parse as unknown, not as off
  assert(TuyaLeds_parseInto("x", s, TUYA_LEDS_MAX) == 1);
  assert(s[0] == TUYA_LED_UNKNOWN);

  // truncation at max
  assert(TuyaLeds_parseInto("111111111", s, TUYA_LEDS_MAX) == TUYA_LEDS_MAX);
  assert(TuyaLeds_parseInto("111", s, 2) == 2);

  // empty / NULL
  assert(TuyaLeds_parseInto("", s, TUYA_LEDS_MAX) == 0);
  assert(TuyaLeds_parseInto(NULL, s, TUYA_LEDS_MAX) == 0);

  printf("PASS\n");
  return 0;
}

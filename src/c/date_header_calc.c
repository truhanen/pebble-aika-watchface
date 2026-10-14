// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include "date_header_calc.h"

bool DateHeader_textIsAscii(const char* s) {
  for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
    if (*p & 0x80) return false;   // any high-bit byte -> non-ASCII (UTF-8 multibyte)
  }
  return true;
}

bool DateHeader_fontIsAsciiOnly(uint8_t fontId) {
  switch (fontId) {
    case BIG_DATE_FONT_GOTHIC: return false;   // full coverage
    default:                   return true;    // Bitham, Serif, unknown -> ascii-only
  }
}

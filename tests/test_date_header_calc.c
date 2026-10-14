// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
//
// Host-gcc unit tests for the big-date font gate.
//   gcc -I src/c tests/test_date_header_calc.c src/c/date_header_calc.c -o t && ./t
//
// The big-date top font (Bitham) lacks accented/extended-Latin, Cyrillic,
// Greek, etc. glyphs, so a date name containing any non-ASCII byte must fall
// back to Gothic. DateHeader_textIsAscii() is that gate.

#include "date_header_calc.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  // Pure ASCII date names (English weekday + day.month) -> Bitham is safe.
  assert(DateHeader_textIsAscii("Mon 6.6") == true);
  assert(DateHeader_textIsAscii("Fri 31.12") == true);
  assert(DateHeader_textIsAscii("") == true);          // empty is ASCII

  // The reported bug: Polish names carry Latin Extended-A (UTF-8 two-byte,
  // high bit set) -> must NOT be treated as ASCII.
  assert(DateHeader_textIsAscii("Śro 8.6") == false);  // Ś = U+015A
  assert(DateHeader_textIsAscii("Pią 8.6") == false);  // ą = U+0105
  assert(DateHeader_textIsAscii("Paź 8.10") == false); // ź = U+017A

  // Other affected scripts (Latin-1, Cyrillic, Greek, CJK) also non-ASCII.
  assert(DateHeader_textIsAscii("Sáb 6.6") == false);  // á = Latin-1 (Spanish)
  assert(DateHeader_textIsAscii("Čt 6.6") == false);   // Č (Czech)
  assert(DateHeader_textIsAscii("Пн 6.6") == false);   // Cyrillic
  assert(DateHeader_textIsAscii("日 6.6") == false);   // CJK

  // A trailing non-ASCII byte must still be caught (loop reaches the end).
  assert(DateHeader_textIsAscii("6.6 Ś") == false);

  // Big-date font choice -> whether the font lacks extended glyphs (asciiOnly).
  assert(DateHeader_fontIsAsciiOnly(BIG_DATE_FONT_BITHAM) == true);   // display font, no ext glyphs
  assert(DateHeader_fontIsAsciiOnly(BIG_DATE_FONT_SERIF)  == true);   // Droid Serif, no ext glyphs (tofu)
  assert(DateHeader_fontIsAsciiOnly(BIG_DATE_FONT_GOTHIC) == false);  // full coverage
  assert(DateHeader_fontIsAsciiOnly(99) == true);                    // unknown -> safe (force Gothic)

  printf("test_date_header_calc: all assertions passed\n");
  return 0;
}

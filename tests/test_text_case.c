// SPDX-License-Identifier: GPL-3.0-only
// Test the text_case module with UTF-8 handling
//
// Compile and run: gcc -std=c11 -Wall -I src/c -o t_text_case tests/test_text_case.c src/c/text_case.c && ./t_text_case
#include "text_case.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Note: We don't test all language names here because languages.c depends on pebble.h.
// Instead, we'll validate the language names work in the emulator tests.

int main(void) {
  printf("--- text_initial_caps tests ---\n");

  // Test 1: Empty string
  {
    char buf[10];
    text_initial_caps("", buf, sizeof(buf));
    assert(buf[0] == '\0');
    printf("PASS: empty string\n");
  }

  // Test 2: NULL input
  {
    char buf[10];
    text_initial_caps(NULL, buf, sizeof(buf));
    assert(buf[0] == '\0');
    printf("PASS: NULL input\n");
  }

  // Test 3: ASCII A-Z
  {
    char buf[10];
    text_initial_caps("WED", buf, sizeof(buf));
    assert(strcmp(buf, "Wed") == 0);
    printf("PASS: ASCII A-Z (WED → Wed)\n");
  }

  // Test 4: ASCII single character
  {
    char buf[10];
    text_initial_caps("A", buf, sizeof(buf));
    assert(strcmp(buf, "A") == 0);
    printf("PASS: ASCII single uppercase letter\n");
  }

  // Test 5: ASCII lowercase stays lowercase
  {
    char buf[10];
    text_initial_caps("wed", buf, sizeof(buf));
    assert(strcmp(buf, "wed") == 0);
    printf("PASS: ASCII lowercase stays lowercase\n");
  }

  // Test 6: Latin-1 Supplement (U+00C0–U+00DE)
  {
    char buf[10];
    // Á (U+00C1) → á (U+00E1)
    text_initial_caps("ÁUGUST", buf, sizeof(buf));
    assert(strcmp(buf, "Áugust") == 0);
    printf("PASS: Latin-1 Supplement (ÁUGUST → Áugust)\n");
  }

  // Test 7: Latin-1 Supplement exception U+00D7 (×)
  {
    char buf[10];
    text_initial_caps("×", buf, sizeof(buf));
    assert(strcmp(buf, "×") == 0);
    printf("PASS: Latin-1 exception U+00D7 stays unchanged\n");
  }

  // Test 8: Latin Extended-A even/odd alternation
  {
    char buf[10];
    // Ā (U+0100) → ā (U+0101)
    text_initial_caps("ĀUGUST", buf, sizeof(buf));
    assert(strcmp(buf, "Āugust") == 0);
    printf("PASS: Latin Extended-A (ĀUGUST → Āugust)\n");
  }

  // Test 9: Latin Extended-A exception U+0130 (İ → ı)
  {
    char buf[10];
    // İ is U+0130 (dotted capital I), should become ı (U+0069)
    text_initial_caps("İ", buf, sizeof(buf));
    assert(strcmp(buf, "İ") == 0);  // First char keeps its case
    printf("PASS: Latin Extended-A U+0130 (İ) first char keeps case\n");
  }

  // Test 10: Latin Extended-A exception U+0178 (Ÿ → ÿ)
  {
    char buf[10];
    text_initial_caps("Ÿ", buf, sizeof(buf));
    assert(strcmp(buf, "Ÿ") == 0);  // First char keeps its case
    printf("PASS: Latin Extended-A U+0178 (Ÿ) first char keeps case\n");
  }

  // Test 11: Greek (U+0391–U+03A9)
  {
    char buf[20];
    // ΔΕΥ → Δευ (Delta Epsilon Upsilon, but lowercase epsilon and upsilon)
    // Actually the test says "ΔΕΥ" → "Δευ" which looks wrong...
    // Let me check: Δ (U+0394), Ε (U+0395), Υ (U+03A5)
    // These should become δ (U+03B4), ε (U+03B5), υ (U+03C5)
    // So "ΔΕΥ" → "Δευ" is correct if we lowercase E→ε and Υ→υ
    text_initial_caps("ΔΕΥ", buf, sizeof(buf));
    assert(strcmp(buf, "Δευ") == 0);
    printf("PASS: Greek (ΔΕΥ → Δευ)\n");
  }

  // Test 12: Greek exception U+03A2
  {
    char buf[10];
    text_initial_caps("Α", buf, sizeof(buf));  // Alpha
    assert(strcmp(buf, "Α") == 0);
    printf("PASS: Greek Alpha (Α) first char unchanged\n");
  }

  // Test 13: Cyrillic U+0410–U+042F
  {
    char buf[20];
    // ПН → Пн (Pi Nu, becomes pi nu lowercase)
    text_initial_caps("ПН", buf, sizeof(buf));
    assert(strcmp(buf, "Пн") == 0);
    printf("PASS: Cyrillic U+0410–U+042F (ПН → Пн)\n");
  }

  // Test 14: Cyrillic U+0400–U+040F
  {
    char buf[20];
    // Ё (U+0401) → ё (U+0451)
    text_initial_caps("ЁЛ", buf, sizeof(buf));
    assert(strcmp(buf, "Ёл") == 0);
    printf("PASS: Cyrillic U+0400–U+040F (ЁЛ → Ёл)\n");
  }

  // Test 15: Non-letter characters pass through
  {
    char buf[20];
    text_initial_caps("123", buf, sizeof(buf));
    assert(strcmp(buf, "123") == 0);
    printf("PASS: Non-letter characters\n");
  }

  // Test 16: Mixed string with spaces and punctuation
  {
    char buf[20];
    text_initial_caps("HELLO WORLD!", buf, sizeof(buf));
    assert(strcmp(buf, "Hello world!") == 0);
    printf("PASS: Mixed string with spaces and punctuation\n");
  }

  // Test 17: Buffer too small - truncate at character boundary
  {
    char buf[3];
    text_initial_caps("HELLO", buf, sizeof(buf));
    assert(strcmp(buf, "He") == 0);
    printf("PASS: Buffer too small\n");
  }

  // Test 18: Truncation in middle of multi-byte character must not split
  {
    char buf[2];
    // Á is 2 bytes (U+00C1 = 0xC3 0x81 in UTF-8)
    text_initial_caps("Á", buf, sizeof(buf));
    assert(buf[0] == '\0');  // Not enough room for even one 2-byte character
    printf("PASS: Truncation at character boundary (multi-byte)\n");
  }

  // Test 19: Truncation with multi-byte second character
  {
    char buf[3];
    // "AÁ" = 'A' (1 byte) + Á (2 bytes)
    // With buffer size 3, we can only fit 'A' (1 byte) + null
    text_initial_caps("AÁ", buf, sizeof(buf));
    // Should be "A" only, since Á (2 bytes) doesn't fit
    assert(strcmp(buf, "A") == 0);
    printf("PASS: Truncation with multi-byte second character\n");
  }

  // Test 20: Czech example from requirements - ČT
  {
    char buf[10];
    text_initial_caps("ČT", buf, sizeof(buf));
    assert(strcmp(buf, "Čt") == 0);
    printf("PASS: Czech (ČT → Čt)\n");
  }

  // Test 21: October example from requirements - OCT
  {
    char buf[10];
    text_initial_caps("OCT", buf, sizeof(buf));
    assert(strcmp(buf, "Oct") == 0);
    printf("PASS: October (OCT → Oct)\n");
  }

  // Test 22: French example - FÉV
  {
    char buf[10];
    text_initial_caps("FÉV", buf, sizeof(buf));
    assert(strcmp(buf, "Fév") == 0);
    printf("PASS: French February (FÉV → Fév)\n");
  }

  // Note: Language names validation (all 37 languages) is done in emulator tests

  printf("--- all text_case tests passed ---\n");
  return 0;
}

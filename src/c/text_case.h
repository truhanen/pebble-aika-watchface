#pragma once

#include <stddef.h>

// Converts the first character to uppercase (if it is a letter) and all following
// characters to lowercase. Handles UTF-8 multi-byte characters correctly and never
// splits a character. Always NUL-terminates, truncating only at character boundaries.
//
// Supports:
// - ASCII A–Z
// - Latin-1 Supplement U+00C0–U+00DE (except U+00D7)
// - Latin Extended-A U+0100–U+017F (with correct exceptions)
// - Greek U+0391–U+03A9 (except U+03A2)
// - Cyrillic U+0410–U+042F and U+0400–U+040F
//
// Parameters:
//   in: input string (may be NULL or empty)
//   out: output buffer (must be at least n bytes)
//   n: size of output buffer (must be > 0 for non-empty output)
void text_initial_caps(const char *in, char *out, size_t n);

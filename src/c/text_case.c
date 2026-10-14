// SPDX-License-Identifier: GPL-3.0-only
#include "text_case.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Decodes a single UTF-8 character starting at s, and writes its Unicode code point
// to *codepoint. Returns the byte length of the character (1, 2, 3, or 4), or 0 if
// the sequence is invalid. Does not read past the first null byte.
static int utf8_decode_char(const unsigned char *s, uint32_t *codepoint) {
  if (!s || !*s) { return 0; }

  unsigned char b0 = s[0];

  if ((b0 & 0x80) == 0) {
    // Single-byte ASCII
    *codepoint = b0;
    return 1;
  } else if ((b0 & 0xE0) == 0xC0) {
    // 2-byte sequence
    if (!s[1] || (s[1] & 0xC0) != 0x80) { return 0; }
    *codepoint = ((b0 & 0x1F) << 6) | (s[1] & 0x3F);
    return 2;
  } else if ((b0 & 0xF0) == 0xE0) {
    // 3-byte sequence
    if (!s[1] || (s[1] & 0xC0) != 0x80 || !s[2] || (s[2] & 0xC0) != 0x80) { return 0; }
    *codepoint = ((b0 & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    return 3;
  } else if ((b0 & 0xF8) == 0xF0) {
    // 4-byte sequence
    if (!s[1] || (s[1] & 0xC0) != 0x80 || !s[2] || (s[2] & 0xC0) != 0x80 ||
        !s[3] || (s[3] & 0xC0) != 0x80) { return 0; }
    *codepoint = ((b0 & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
                 ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    return 4;
  }
  return 0;  // Invalid UTF-8 start byte
}

// Encodes a Unicode code point as UTF-8, writing to out. out must have enough space
// (up to 4 bytes). Returns the number of bytes written.
static int utf8_encode_char(uint32_t codepoint, unsigned char *out) {
  if (codepoint < 0x80) {
    out[0] = (unsigned char)codepoint;
    return 1;
  } else if (codepoint < 0x800) {
    out[0] = (unsigned char)(0xC0 | (codepoint >> 6));
    out[1] = (unsigned char)(0x80 | (codepoint & 0x3F));
    return 2;
  } else if (codepoint < 0x10000) {
    out[0] = (unsigned char)(0xE0 | (codepoint >> 12));
    out[1] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
    out[2] = (unsigned char)(0x80 | (codepoint & 0x3F));
    return 3;
  } else if (codepoint < 0x110000) {
    out[0] = (unsigned char)(0xF0 | (codepoint >> 18));
    out[1] = (unsigned char)(0x80 | ((codepoint >> 12) & 0x3F));
    out[2] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
    out[3] = (unsigned char)(0x80 | (codepoint & 0x3F));
    return 4;
  }
  return 0;
}

// Lowercases a single Unicode code point. Returns the lowercased code point.
// If the character is not an uppercase letter, returns it unchanged.
static uint32_t to_lower(uint32_t cp) {
  // ASCII A–Z
  if (cp >= 0x0041 && cp <= 0x005A) {
    return cp + 0x20;
  }

  // Latin-1 Supplement: U+00C0–U+00DE (except U+00D7)
  if (cp >= 0x00C0 && cp <= 0x00DE && cp != 0x00D7) {
    return cp + 0x20;
  }

  // Latin Extended-A: U+0100–U+017F with alternating even/odd exceptions
  if (cp >= 0x0100 && cp <= 0x017F) {
    // Most uppercase letters are at even positions, lowercase at odd
    if ((cp & 1) == 0) {
      // Exceptions: 0x0130, 0x0131, 0x0138, 0x0149, 0x0178–0x017E
      if (cp == 0x0130 || cp == 0x0131 || cp == 0x0138 || cp == 0x0149 ||
          (cp >= 0x0178 && cp <= 0x017E)) {
        // These have special handling; 0x0130→0x0069, others follow pattern
        if (cp == 0x0130) return 0x0069;  // ı (lowercase dotless i)
        // For others, check if they need special handling
        if (cp == 0x0178) return 0x00FF;  // Ÿ → ÿ
        // The rest (0x0149, 0x0179, 0x017B, 0x017D) already have correct +1 pattern
      }
      return cp + 1;
    }
  }

  // Greek: U+0391–U+03A9 (except U+03A2)
  if (cp >= 0x0391 && cp <= 0x03A9 && cp != 0x03A2) {
    return cp + 0x20;
  }

  // Cyrillic: U+0410–U+042F
  if (cp >= 0x0410 && cp <= 0x042F) {
    return cp + 0x20;
  }

  // Cyrillic: U+0400–U+040F
  if (cp >= 0x0400 && cp <= 0x040F) {
    return cp + 0x50;
  }

  return cp;
}

void text_initial_caps(const char *in, char *out, size_t n) {
  if (!out || n == 0) { return; }

  if (!in || !*in) {
    out[0] = '\0';
    return;
  }

  const unsigned char *src = (const unsigned char *)in;
  unsigned char *dst = (unsigned char *)out;
  size_t pos = 0;
  bool first_char = true;

  while (pos < n - 1 && *src) {
    uint32_t codepoint;
    int char_len = utf8_decode_char(src, &codepoint);

    if (char_len <= 0) {
      // Invalid UTF-8; just copy the byte and try to resync
      dst[pos++] = *src++;
      continue;
    }

    // Process the character
    if (first_char) {
      // First character: keep original case but don't lowercase
      // Actually, the requirement says "first letter keeps its case"
      // But looking at examples: "WED" → "Wed", "OCT" → "Oct"
      // This means the first letter is uppercased and rest are lowercased.
      // But if it's already uppercase, we keep it uppercase.
      // Actually, re-reading: "the first letter keeps its case, and every following letter is lowercased"
      // So if the first letter is "W", we keep it as "W" (uppercase).
      // Let's check examples again: "WED" → "Wed", "ČT" → "Čt", "ΔΕΥ" → "Δευ", "ПН" → "Пн"
      // So the first character stays as-is, and the rest are lowercased.

      // Wait, re-reading the requirement more carefully:
      // "Display rule: the first letter keeps its case, and every following letter is lowercased."
      // This is ambiguous. But looking at the examples:
      // "WED" → "Wed" means W keeps its case (uppercase), E→e, D→d
      // "ČT" → "Čt" means Č keeps its case (uppercase), T→t
      // So it seems "keeps its case" means we don't change the first character at all.

      // Copy the first character as-is
      if (pos + char_len <= n - 1) {
        memcpy(dst + pos, src, char_len);
        pos += char_len;
      } else {
        // Not enough space for the full character
        break;
      }
      first_char = false;
    } else {
      // Following characters: lowercase them
      uint32_t lower_cp = to_lower(codepoint);
      int lower_len = utf8_encode_char(lower_cp, dst + pos);

      if (lower_len > 0 && pos + lower_len <= n - 1) {
        pos += lower_len;
      } else {
        // Not enough space for the lowercased character
        break;
      }
    }

    src += char_len;
  }

  dst[pos] = '\0';
}

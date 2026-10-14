// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#pragma once
#include <stdbool.h>
#include <stdint.h>

// True when every byte of `s` is 7-bit ASCII (< 0x80). The big-date top font
// (Bitham) is a decorative display face that lacks accented / extended-Latin,
// Cyrillic, Greek, etc. glyphs and renders them blank (e.g. Polish "Śro" ->
// "ro"). Any non-ASCII date name must therefore fall back to Gothic (full
// coverage). Pure/SDK-free so it can be host-unit-tested.
bool DateHeader_textIsAscii(const char* s);

// Big date header font choice (persisted as settings.bigDateFontId; sent from
// the phone via SettingBigDateFont). Values are frozen — append only.
typedef enum {
  BIG_DATE_FONT_BITHAM = 0,   // default: FONT_KEY_BITHAM_30_BLACK (display face, no extended glyphs)
  BIG_DATE_FONT_GOTHIC = 1,   // FONT_KEY_GOTHIC_28_BOLD (full glyph coverage)
  BIG_DATE_FONT_SERIF  = 2,   // FONT_KEY_DROID_SERIF_28_BOLD (serif; no extended glyphs)
} BigDateFontId;

// True when the chosen font lacks accented/extended-Latin, Cyrillic, Greek, etc.
// glyphs and must be skipped for a non-ASCII date name (Bitham renders them
// blank, Droid Serif renders tofu boxes). Gothic has full coverage. Unknown ids
// return true (safe: forces the Gothic fallback). Pure/SDK-free (host-testable).
bool DateHeader_fontIsAsciiOnly(uint8_t fontId);

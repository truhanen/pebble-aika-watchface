// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include "date_header.h"
#include "date_header_calc.h"
#include "settings.h"
#include "theme.h"
#include "languages.h"

// One centered line, e.g. "Ti 6.6" — weekday (title-cased) + day.month. No
// trailing period: the longest date ("Su 31.12") only fits the BITHAM_30_BLACK
// line on emery with two panels + large fonts (122px) without it.
static TextLayer* s_date_layer;
static char s_date_buffer[24];
static GRect s_frame;   // current layer frame; drives auto-scaling
#define DATE_FIT_MARGIN 4   // total px of width slack (TextLayer inset + safety);
                            // if even the smallest font overflows, the layer's
                            // trailing-ellipsis mode is the fallback.

// Gothic 36 Bold is bigger than the SDK's largest header-exposed Gothic (28) and
// has full glyph coverage. The app SDK's pebble_fonts.h does NOT define a
// FONT_KEY_GOTHIC_36_BOLD macro (it only lives in the toolchain's moddable host),
// but the resource ships in the Core Devices / PebbleOS firmware
// (resources/.../GOTHIC_36_BOLD.pbf) and fonts_get_system_font() loads it by its
// resource-id string. Verified on the emery emulator. If a watch's firmware lacks
// it, fonts_get_system_font falls back to a small default — acceptable since the
// ladder below also carries the header-defined Gothic sizes for width fitting.
#define BIG_DATE_GOTHIC_36_BOLD "RESOURCE_ID_GOTHIC_36_BOLD"

// Map the configurable big-date font choice to a system font key. Bitham (the
// default) and Serif are decorative display faces with no accented/extended
// glyphs; Gothic has full coverage. See BigDateFontId in date_header_calc.h.
static const char* big_date_top_font_key(uint8_t fontId) {
  switch (fontId) {
    case BIG_DATE_FONT_GOTHIC: return BIG_DATE_GOTHIC_36_BOLD;
    case BIG_DATE_FONT_SERIF:  return FONT_KEY_DROID_SERIF_28_BOLD;
    case BIG_DATE_FONT_BITHAM:
    default:                   return FONT_KEY_BITHAM_30_BLACK;
  }
}

// Return the largest ladder font whose rendered width fits `width`. The ladder
// top is the user-chosen font (settings.bigDateFontId); below it are the Gothic
// fallback sizes (full glyph coverage). An asciiOnly top font (Bitham/Serif) is
// skipped for a non-ASCII date name, and any too-wide font steps down — so
// extended-glyph languages (Polish, Czech, Cyrillic, ...) always render
// correctly in Gothic regardless of the choice. Falls back to the smallest if
// none fit (the layer's trailing-ellipsis then applies).
static GFont pick_date_font(const char* text, int16_t width) {
  const bool ascii = DateHeader_textIsAscii(text);
  const uint8_t fontId = settings.bigDateFontId;
  const struct { const char* key; bool asciiOnly; } ladder[] = {
    { big_date_top_font_key(fontId), DateHeader_fontIsAsciiOnly(fontId) },
    { BIG_DATE_GOTHIC_36_BOLD,  false },
    { FONT_KEY_GOTHIC_28_BOLD,  false },
    { FONT_KEY_GOTHIC_24_BOLD,  false },
    { FONT_KEY_GOTHIC_18_BOLD,  false },
  };
  const int count = sizeof(ladder) / sizeof(ladder[0]);
  GFont chosen = fonts_get_system_font(ladder[count - 1].key);
  for (int i = 0; i < count; i++) {
    if (ladder[i].asciiOnly && !ascii) continue;  // font lacks the glyphs
    GFont f = fonts_get_system_font(ladder[i].key);
    // Wide box -> no wrapping -> natural single-line width.
    GSize sz = graphics_text_layout_get_content_size(
        text, f, GRect(0, 0, 1000, 1000),
        GTextOverflowModeWordWrap, GTextAlignmentCenter);
    if (sz.w <= width - DATE_FIT_MARGIN) {
      chosen = f;
      break;
    }
  }
  return chosen;
}

bool DateHeader_isSupported(void) {
#if defined(PBL_RECT) && !defined(PBL_PLATFORM_APLITE)
  return true;
#else
  return false;
#endif
}

void DateHeader_updateTime(struct tm* timeInfo) {
  char day[8];
  strncpy(day, dayNames[settings.languageId][timeInfo->tm_wday], sizeof(day));
  day[sizeof(day) - 1] = '\0';
  // Title-case ASCII names ("TI" -> "Ti"): lowercase only A-Z after the first
  // byte. High bytes are left untouched so multi-byte UTF-8 names (Greek,
  // Cyrillic, CJK, accented Latin) are never mangled.
  for (int i = 1; day[i] != '\0'; i++) {
    if (day[i] >= 'A' && day[i] <= 'Z') {
      day[i] = day[i] - 'A' + 'a';
    }
  }
  // day-of-month (+ optional month) with no leading zeros
  if (settings.showBigDateMonth) {
    snprintf(s_date_buffer, sizeof(s_date_buffer), "%s %d.%d",
             day, timeInfo->tm_mday, timeInfo->tm_mon + 1);
  } else {
    snprintf(s_date_buffer, sizeof(s_date_buffer), "%s %d",
             day, timeInfo->tm_mday);
  }
#ifdef SCREENSHOT_FIXTURES
  // Deterministic short demo date for appstore screenshots; honours the month
  // setting so both states can be captured. Screenshot-only; compiles out.
  if (settings.showBigDateMonth) {
    snprintf(s_date_buffer, sizeof(s_date_buffer), "%s 8.6", day);
  } else {
    snprintf(s_date_buffer, sizeof(s_date_buffer), "%s 8", day);
  }
#endif
}

// Memoized pick_date_font: the ladder does up to 5
// graphics_text_layout_get_content_size() measurements against a 1000x1000 box, and the
// answer depends only on the string, the available width and the configured font.
static GFont pick_date_font_cached(const char *text, int16_t width) {
  static char cachedText[sizeof(s_date_buffer)] = { 1, 0 };  // impossible first value
  static int16_t cachedWidth = -1;
  static uint8_t cachedFontId = 0xFF;
  static GFont cachedFont;
  if (cachedWidth != width || cachedFontId != settings.bigDateFontId
      || strncmp(cachedText, text, sizeof(cachedText)) != 0) {
    cachedFont = pick_date_font(text, width);
    strncpy(cachedText, text, sizeof(cachedText) - 1);
    cachedText[sizeof(cachedText) - 1] = '\0';
    cachedWidth = width;
    cachedFontId = settings.bigDateFontId;
  }
  return cachedFont;
}

void DateHeader_redraw(void) {
  if (!s_date_layer) return;
  // Nothing to do while the header is hidden (no big date, or no room for the top strip):
  // the layer stays allocated but is excluded from the render pass, so every measurement
  // below would be work for pixels nobody draws -- on every tick. The un-hide path
  // (apply_twt_layout) calls DateHeader_redraw() right after DateHeader_setHidden(false),
  // so becoming visible still refreshes immediately.
  if (layer_get_hidden(text_layer_get_layer(s_date_layer))) return;
  text_layer_set_text_color(s_date_layer, theme.timeColor); // track color setting changes
  text_layer_set_background_color(s_date_layer, theme.dateBgColor); // track color setting changes
  text_layer_set_font(s_date_layer, pick_date_font_cached(s_date_buffer, s_frame.size.w));
  text_layer_set_text(s_date_layer, s_date_buffer);
  layer_mark_dirty(text_layer_get_layer(s_date_layer));
}

void DateHeader_setFrame(GRect frame) {
  if (!s_date_layer) return;
  s_frame = frame;
  layer_set_frame(text_layer_get_layer(s_date_layer), frame);
}

void DateHeader_initLayer(Layer* parent, GRect frame) {
  if (!DateHeader_isSupported()) return;
  s_date_layer = text_layer_create(frame);
  s_frame = frame;
  text_layer_set_background_color(s_date_layer, theme.dateBgColor); // GColorClear = inherit
  text_layer_set_text_color(s_date_layer, theme.timeColor);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_date_layer, GTextOverflowModeTrailingEllipsis);
  layer_add_child(parent, text_layer_get_layer(s_date_layer));
  layer_set_hidden(text_layer_get_layer(s_date_layer), true); // shown by main.c per setting
}

void DateHeader_setHidden(bool hidden) {
  if (s_date_layer) {
    layer_set_hidden(text_layer_get_layer(s_date_layer), hidden);
  }
}

void DateHeader_deinitLayer(void) {
  if (s_date_layer) {
    text_layer_destroy(s_date_layer);
    s_date_layer = NULL;
  }
}

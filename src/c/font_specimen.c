#include "font_specimen.h"

#ifdef TEST_HOOKS
#include <string.h>
#include "widget_fonts.h"

void FontSpecimen_draw(GContext *ctx, GRect bounds, int scene) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorBlack);

  int font = (scene - 1) % WIDGET_FONT_COUNT;
  int page = (scene - 1) / WIDGET_FONT_COUNT;
  GFont f = WidgetFonts_get((uint8_t)font);

  const char *p = SPECIMEN_CHARS;
  for (int skip = page * SPECIMEN_PER_PAGE; skip > 0 && *p; skip--) {
    p += ((uint8_t)*p >= 0xC0) ? 2 : 1;   // only ° is multi-byte
  }
  for (int i = 0; i <= SPECIMEN_PER_PAGE; i++) {
    char s[4] = { 0 };
    if (i == SPECIMEN_PER_PAGE) {
      strcpy(s, SPECIMEN_SENTINEL);
    } else if (*p) {
      int n = ((uint8_t)*p >= 0xC0) ? 2 : 1;
      memcpy(s, p, n);
      p += n;
    } else {
      continue;
    }
    GRect cell = GRect((i % SPECIMEN_COLS) * SPECIMEN_CELL_W, (i / SPECIMEN_COLS) * SPECIMEN_CELL_H,
                       SPECIMEN_CELL_W, SPECIMEN_CELL_H);
    graphics_draw_text(ctx, s, f, cell, GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  }
}

#else
void FontSpecimen_draw(GContext *ctx, GRect bounds, int scene) { (void)ctx; (void)bounds; (void)scene; }
#endif

#pragma once
// Pure widget layout maths: font run splitting and the placement of widget values,
// icons/units and edge columns. Pebble-free (stdint/stdbool only) so
// tests/test_widget_layout_calc.c can host-compile it.
#include <stdbool.h>
#include <stdint.h>
#include "widget_font_metrics.h"

#define WL_FONT_COUNT 17

// Distance between a widget's value and its icon/unit, whatever the font size.
#define WL_ICON_GAP 3
// Distance from the edge strip to the widgets drawn next to it.
#define WL_EDGE_INSET 2
// Smallest vertical gap between two widgets of an edge column.
#define WL_MIN_COLUMN_GAP 5
// Gap between the parts of a stacked fraction unit and its rule.
#define WL_FRACTION_GAP 1

// Every graphical icon (not the text units) is this high, ink included, so that the
// left and right columns line up whichever widgets they hold. It is the height of the
// vertical battery of the original Nyquist: a 20 px body plus its 3 px nub.
#define WL_ICON_H 23

typedef enum { WL_LEFT, WL_RIGHT, WL_CENTER } WlPosition;

const WidgetFontMetrics *wl_metrics(int font);

// Decode the UTF-8 code point at s (NUL-safe: returns 0 with *len = 0 at the end).
uint32_t wl_next_cp(const char *s, int *len);
// True if the font really has the glyph (space and control characters always count).
bool wl_font_has(const WidgetFontMetrics *m, uint32_t cp);

// A run of text drawn in one font: the primary one, or m->fallback.
typedef struct { int16_t start, len; bool fallback; } WlRun;
// Split text into runs by glyph coverage; returns the run count (<= max_runs, the
// rest is appended to the last run). Spaces join the preceding run.
int wl_split_runs(const WidgetFontMetrics *m, const char *text, WlRun *runs, int max_runs);

// Vertical placement of the draw box of a font so its digits' ink starts at ink_top.
int wl_box_top_for_ink(const WidgetFontMetrics *m, int ink_top);
// Draw-box top of a run so it shares the baseline of the primary font's box at
// primary_box_top.
int wl_run_box_top(const WidgetFontMetrics *primary, const WidgetFontMetrics *run_font,
                   int primary_box_top);

// Top of the ink (digit rows) of an inline, one-line unit in font `unit` so that the unit
// sits on the same baseline as a value in font `value` whose digit ink starts at
// value_ink_top: baseline = value draw-box top + base(value); unit draw-box top =
// baseline - base(unit); its ink top is that box top + digit_top(unit).
int wl_inline_unit_ink_top(const WidgetFontMetrics *value, int value_ink_top,
                           const WidgetFontMetrics *unit);

// --- vertical extent of a widget ---
// Ink may never reach the 5-minute tick strip (WL_TICK_DEPTH) or its shadow
// (WL_TICK_SHADOW), so every column lives between that clearance and the screen's far side.
#define WL_TICK_DEPTH 4
#define WL_TICK_SHADOW 2
#define WL_UNIT_FONT 0   // GOTHIC_14, the font of every inline unit (widget_fonts.h WIDGET_FONT_UNIT)

// Column bounds: first row top and one past the last row bottom. Never closer to a
// screen edge than max(strip_w + WL_EDGE_INSET, WL_TICK_DEPTH + WL_TICK_SHADOW).
void wl_column_bounds(int screen_h, int strip_w, int *top, int *bottom);

// True when the text has a glyph that hangs below the baseline (g j p q y , ; ( )).
bool wl_text_has_descender(const char *text);

// What a widget draws, as the layout needs it.
typedef struct {
  int value_font;       // font index of the value
  int value_w, ink_h;   // value advance width, digit ink height (0 = no value ink)
  int icon_w, icon_h;   // icon / unit box (0 = none)
  bool unit_inline;     // the icon box is a one-line unit on the value's baseline
  bool value_desc;      // the value text has a descender
  bool unit_desc;       // the unit text has a descender
} WlItemDims;

// Vertical extent [top, bottom) of everything the item draws (value glyphs with
// descenders, inline unit with its descender, fraction, icon), relative to the top of
// the value's digit ink (or of the icon, when the item has no value ink).
void wl_item_extent(const WlItemDims *d, int *top, int *bottom);
// Height of the extent: the height a column reserves for the item.
int wl_item_height(const WlItemDims *d);

// Y of the value's ink and of the icon / unit relative to slot_top, for an item in a
// slot slot_h high: the ink (or icon-only item) is centred, then moved only as far as
// needed to keep the whole extent inside the slot. The icon is centred on the ink; an
// inline unit shares the value's baseline.
typedef struct { int16_t ink_top, icon_top; } WlVPlace;
void wl_place_v(const WlItemDims *d, int slot_h, WlVPlace *out);

// --- edge columns ---
// Top y of each of n items of the given heights, distributed "space-between" over
// [top, bottom): the first at top, the last ending at bottom; a single item is
// centred; with no room to spare the items are min_gap apart from the top.
void wl_column_layout(const int16_t *heights, int n, int top, int bottom, int min_gap,
                      int16_t *ys);
// How many of the items (from the first) fit in `inner` px with min_gap between them.
int wl_column_visible(const int16_t *heights, int n, int inner, int min_gap);

// --- horizontal placement ---
typedef struct { int16_t value_x, icon_x; } WlHPlace;
// x of a widget's value and icon/unit boxes. Left/right: the value sits against the
// edge (strip_w + WL_EDGE_INSET from it) and the icon is next to it towards the
// centre. Centre: value and icon are centred as one group, icon to the right.
// icon_w == 0 or value_w == 0 drops the gap.
void wl_place_h(WlPosition pos, int screen_w, int strip_w, int value_w, int icon_w,
                WlHPlace *out);

// Offset of an item of height inner_h centred in outer_h (floor; negative when the
// item is the larger one).
int wl_center_offset(int outer_h, int inner_h);

// --- stacked fraction unit ("BTC/USD", "c/kWh") ---
typedef struct {
  int16_t h;          // total height of the stack
  int16_t num_base;   // baseline row of the numerator, relative to the stack top
  int16_t rule_y;     // row of the 1 px rule
  int16_t den_top;    // top of the denominator's capitals
} WlFraction;
// Split "BTC/USD" at the slash into num/den (buffers of cap bytes). Returns false when
// the label has no slash or an empty half; num then holds the whole label.
bool wl_split_fraction(const char *label, char *num, char *den, int cap);
// Layout of a fraction in the unit font m (Gothic 14).
void wl_fraction_layout(const WidgetFontMetrics *m, WlFraction *out);

// Fitting a vector icon to WL_ICON_H. The icon's points span [min, max] on an axis in
// raw units (1/8 px for precise paths); `stroke` px of line is drawn around them. The
// scale maps the raw extent onto (target_px - stroke) px, as the ratio num/den, and
// wl_icon_coord moves one raw coordinate through it to land `origin` raw units from
// the box edge (the stroke's half width, so the ink starts at the box edge).
typedef struct { int32_t num, den; } WlIconScale;
WlIconScale wl_icon_scale(int raw_extent, int raw_per_px, int stroke, int target_px);
int wl_icon_coord(int v, int v_min, int origin, WlIconScale s);
// Box width in px of an icon whose raw width is raw_w under the scale (rounded up).
int wl_icon_width_px(int raw_w, int raw_per_px, int stroke, WlIconScale s);

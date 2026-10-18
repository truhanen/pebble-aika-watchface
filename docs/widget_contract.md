# Widget contract (Phase 4 / 5)

This is shared by the watch (C, `src/c`) and the phone and config page (TypeScript, `src/ts`). Both sides must change together.

## Message keys

The three widget lists, all byte arrays, all with the same caps (64 bytes, at most 16 slots):

| Key | Position | Max bytes |
|---|---|---|
| `SettingCenterWidgetList` | centre column | 64 |
| `SettingWidgetList` | left edge | 64 |
| `SettingRightWidgetList` | right edge | 64 |

The phone-only Clay keys are `WidgetListCenter`, `WidgetList` and `WidgetListRight` (the config page lists them in that order, the centre column first). The separate top and bottom centre lists no longer exist.

Default centre column: `[date "Wkday dd" (30), Empty (0)]`, so the date sits at the top and the Empty slot below keeps the space. The edges are unchanged.

`SettingShowBatteryPct` is removed. Use the battery widget (2) in unit mode (`63 %`) or icon mode, or the icon-only battery widget (36).

## Payload format (v2)

A list is a sequence of slots. Each slot is either:

- a **plain widget**: `id, flags` (2 bytes), or
- a **rotating group**: `0xFF, count (2..6), interval_code (0..5), (id, flags) × count`.

`flags` is laid out as follows:

| Bits | Meaning |
|---|---|
| 0–4 | Font index 0..16, see the table below. 31 means "position default": 5 for the edges, 7 (Bitham large) for the centre column. |
| 5 | Identifier NONE (hidden). |
| 6 | Identifier UNIT (the widget's unit label instead of its icon). |
| 7 | Reserved, must be 0. |

Identifier mode = bits 5/6: neither = ICON, bit 6 = UNIT, bit 5 = NONE. Both bits set is invalid and is read as NONE (the sanitizer rewrites it to NONE).

Each widget declares which identifiers it has (an icon, a unit, or both). When the requested mode is unavailable the watch falls back: UNIT -> ICON -> NONE, or ICON -> UNIT -> NONE. The config page offers only the available modes, in the order ICON (`◉`), UNIT (`U`), NONE (`⊘`); a widget with no identifier at all has no button. Older stored config entries `{id, hide, font}` load as hide -> `none`, else `icon`; the stored field is now `ident: 'icon'|'unit'|'none'`.

| Widget | Icon mode | Unit mode |
|---|---|---|
| 2 battery | battery icon, value `63` | `63` + `%` |
| 22 battery days | battery icon | `d` (none while charging) |
| 7 temperature | weather icon, value `19°` | value `19` + `°C` / `°F` (`SettingUseMetric`); NONE shows the bare `19` |
| 8 forecast | forecast icon, `11/18` (low/high) | `11/18` + `°C` / `°F` |
| 13 UV | (no icon; shows the unit) | `UVI` |
| 40..44 max rain | light-rain icon, value `1.4` (mm/h) or `0.06` (in/h), `--` unknown | stacked `mm` / `h` (1 h) or `hi mm` / `3 h` (N h); inch: `in` |
| 9, 20, 24, 25 sleep | sleep icon / deep icon / bar | `h` |
| 10 steps | steps icon | `steps` |
| 21 distance | steps icon | `km` / `mi` |
| 12 heart rate | heart icon | `bpm` |
| 6 week | (no icon; shows the unit) | week word (`Wk`, localised) |
| 3 alt time | (no icon; shows the unit) | the alt-zone name |
| 35 digital time | (no icon; shows the unit) | AM / PM (12 h only) |
| 14, 18, 19 electricity | (no icon; shows the unit) | `c/kWh` fraction |
| crypto / currency / tuya | (no icon; shows the unit) | the label |
| 23 Tuya LEDs | the LED row | (none) |
| 26..34 dates, 5 seconds, 11 beats, 36..38 icon-only | no identifier switch | |

- `id` uses the whole byte. The old 0x20 hide-flag-in-id encoding is gone.
- `id` 0 (Empty) is a valid slot.
- A rotating group counts as one slot (all three lists).
- Groups are packed whole or not at all. A group with one member degrades to a plain slot, and a group with no members is dropped.
- Interval codes 0..5 mean 5 s, 10 s, 30 s, 1 min, 2 min and 5 min.

## Layout (watch)

- Every list is a column laid out top to bottom, "space-between": the first slot at the column top, the last ending at the column bottom, a single slot centred, the slack spread evenly (gaps differ by at most 1 px). With no slack the slots are 5 px apart from the top and those that do not fit are not drawn.
- Left edge: value at the left edge, icon / unit to its right. Right edge: mirrored. Centre column: value and icon / unit are centred horizontally as one group, the icon / unit to the RIGHT of the value. A rotating group works in any column.
- A slot's height is the extent of everything the widget draws: value glyphs (descenders included when the text has any of `g j p q y , ; ( )`), an inline unit with its descender (Gothic 14 metrics, `desc_bottom`), a fraction unit (its top follows the numerator: a numerator with only x-height glyphs such as `c` is 2 px shorter), an icon. Nothing is drawn outside the column bounds.
- The column bounds keep at least 7 px (the 5 px 5-minute tick plus its 2 px shadow) from the top and bottom of the screen, so ink never enters the tick strip.
- An inline unit shares the value's baseline, an icon is centred on the value's ink.
- The default font of the centre column is 7 (Bitham large).
- The automatic battery / Bluetooth-disconnect substitution only works on the edge columns.

## Font indices

The label is shown on the config page. Size details are noted with the Pebble font key.

| # | Label | Pebble font |
|---|---|---|
| 0 | 3XS Gothic | GOTHIC_14 |
| 1 | 2XS Gothic | GOTHIC_14_BOLD |
| 2 | XS Gothic | GOTHIC_18_BOLD |
| 3 | S Gothic | GOTHIC_24_BOLD |
| 4 | S Roboto | ROBOTO_CONDENSED_21 |
| 5 | M Gothic | GOTHIC_28_BOLD |
| 6 | M Leco | LECO_20_BOLD_NUMBERS |
| 7 | XL Bitham | BITHAM_30_BLACK |
| 8 | L Droid serif | DROID_SERIF_28_BOLD |
| 9 | L Leco | LECO_26_BOLD_NUMBERS_AM_PM |
| 10 | XL Leco | LECO_32_BOLD_NUMBERS |
| 11 | 2XL Bitham | BITHAM_42_BOLD |
| 12 | 2XL Leco 36 | LECO_36_BOLD_NUMBERS |
| 13 | 2XL Leco 38 | LECO_38_BOLD_NUMBERS |
| 14 | 3XL Leco | LECO_42_NUMBERS |
| 15 | 5XL Leco | LECO_60_NUMBERS_AM_PM |
| 16 | 5XL Leco bold | LECO_60_BOLD_NUMBERS_AM_PM |

## Widget ids

### Existing ids

These ids are kept, ported to the new API, and listed on the config page:

| Id | Widget | Config label |
|---|---|---|
| 0 | Empty | EMPTY |
| 2 | Battery | BATTERY percentage |
| 3 | Alternate time zone | TIME alt time zone |
| 5 | Seconds | TIME seconds |
| 6 | Week number | DATE week number |
| 7 | Current weather | WEATHER temperature |
| 8 | Today's forecast | WEATHER forecast low/high |
| 9 | Sleep | SLEEP hours |
| 10 | Steps | HEALTH steps |
| 11 | Swatch beats | TIME swatch beats |
| 12 | Heart rate | HEALTH heart rate |
| 13 | UV index | WEATHER UV index |
| 40..44 | Max rain in the next 1 / 3 / 6 / 9 / 12 h | WEATHER max rain 1 h / 3 h / 6 h / 9 h / 12 h |
| 14 | Electricity price | ELECTR price |
| 18 | Next cheap electricity | ELECTR next cheap |
| 19 | Cheapest electricity hour | ELECTR cheapest hour |
| 20 | Deep sleep | SLEEP deep hours |
| 21 | Distance | HEALTH distance |
| 22 | Battery days left | BATTERY days left |
| 23 | Tuya LEDs | MISC Tuya LEDs |
| 24 | Sleep + deep sleep | SLEEP + deep hours |
| 25 | Sleep + deep bar | SLEEP hours + deep bar |

These are dynamic ranges, unchanged:

| Ids | Source |
|---|---|
| 128..143 | Tuya |
| 200..215 | Crypto |
| 216..222 | Currency |

Other existing ids:

- **1** (BT disconnect): internal only, used by the fallback; never listed on the config page.
- **4** (old date): **retired**. Drop it from the config page; it is replaced by 26..34.
- **15/16/17**: legacy crypto ids. Treat them as crypto, but don't list them.

### New ids

| Id | Widget | Config label | Example / notes |
|---|---|---|---|
| 26 | Date `d.m.` | DATE d.m. | 4.7. |
| 27 | Date `dd.mm.` | DATE dd.mm. | 04.07. |
| 28 | Date `dd.mm` | DATE dd.mm | 04.07 |
| 29 | Date `d.m.yyyy` | DATE d.m.yyyy | 4.7.2026 |
| 30 | Date `Wkday dd` | DATE wkday dd | Fr 04 |
| 31 | Date `Wkday d.m.` | DATE wkday d.m. | Fr 4.7. |
| 32 | Date `Wkday` | DATE wkday | Fr |
| 33 | Date `dd Mon` | DATE dd mon | 04 Jul |
| 34 | Date `yyyy-mm-dd` | DATE yyyy-mm-dd | 2026-07-04 |
| 35 | Digital time | TIME digital | Follows `clock_is_24h_style()`. In 12 h mode AM/PM is drawn as a unit. The leading zero follows `SettingShowLeadingZero`. |
| 36 | Battery icon only | BATTERY icon | |
| 37 | Weather icon only | WEATHER condition icon | |
| 38 | Bluetooth icon only | MISC bluetooth icon | Shows a connected or disconnected glyph. |

- Weekday and month names are localised (`languages.c`).
- `MAX_WIDGET_TYPE` is 38. Id 39 (a separate Battery % widget) existed briefly and is gone: widget 2 in unit mode replaces it.

# Widget contract (Phase 4 / 5)

This is shared by the watch (C, `src/c`) and the phone and config page (TypeScript, `src/ts`). Both sides must change together.

## Message keys

The four widget lists, all byte arrays:

| Key | Position | Max bytes |
|---|---|---|
| `SettingWidgetList` | left edge | 64 |
| `SettingRightWidgetList` | right edge | 64 |
| `SettingTopWidgetList` | top centre | 16 |
| `SettingBottomWidgetList` | bottom centre | 16 |

The phone-only Clay keys are `WidgetList`, `WidgetListRight`, `WidgetListTop` and `WidgetListBottom`.

`SettingShowBatteryPct` is removed. Use the battery widget (icon + %) or the icon-only battery widget instead.

## Payload format (v2)

A list is a sequence of slots. Each slot is either:

- a **plain widget**: `id, flags` (2 bytes), or
- a **rotating group**: `0xFF, count (2..6), interval_code (0..5), (id, flags) × count`.

`flags` is laid out as follows:

| Bits | Meaning |
|---|---|
| 0–4 | Font index 0..16, see the table below. 31 means "position default": 5 for the edges, 7 for top and bottom. |
| 5 | Hide the identifier (icon/unit). |
| 6–7 | Reserved, must be 0. |

- `id` uses the whole byte. The old 0x20 hide-flag-in-id encoding is gone.
- `id` 0 (Empty) is a valid slot.
- The top and bottom lists hold **at most one slot**. A rotating group counts as one slot.
- Groups are packed whole or not at all. A group with one member degrades to a plain slot, and a group with no members is dropped.
- Interval codes 0..5 mean 5 s, 10 s, 30 s, 1 min, 2 min and 5 min.

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
| 7 | L Bitham | BITHAM_30_BLACK |
| 8 | L Droid serif | DROID_SERIF_28_BOLD |
| 9 | L Leco | LECO_26_BOLD_NUMBERS_AM_PM |
| 10 | XL Leco | LECO_32_BOLD_NUMBERS |
| 11 | 2XL Bitham | BITHAM_42_BOLD |
| 12 | 2XL Leco 36 | LECO_36_BOLD_NUMBERS |
| 13 | 2XL Leco 38 | LECO_38_BOLD_NUMBERS |
| 14 | 3XL Leco | LECO_42_NUMBERS |
| 15 | 4XL Leco | LECO_60_NUMBERS_AM_PM |
| 16 | 5XL Leco | LECO_60_BOLD_NUMBERS_AM_PM |

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
| 8 | Today's forecast | WEATHER forecast high/low |
| 9 | Sleep | SLEEP hours |
| 10 | Steps | HEALTH steps |
| 11 | Swatch beats | TIME swatch beats |
| 12 | Heart rate | HEALTH heart rate |
| 13 | UV index | WEATHER UV index |
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
| 39 | Battery % | BATTERY percentage with % unit | Value is the battery percentage; identifier is the `%` unit, inline with the value. Hide flag hides the unit. Turkish: value is `%NN` (percent-prefix) with no separate unit. |

- Weekday and month names are localised (`languages.c`).
- `MAX_WIDGET_TYPE` becomes 39.

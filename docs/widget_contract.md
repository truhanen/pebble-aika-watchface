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

The label is shown on the config page.

| # | Label | Pebble font |
|---|---|---|
| 0 | Gothic 3x-small (14) | GOTHIC_14 |
| 1 | Gothic 2x-small (14 bold) | GOTHIC_14_BOLD |
| 2 | Gothic x-small (18 bold) | GOTHIC_18_BOLD |
| 3 | Gothic small (24 bold) | GOTHIC_24_BOLD |
| 4 | Roboto small (21 condensed) | ROBOTO_CONDENSED_21 |
| 5 | Gothic medium (28 bold) | GOTHIC_28_BOLD |
| 6 | Leco medium (20 bold) | LECO_20_BOLD_NUMBERS |
| 7 | Bitham large (30 black) | BITHAM_30_BLACK |
| 8 | Droid serif large (28 bold) | DROID_SERIF_28_BOLD |
| 9 | Leco large (26 bold) | LECO_26_BOLD_NUMBERS_AM_PM |
| 10 | Leco x-large (32 bold) | LECO_32_BOLD_NUMBERS |
| 11 | Bitham 2x-large (42 bold) | BITHAM_42_BOLD |
| 12 | Leco 2x-large (36 bold) | LECO_36_BOLD_NUMBERS |
| 13 | Leco 2x-large (38 bold) | LECO_38_BOLD_NUMBERS |
| 14 | Leco 3x-large (42) | LECO_42_NUMBERS |
| 15 | Leco 4x-large (60) | LECO_60_NUMBERS_AM_PM |
| 16 | Leco 5x-large (60 bold) | LECO_60_BOLD_NUMBERS_AM_PM |

## Widget ids

### Existing ids

These ids are kept, ported to the new API, and listed on the config page:

| Id | Widget |
|---|---|
| 0 | Empty |
| 2 | Battery (icon + %) |
| 3 | Alternate time zone |
| 5 | Seconds |
| 6 | Week number |
| 7 | Current weather |
| 8 | Today's forecast |
| 9 | Sleep |
| 10 | Steps |
| 11 | Swatch beats |
| 12 | Heart rate |
| 13 | UV index |
| 14 | Electricity price |
| 18 | Next cheap electricity |
| 19 | Cheapest electricity hour |
| 20 | Deep sleep |
| 21 | Distance |
| 22 | Battery days left |
| 23 | Tuya LEDs |
| 24 | Sleep + deep sleep |
| 25 | Sleep + deep bar |

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

| Id | Widget | Example / notes |
|---|---|---|
| 26 | Date `d.m.` | 4.7. |
| 27 | Date `dd.mm.` | 04.07. |
| 28 | Date `dd.mm` | 04.07 |
| 29 | Date `d.m.yyyy` | 4.7.2026 |
| 30 | Date `Wkday dd` | Fr 04 |
| 31 | Date `Wkday d.m.` | Fr 4.7. |
| 32 | Date `Wkday` | Fr |
| 33 | Date `dd Mon` | 04 Jul |
| 34 | Date `yyyy-mm-dd` | 2026-07-04 |
| 35 | Digital time | Follows `clock_is_24h_style()`. In 12 h mode AM/PM is drawn as a unit. The leading zero follows `SettingShowLeadingZero`. |
| 36 | Battery icon only | |
| 37 | Weather icon only | |
| 38 | Bluetooth icon only | Shows a connected or disconnected glyph. |

- Weekday and month names are localised (`languages.c`).
- `MAX_WIDGET_TYPE` becomes 38.
